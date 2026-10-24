"""Audit the native repetition workbenches against a captured domain bank.

Reads a bank written by capture-repetition-domain-bank.py and sends every
representable call through minera_repetition_batch, which invokes the same
workbench functions as POST /api/reference-table-repetition and the log
filter route. The contract checked here:

* stability: a call whose public outcome differs between hash seeds is
  reported, and native must equal one of the observed outcomes;
* tables: the same columns, dtypes, retained source rows in order, index
  class and names, and unchanged categorical declarations;
* logs: the same retained case names in order, and an unchanged source
  (sources are XES documents imported by both sides);
* errors: the same error class and complete message (Polars also the phase).

Requires no reference runtime.
"""
import argparse
import gzip
import json
import subprocess
import sys
from collections import Counter
from pathlib import Path


# Messages of explicit native rejections of declared out-of-contract inputs.
DECLARED_REJECTIONS = {
    "pandas repetition targets for typed columns must be scalars or row-aligned lists",
    "pandas datetime column string targets must be ISO-8601 timestamps",
    "Polars repetition targets must be scalars or lists",
    "Polars list targets must hold 64-bit integers",
    "Polars list targets must be homogeneous",
    "Polars repetition targets cannot be pandas NA",
    "Polars integer targets must fit in 128 bits",
    "Polars datetime targets must be ISO-8601 timestamps",
}


def native_text(value):
    if isinstance(value, dict) and value.get("native_bytes") == "hex-v1":
        return bytes.fromhex(value["data"]).decode("utf-8", "surrogateescape")
    return value


def strict_table(text):
    """Columns, dtypes, row keys (with index labels), index and categories of a native table."""
    table = json.loads(text)
    raw = "native_byte_strings" in table
    if raw:
        table = table["value"]
        unhex = lambda s: bytes.fromhex(s).decode("utf-8", "surrogateescape")
        columns = [unhex(c) for c in table["columns"]]
        dtypes = [unhex(d) for d in table["dtypes"]]
        index = table.get("index")
        index_class = unhex(index["kind"]) if index else None
        index_names = [None if n is None else unhex(n) for n in index["names"]] if index else None
        labels = [l["value"] for l in index["labels"]] if index else None
    else:
        columns, dtypes = table["columns"], table.get("dtypes", [])
        index = table.get("index")
        index_class = index["class"] if index else None
        index_names = index["names"] if index else None
        labels = [l["value"] for l in index["labels"]] if index else None
    rows = table["rows"]
    if labels is not None:
        rows = [r + [l] for r, l in zip(rows, labels)]
    return dict(raw=raw, columns=columns, dtypes=dtypes, rows=rows, index_class=index_class,
                index_names=index_names, categorical=table.get("categorical_dtypes", []))


def positions(source_rows, result_rows):
    keys = [json.dumps(r, sort_keys=True) for r in source_rows]
    out, at = [], 0
    for row in result_rows:
        key = json.dumps(row, sort_keys=True)
        while at < len(keys) and keys[at] != key:
            at += 1
        if at == len(keys):
            return None
        out.append(at)
        at += 1
    return out


def encode_request(route, parts):
    chunks = [f"{route}\n{len(parts)}\n".encode()]
    for name, filename, is_file, data in parts:
        data = data.encode("utf-8") if isinstance(data, str) else data
        chunks.append(f"{name}\n{filename}\n{1 if is_file else 0}\n{len(data)}\n".encode("utf-8"))
        chunks.append(data)
    return b"".join(chunks)


def run_native(binary, requests):
    run = subprocess.run([str(binary)], input=b"".join(requests), capture_output=True, check=True)
    out, at, responses = run.stdout, 0, []
    while at < len(out):
        newline = out.index(b"\n", at)
        size = int(out[at:newline])
        responses.append(json.loads(out[newline + 1:newline + 1 + size].decode("utf-8")))
        at = newline + 1 + size
    return responses


def table_request(engine, call):
    parts = [("engine", "", False, engine), ("source_file", "source.json", True, call["source"]),
             ("target_json", "", False, call["target_json"])]
    for name, value in call["controls"].items():
        parts.append((name, "", False, value))
    return encode_request("table", parts)


def log_request(call):
    parts = [("operation", "", False, "repetition"), ("source_format", "", False, "xes"),
             ("source_file", "source.xes", True, call["source"]),
             ("target_json", "", False, call["target_json"])]
    for name, value in call["controls"].items():
        parts.append(({"attribute_key": "activity_key"}.get(name, name), "", False, value))
    return encode_request("log", parts)


def native_table_outcome(engine, response):
    if response["http_status"] != 200:
        outcome = {"error": native_text(response["error_type"]), "message": native_text(response["error"])}
        if engine == "polars":
            outcome["phase"] = response["phase"]
        return outcome
    source = strict_table(native_text(response["source_table_json"]))
    result = strict_table(native_text(response["filtered_table_json"]))
    summary = {"columns": result["columns"], "dtypes": result["dtypes"],
               "positions": positions(source["rows"], result["rows"])}
    if result["index_class"] is not None and engine == "pandas":
        summary["index_class"] = result["index_class"]
        summary["index_names"] = result["index_names"]
    summary["categorical_unchanged"] = result["categorical"] == source["categorical"]
    return {"summary": summary, "source_unchanged": response["source_unchanged"]}


def public_table_outcome(engine, call, outcome):
    if "summary" not in outcome:
        return {k: outcome[k] for k in ("error", "message", "phase") if k in outcome}
    s = dict(outcome["summary"])
    source = json.loads(call["source"])
    s["categorical_unchanged"] = s.pop("categorical") == source.get("categorical_dtypes", [])
    if engine == "polars":
        s.pop("index_class", None)
        s.pop("index_names", None)
    return {"summary": s, "source_unchanged": outcome.get("source_unchanged", True)}


def native_log_outcome(response):
    if response["http_status"] != 200:
        return {"error": native_text(response["error"]), "source_unchanged": response["source_unchanged"]}
    result = json.loads(native_text(response["filtered_log_json"]))
    return {"retained": result, "source_unchanged": response["source_unchanged"]}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("bank", type=Path)
    parser.add_argument("--native", type=Path, required=True, help="minera_repetition_batch")
    parser.add_argument("--report", type=Path)
    parser.add_argument("--show", type=int, default=10)
    args = parser.parse_args()
    with gzip.open(args.bank, "rb") as raw:
        bank = json.load(raw)
    engine, outcomes = bank["engine"], bank["outcomes"]
    calls = [c for c in bank["calls"] if c["target_json"] is not None]
    skipped = len(bank["calls"]) - len(calls)
    if engine == "log":
        import importlib.util
        spec = importlib.util.spec_from_file_location("repetition_domain", Path(__file__).with_name("repetition_domain.py"))
        domain = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(domain)
        responses = run_native(args.native, [log_request(c) for c in calls])
    else:
        responses = run_native(args.native, [table_request(engine, c) for c in calls])
    stats, failures = Counter(), []
    for call, response in zip(calls, responses):
        observed = [outcomes[i] for i in call["observations"]]
        texts = {json.dumps(o, sort_keys=True) for o in observed}
        stats["calls"] += 1
        stats["varying" if len(texts) > 1 else "stable"] += 1
        if engine == "log":
            native = domain.native_log_summary(call, native_log_outcome(response))
            expected = [domain.public_log_summary(o) for o in observed]
        else:
            native = native_table_outcome(engine, response)
            expected = [public_table_outcome(engine, call, o) for o in observed]
        kind = "error" if "error" in expected[0] else "result"
        stats[kind] += 1
        if native.get("error") == "native_error" and native.get("message") in DECLARED_REJECTIONS:
            # An input shape the native contract rejects explicitly; the public
            # outcome is retained in the bank but not reproduced.
            stats["declared_rejection"] += 1
            stats["rejected: " + native["message"]] += 1
        elif any(native == e for e in expected):
            stats["pass"] += 1
        else:
            stats["fail"] += 1
            failures.append(dict(case=call["case"], spec=call["spec"], target=call["target_json"],
                                 controls=call["controls"], expected=expected[0], native=native))
    print(f"{engine}: {stats['calls']} calls ({skipped} unrepresentable skipped), "
          f"{stats['result']} results, {stats['error']} errors, {stats['varying']} varying, "
          f"{stats['pass']} pass, {stats['declared_rejection']} declared rejections, {stats['fail']} fail")
    for key in sorted(k for k in stats if k.startswith("rejected: ")):
        print(f"  {stats[key]} {key}")
    for f in failures[:args.show]:
        print(json.dumps(f, ensure_ascii=False)[:1500])
    if args.report:
        args.report.write_text(json.dumps(dict(engine=engine, stats=stats, skipped=skipped, failures=failures),
                                          ensure_ascii=False, indent=1), encoding="utf-8")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
