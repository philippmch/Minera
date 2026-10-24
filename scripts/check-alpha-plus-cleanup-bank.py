"""Audit native Alpha+ cleanup selection against a captured public bank.

Reads a bank from capture-alpha-plus-cleanup-bank.py and, per case and trace
order, runs minera_export --alpha-plus-alternatives and --alpha-plus-result.
The contract checked here:

* soundness: every public outcome of every process is one of the native
  enumerated alternatives (failures compare by presence, not class/message);
* exactness: where the enumeration has a single outcome, the public outcome
  is constant across processes and equals it;
* determinism: repeated native results are identical and equal the first
  enumerated alternative, which is itself a member of the support.

Public outcomes that vary across processes are reported, never bound.
Requires no reference runtime.
"""
import argparse
import gzip
import hashlib
import importlib.util
import json
import subprocess
import sys
import tempfile
from collections import Counter
from pathlib import Path
from xml.sax.saxutils import quoteattr

HERE = Path(__file__).resolve().parent


def _shared():
    spec = importlib.util.spec_from_file_location(
        "alpha_plus_capture", HERE / "capture-alpha-plus-reference.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def outcome_key(value):
    return "error" if "error" in value else json.dumps(value, sort_keys=True)


def has_name_collision(value):
    if "error" in value:
        return False
    names = [json.dumps(p["name"]) for p in value["places"]]
    return len(names) != len(set(names))


def from_support_fixture(fixture):
    """Read a merged support fixture (docs/alpha-plus-reference-fixtures.json)
    as a bank with one pseudo-process holding each row's observed support."""
    outcomes, index, rows = [], {}, []
    for case, row in enumerate(fixture["rows"]):
        ids = []
        for value in row.get("reference_support", [row["result"]]):
            text = json.dumps(value, sort_keys=True)
            if text not in index:
                index[text] = len(outcomes)
                outcomes.append(value)
            ids.append(index[text])
        rows.append(dict(case=case, traces=row["traces"], remove_unconnected=row["remove_unconnected"],
                         observations=[dict(hash_seed=None, trace_order="original", outcomes=ids)]))
    return dict(reference=fixture["reference"], python=None, hash_seeds=fixture.get("captures"),
                trace_orders=["original"], calls_per_cell=None, outcomes=outcomes, rows=rows)


def run_native(native, traces, cleanup, flag):
    xml = '<log>' + ''.join('<trace>' + ''.join(
        '<event><string key="concept:name" value=' + quoteattr(label) + '/></event>'
        for label in word) + '</trace>' for word in traces) + '</log>'
    with tempfile.NamedTemporaryFile(mode="w", suffix=".xes", encoding="utf-8", delete=False) as source:
        source.write(xml)
        filename = source.name
    try:
        output = subprocess.run([str(native.resolve()), filename, flag, "--alpha-remove-unconnected",
                                 str(cleanup).lower()], capture_output=True, text=True,
                                encoding="utf-8", timeout=60)
    finally:
        Path(filename).unlink()
    if output.returncode not in (0, 1) or "Usage:" in output.stderr:
        raise RuntimeError(f"native audit executable failed: {output.stderr[:400]}")
    return json.loads(output.stdout) if output.returncode == 0 else None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("bank", type=Path)
    parser.add_argument("--native", required=True, type=Path)
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    shared = _shared()
    raw = args.bank.read_bytes()
    if raw[:2] == b"\x1f\x8b":
        raw = gzip.decompress(raw)
    bank = json.loads(raw)
    if "outcomes" not in bank:
        bank = from_support_fixture(bank)
    assert bank["reference"] == "2.7.22"
    outcomes = bank["outcomes"]
    failures, stats = [], Counter()
    for row in bank["rows"]:
        for order in bank["trace_orders"]:
            traces = list(reversed(row["traces"])) if order == "reversed" else row["traces"]
            observed = [outcomes[i] for o in row["observations"] if o["trace_order"] == order
                        for i in o["outcomes"]]
            cells = [{outcome_key(outcomes[i]) for i in o["outcomes"]}
                     for o in row["observations"] if o["trace_order"] == order]
            where = f"case {row['case']} cleanup={row['remove_unconnected']} order={order}"
            stats["cells"] += 1
            stats["public_calls"] += len(observed)
            if bank["calls_per_cell"] and any(len(cell) > 1 for cell in cells):
                stats["public_within_process_varying"] += 1
            public = {outcome_key(value) for value in observed}
            stats["public_varying" if len(public) > 1 else "public_constant"] += 1
            if any(has_name_collision(value) for value in observed):
                stats["public_name_collision_cells"] += 1

            listing = run_native(args.native, traces, row["remove_unconnected"], "--alpha-plus-alternatives")
            if listing is None:
                # A failure before any tied choice: the public must fail too.
                stats["native_pre_choice_failures"] += 1
                if public != {"error"}:
                    failures.append(f"{where}: native fails before cleanup, public {len(public)} outcomes")
                continue
            assert listing["family"] == "ALPHA_PLUS_CLEANUP_ALTERNATIVES"
            support = []
            for alternative in listing["alternatives"]:
                support.append("error" if alternative["result"] is None
                               else outcome_key(shared.summarize_native(alternative["result"])))
            distinct = set(support)
            stats["native_alternatives"] += len(support)
            stats["native_support_singleton" if len(distinct) == 1 else "native_support_multiple"] += 1
            missing = public - distinct
            if missing:
                failures.append(f"{where}: {len(missing)} public outcomes outside native enumeration")
            stats["enumerated_outcomes"] += len(distinct)
            stats["enumerated_outcomes_observed"] += len(distinct & public)
            if len(distinct) == 1 and public != distinct:
                failures.append(f"{where}: unique native outcome differs from public")

            picks = set()
            for _ in range(args.repeats):
                value = run_native(args.native, traces, row["remove_unconnected"], "--alpha-plus-result")
                picks.add("error" if value is None else outcome_key(shared.summarize_native(value)))
            if len(picks) != 1:
                failures.append(f"{where}: native deterministic result varies")
            elif next(iter(picks)) != support[0]:
                failures.append(f"{where}: native result is not the first enumerated alternative")
            pick = next(iter(picks))
            if len(public) == 1:
                stats["constant_public_native_equal" if pick in public else "constant_public_native_other"] += 1
            else:
                stats["varying_public_native_observed" if pick in public else "varying_public_native_unobserved"] += 1
    report = dict(bank=str(args.bank), bank_sha256=hashlib.sha256(raw).hexdigest(),
                  reference=bank["reference"], python=bank["python"], hash_seeds=bank["hash_seeds"],
                  trace_orders=bank["trace_orders"], calls_per_cell=bank["calls_per_cell"],
                  stats=dict(sorted(stats.items())), failures=failures)
    text = json.dumps(report, indent=2) + "\n"
    if args.report:
        args.report.write_text(text, encoding="utf-8")
    print(text)
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
