"""Check repeated native draws against captured public Alpha+ output support.

This is a finite support check, not a proof of distribution equivalence.
"""
import argparse
import importlib.util
import json
import subprocess
import tempfile
from pathlib import Path
from xml.sax.saxutils import quoteattr


def _native_output(process, capture):
    """Decode one native draw while keeping failure semantics explicit.

    A return code of one is the native discovery failure channel.  A successful
    process must emit a valid raw Alpha+ object; malformed JSON or schema is a
    failed draw and never enters the sampled support set.
    """
    if process.returncode not in (0, 1) or "Usage:" in process.stderr:
        raise RuntimeError("Native executable failed before discovery")
    if process.returncode == 1:
        if process.stdout.strip():
            raise ValueError("native failure emitted unexpected output")
        return {"error": True}, False
    try:
        value = capture.summarize_native(json.loads(process.stdout))
    except (ValueError, TypeError, KeyError, IndexError) as error:
        raise ValueError(f"malformed native Alpha+ output: {error}") from None
    return value, True


def _record_observation(observed, value, support):
    identity = json.dumps(value, sort_keys=True)
    entry = observed.setdefault(identity, dict(result=value, count=0, supported=any(
        ("error" in value and "error" in expected) or value == expected
        for expected in support.values())))
    entry["count"] += 1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--reference", type=Path, action="append", required=True)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--samples", type=int, default=20)
    parser.add_argument("--variable-only", action="store_true", help="Limit to cases with observed public variability")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.samples < 1:
        parser.error("--samples must be positive")
    spec = importlib.util.spec_from_file_location("alpha_capture", Path(__file__).with_name("capture-alpha-plus-reference.py"))
    capture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(capture)
    cases = {}
    reference_commits = set()
    key = lambda value: json.dumps(value, sort_keys=True)
    for path in args.reference:
        document = json.loads(path.read_text(encoding="utf-8"))
        if document["reference"] != "2.7.22":
            raise ValueError("unexpected reference version")
        if document.get("reference_commit") != capture.REFERENCE_COMMIT:
            raise ValueError("missing or unexpected reference commit")
        reference_commits.add(document["reference_commit"])
        for row in document["rows"]:
            identity = key([row["traces"], row["remove_unconnected"]])
            case = cases.setdefault(identity, dict(traces=row["traces"], cleanup=row["remove_unconnected"], support={}))
            for observed in row.get("reference_support", [row["result"]]):
                case["support"][key(observed)] = observed
    rows = []
    malformed_draws = 0
    with tempfile.TemporaryDirectory(prefix="minera-alpha-support-") as directory:
        source = Path(directory) / "log.xes"
        for case in cases.values():
            if args.variable_only and len(case["support"]) < 2:
                continue
            source.write_text('<log>' + ''.join('<trace>' + ''.join(
                '<event><string key="concept:name" value=' + quoteattr(label) + '/></event>'
                for label in trace) + '</trace>' for trace in case["traces"]) + '</log>', encoding="utf-8")
            observed = {}
            for _ in range(args.samples):
                result = subprocess.run([str(args.native.resolve()), str(source), "--alpha-plus-result",
                                         "--alpha-remove-unconnected", str(case["cleanup"]).lower()],
                                        capture_output=True, text=True, encoding="utf-8", timeout=30,
                                        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
                try:
                    value, _ = _native_output(result, capture)
                except ValueError as error:
                    malformed_draws += 1
                    value = {"error": "malformed_native_output", "message": str(error)}
                _record_observation(observed, value, case["support"])
            rows.append(dict(traces=case["traces"], cleanup=case["cleanup"],
                             reference_support_size=len(case["support"]), native=list(observed.values())))
    unsupported = sum(item["count"] for row in rows for item in row["native"] if not item["supported"])
    args.output.write_text(json.dumps(dict(reference="2.7.22",
                                          reference_commit=next(iter(reference_commits), capture.REFERENCE_COMMIT),
                                          samples=args.samples, cases=len(rows), unsupported_draws=unsupported,
                                          malformed_draws=malformed_draws, rows=rows), indent=2) + "\n", encoding="utf-8")
    print(f"{len(rows)} cases, {len(rows) * args.samples} native draws, {unsupported} outside sampled reference support")
    if malformed_draws:
        print(f"{malformed_draws} draws emitted malformed native Alpha+ output")
    return 1 if unsupported or malformed_draws else 0


if __name__ == "__main__":
    raise SystemExit(main())
