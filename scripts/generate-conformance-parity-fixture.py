"""Generate the C++ fixture for the conformance parity contract tests.

Reads the merged reference bank written by merge-conformance-parity-reference.py
and emits src/backend/tests/conformance_parity_reference_cases.inc. Each case
gets its own builder function so that MSVC never sees one huge initializer.

Only observations that the contract binds are emitted:
  * Petri enum variants: the admissible (cost, bwc, fitness) tuples per trace.
  * Petri generator variants: the cost and the union of reference alignments
    (flagged when a run hit the capture's enumeration limit).
  * DFG classic: the stable alignment, cost and visited states per trace.
  * Token replay (forward, backwards): admissible rows per trace, or the error.
"""
import argparse
import gzip
import hashlib
import json
from pathlib import Path

ENUM_VARIANTS = {
    "VERSION_STATE_EQUATION_A_STAR": "kStateEquationAStar",
    "VERSION_DIJKSTRA_NO_HEURISTICS": "kDijkstraNoHeuristics",
    "VERSION_DIJKSTRA_LESS_MEMORY": "kDijkstraLessMemory",
    "VERSION_DISCOUNTED_A_STAR": "kDiscountedAStar",
}
GENERATOR_VARIANTS = {
    "GENERATOR_DIJKSTRA_LESS_MEMORY": "kGeneratorDijkstraLessMemory",
    "GENERATOR_DIJKSTRA_NO_HEURISTICS": "kGeneratorDijkstraNoHeuristics",
}
# The capture stops generator enumeration after this many alignments.
GENERATOR_LIMIT = 64
REPLAY_FAMILIES = {
    "replay:TOKEN_REPLAY": ("kForward", False),
    "replay:BACKWARDS": ("kBackwards", False),
    "replay:BACKWARDS:nonempty": ("kBackwards", True),
}


def s(value):
    return json.dumps(value)


def num(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise TypeError(value)
    return repr(float(value))


def strings(values):
    return "{" + ",".join(s(v) for v in values) + "}"


def moves(alignment):
    return "{" + ",".join(
        "{" + s(log) + "," + ("std::nullopt" if model is None else s(model)) + "}"
        for log, model in alignment) + "}"


def values(entry):
    return [entry["stable"]] if "stable" in entry else entry["unstable"]


def replay_row(row):
    reached = row.get("reached_marking")
    reached_code = "std::nullopt" if reached is None else (
        "std::vector<std::pair<std::string,long long>>{" +
        ",".join("{" + s(p) + "," + str(n) + "}" for p, n in reached) + "}")
    return ("ReplayExpectation{" + ("true" if row["trace_is_fit"] else "false") + "," +
            num(row["trace_fitness"]) + "," + strings(row["activated_transitions"]) + "," +
            strings(row["transitions_with_problems"]) + "," +
            strings(row["enabled_transitions_in_marking"]) + "," + reached_code + "," +
            ",".join(str(row[k]) for k in ("missing_tokens", "consumed_tokens",
                                           "remaining_tokens", "produced_tokens")) + "}")


def case_function(index, case):
    out = [f"MINERA_CONFORMANCE_PARITY_NOINLINE ConformanceParityCase conformance_parity_case_{index}() {{",
           "  ConformanceParityCase c;",
           f"  c.name = {s(case['name'])};",
           f"  c.tree = {s(case['tree'])};",
           "  c.traces = {" + ",".join(strings(t) for t in case["traces"]) + "};"]
    net = case["net"]
    out.append("  c.places = " + strings(net["places"]) + ";")
    out.append("  c.transitions = {" + ",".join(
        "{" + s(t) + "," + ("std::nullopt" if label is None else s(label)) + "}"
        for t, label in net["transitions"]) + "};")
    out.append("  c.arcs = {" + ",".join("{" + s(a) + "," + s(b) + "}" for a, b in net["arcs"]) + "};")
    out.append("  c.initial = {" + ",".join("{" + s(p) + "," + str(n) + "}" for p, n in net["initial"]) + "};")
    out.append("  c.final = {" + ",".join("{" + s(p) + "," + str(n) + "}" for p, n in net["final"]) + "};")
    dfg = case["dfg"]
    out.append("  c.dfg_edges = {" + ",".join("{" + s(a) + "," + s(b) + "," + str(n) + "}" for a, b, n in dfg["edges"]) + "};")
    out.append("  c.dfg_starts = {" + ",".join("{" + s(a) + "," + str(n) + "}" for a, n in dfg["starts"]) + "};")
    out.append("  c.dfg_ends = {" + ",".join("{" + s(a) + "," + str(n) + "}" for a, n in dfg["ends"]) + "};")
    obs = case["observations"]
    for family, variant in ENUM_VARIANTS.items():
        for trace, entry in enumerate(obs["petri:" + family]):
            tuples = []
            for value in values(entry):
                if "result" not in value:
                    raise ValueError(f"{case['name']} {family} {trace}: reference did not finish")
                r = value["result"]
                item = "{" + ",".join(num(r[k]) for k in ("cost", "bwc", "fitness")) + "}"
                if item not in tuples:
                    tuples.append(item)
            out.append(f"  c.petri.push_back({{AlignmentVariant::{variant},{trace},{{{','.join(tuples)}}}}});")
    for family, variant in GENERATOR_VARIANTS.items():
        for trace, entry in enumerate(obs["petri:" + family]):
            union, costs, truncated = [], set(), False
            runs = values(entry)
            if any("result" not in value for value in runs):
                # The capture's per-trace time limit expired; nothing to bind.
                out.append(f"  // {family} trace {trace}: reference timed out")
                continue
            for value in runs:
                truncated |= len(value["result"]) >= GENERATOR_LIMIT
                for alignment in value["result"]:
                    costs.add(alignment["cost"])
                    if alignment["alignment"] not in union:
                        union.append(alignment["alignment"])
            if len(costs) != 1:
                raise ValueError(f"{case['name']} {family} {trace}: generator costs differ")
            out.append(f"  c.generators.push_back({{AlignmentVariant::{variant},{trace},{num(costs.pop())},"
                       f"{str(truncated).lower()},"
                       "{" + ",".join(moves(a) for a in union) + "}});")
    for trace, entry in enumerate(obs["dfg:CLASSIC"]):
        if "stable" not in entry:
            raise ValueError(f"{case['name']} dfg {trace}: unstable")
        r = entry["stable"]["result"]
        if r is None:
            out.append(f"  c.dfg.push_back({{{trace},false,{{}},0.0,0}});")
        else:
            out.append(f"  c.dfg.push_back({{{trace},true,{moves(r['alignment'])},{num(r['cost'])},{r['visited_states']}}});")
    for family, (variant, nonempty) in REPLAY_FAMILIES.items():
        entry = obs[family]
        runs = values(entry)
        if not all("result" in v for v in runs):
            if any("result" in v for v in runs):
                raise ValueError(f"{case['name']} {family}: mixed error and result")
            out.append(f"  c.replay.push_back({{TokenReplayVariant::{variant},{str(nonempty).lower()},true,{{}}}});")
            continue
        rows = []
        for k in range(len(runs[0]["result"])):
            admissible = []
            for v in runs:
                code = replay_row(v["result"][k])
                if code not in admissible:
                    admissible.append(code)
            rows.append("{" + ",".join(admissible) + "}")
        out.append(f"  c.replay.push_back({{TokenReplayVariant::{variant},{str(nonempty).lower()},false,{{{','.join(rows)}}}}});")
    out.append("  return c;")
    out.append("}")
    return "\n".join(out)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("bank", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    raw = args.bank.read_bytes()
    bank = json.loads(gzip.decompress(raw))
    lines = [
        "// Generated by scripts/generate-conformance-parity-fixture.py; do not edit.",
        f"// Reference bank {args.bank.name} SHA256 {hashlib.sha256(raw).hexdigest()}",
        f"// Reference {bank['reference_version']} at {bank['reference_commit']}, corpus seed {bank['seed']},",
        f"// {len(bank['runs'])} merged capture runs.",
        "#if defined(_MSC_VER)",
        "#define MINERA_CONFORMANCE_PARITY_NOINLINE __declspec(noinline)",
        "#else",
        "#define MINERA_CONFORMANCE_PARITY_NOINLINE __attribute__((noinline))",
        "#endif",
    ]
    for index, case in enumerate(bank["cases"]):
        lines.append(case_function(index, case))
    lines.append("inline std::vector<ConformanceParityCase> conformance_parity_cases() {")
    lines.append("  std::vector<ConformanceParityCase> cases;")
    for index in range(len(bank["cases"])):
        lines.append(f"  cases.push_back(conformance_parity_case_{index}());")
    lines.append("  return cases;")
    lines.append("}")
    lines.append("#undef MINERA_CONFORMANCE_PARITY_NOINLINE")
    args.output.write_text("\n".join(lines) + "\n", newline="\n")


if __name__ == "__main__":
    main()
