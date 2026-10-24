"""Generate the C++ fixture for the conformance parity contract tests.

Reads merged reference banks written by merge-conformance-parity-reference.py
(the process-tree corpus and the frozen net corpus) and emits
src/backend/tests/conformance_parity_reference_cases.inc. Each case
gets its own builder function so that MSVC never sees one huge initializer.

Only observations that the contract binds are emitted:
  * Petri enum variants, with default or supplied cost functions: the
    admissible (cost, bwc, fitness) tuples per trace, or no alignment, or an
    error.
  * Petri generator variants: the cost and the union of reference alignments
    (flagged when a run hit the capture's enumeration limit), or none.
  * DFG classic: the stable alignment, cost and visited states per trace.
  * Token replay (forward, backwards, each public option toggled): admissible
    rows per trace, or the error.
Calls that hit the capture's time limit bind nothing and are skipped.
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
# Both maps follow AlignmentVariant order; the fixture's row counts rely on it.
GENERATOR_VARIANTS = {
    "GENERATOR_DIJKSTRA_NO_HEURISTICS": "kGeneratorDijkstraNoHeuristics",
    "GENERATOR_DIJKSTRA_LESS_MEMORY": "kGeneratorDijkstraLessMemory",
}
# The capture stops generator enumeration after this many alignments.
GENERATOR_LIMIT = 64
# Replay options the native options struct can express. The reference's
# consider_activities_not_in_model_in_fitness marks traces unfit based on other
# traces of the log; it is a documented open divergence and is not bound.
REPLAY_OPTIONS = {
    "consider_remaining_in_fitness=False": "consider_remaining_in_fitness = false",
    "try_to_reach_final_marking_through_hidden=False": "try_to_reach_final_marking_through_hidden = false",
    "walk_through_hidden_trans=False": "walk_through_hidden_transitions = false",
    "stop_immediately_unfit=True": "stop_immediately_unfit = true",
    "cleaning_token_flood=True": "cleaning_token_flood = true",
    "exhaustive_invisible_exploration=True": "exhaustive_invisible_exploration = true",
}


def replay_family(family):
    """(variant, nonempty traces only, option key) or None when not bound."""
    parts = family.split(":", 2)
    variant = {"TOKEN_REPLAY": "kForward", "BACKWARDS": "kBackwards"}[parts[1]]
    if len(parts) == 2:
        return variant, False, ""
    if parts[2] == "nonempty":
        return variant, True, ""
    if parts[2] not in REPLAY_OPTIONS:
        return None
    # Backwards option families replay the nonempty traces.
    return variant, variant == "kBackwards", parts[2]


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


COUNTS = dict.fromkeys(["kDfgRows", "kForwardReplayRows", "kForwardReplayErrors",
                        "kBackwardsReplayRows", "kBackwardsReplayErrors"], 0)
# Rows per enum variant (AlignmentVariant order) and Outcome, and per generator.
PETRI_ROWS = {v: {"kAlignment": 0, "kNone": 0, "kError": 0} for v in ENUM_VARIANTS.values()}
GENERATOR_ROWS = {v: [0, 0] for v in GENERATOR_VARIANTS.values()}  # rows, empty rows


def case_function(index, case):
    out = [f"MINERA_CONFORMANCE_PARITY_NOINLINE ConformanceParityCase conformance_parity_case_{index}() {{",
           "  ConformanceParityCase c;",
           f"  c.name = {s(case['name'])};",
           f"  c.tree = {s(case['tree'] or '')};",
           "  c.traces = {" + ",".join(strings(t) for t in case["traces"]) + "};"]
    net = case["net"]
    out.append("  c.places = " + strings(net["places"]) + ";")
    out.append("  c.transitions = {" + ",".join(
        "{" + s(t) + "," + ("std::nullopt" if label is None else s(label)) + "}"
        for t, label in net["transitions"]) + "};")
    out.append("  c.arcs = {" + ",".join("{" + s(a[0]) + "," + s(a[1]) + "," + str(a[2] if len(a) > 2 else 1) + "}"
                                        for a in net["arcs"]) + "};")
    out.append("  c.initial = {" + ",".join("{" + s(p) + "," + str(n) + "}" for p, n in net["initial"]) + "};")
    out.append("  c.final = {" + ",".join("{" + s(p) + "," + str(n) + "}" for p, n in net["final"]) + "};")
    dfg = case["dfg"]
    out.append("  c.dfg_edges = {" + ",".join("{" + s(a) + "," + s(b) + "," + str(n) + "}" for a, b, n in dfg["edges"]) + "};")
    out.append("  c.dfg_starts = {" + ",".join("{" + s(a) + "," + str(n) + "}" for a, n in dfg["starts"]) + "};")
    out.append("  c.dfg_ends = {" + ",".join("{" + s(a) + "," + str(n) + "}" for a, n in dfg["ends"]) + "};")
    obs = case["observations"]
    if "costs" in case:
        names = [t for t, _ in net["transitions"]]
        costs = case["costs"]
        out.append("  c.model_costs = {" + ",".join(num(costs["model"][t]) for t in names) + "};")
        out.append("  c.sync_costs = {" + ",".join(num(costs["sync"].get(t, 0)) for t in names) + "};")
        out.append("  c.trace_costs = {" + ",".join(
            "{" + ",".join(num(v) for v in row) + "}" for row in costs["trace"]) + "};")
    for prefix, costed in (("petri:", "false"), ("petricost:", "true")):
        for family, variant in ENUM_VARIANTS.items():
            for trace, entry in enumerate(obs.get(prefix + family, [])):
                runs = values(entry)
                if any("timeout" in v for v in runs):
                    out.append(f"  // {prefix}{family} trace {trace}: reference timed out")
                    continue
                if all("error" in v for v in runs):
                    outcome, tuples = "kError", []
                elif all("result" in v and v["result"] is None for v in runs):
                    outcome, tuples = "kNone", []
                elif all("result" in v and v["result"] is not None for v in runs):
                    outcome, tuples = "kAlignment", []
                    for v in runs:
                        r = v["result"]
                        item = "{" + ",".join(num(r[k]) for k in ("cost", "bwc", "fitness")) + "}"
                        if item not in tuples:
                            tuples.append(item)
                else:
                    raise ValueError(f"{case['name']} {family} {trace}: mixed outcomes")
                PETRI_ROWS[variant][outcome] += 1
                out.append(f"  c.petri.push_back({{AlignmentVariant::{variant},{costed},{trace},"
                           f"Outcome::{outcome},{{{','.join(tuples)}}}}});")
        for family, variant in GENERATOR_VARIANTS.items():
            for trace, entry in enumerate(obs.get(prefix + family, [])):
                union, costs_seen, truncated = [], set(), False
                runs = values(entry)
                if any("result" not in value for value in runs):
                    # The capture's per-trace time limit expired; nothing to bind.
                    out.append(f"  // {prefix}{family} trace {trace}: reference timed out")
                    continue
                for value in runs:
                    truncated |= len(value["result"]) >= GENERATOR_LIMIT
                    for alignment in value["result"]:
                        costs_seen.add(alignment["cost"])
                        if alignment["alignment"] not in union:
                            union.append(alignment["alignment"])
                if len(costs_seen) > 1:
                    raise ValueError(f"{case['name']} {family} {trace}: generator costs differ")
                cost = num(costs_seen.pop()) if costs_seen else "0.0"
                GENERATOR_ROWS[variant][0] += 1
                GENERATOR_ROWS[variant][1] += not union
                out.append(f"  c.generators.push_back({{AlignmentVariant::{variant},{costed},{trace},"
                           f"{cost},{str(truncated).lower()},"
                           "{" + ",".join(moves(a) for a in union) + "}});")
    for trace, entry in enumerate(obs["dfg:CLASSIC"]):
        if "stable" not in entry:
            raise ValueError(f"{case['name']} dfg {trace}: unstable")
        r = entry["stable"]["result"]
        COUNTS["kDfgRows"] += 1
        if r is None:
            out.append(f"  c.dfg.push_back({{{trace},false,{{}},0.0,0}});")
        else:
            out.append(f"  c.dfg.push_back({{{trace},true,{moves(r['alignment'])},{num(r['cost'])},{r['visited_states']}}});")
    for family in sorted(f for f in obs if f.startswith("replay:")):
        spec = replay_family(family)
        if spec is None:
            continue
        variant, nonempty, option = spec
        side = "kForwardReplay" if variant == "kForward" else "kBackwardsReplay"
        head = f"TokenReplayVariant::{variant},{str(nonempty).lower()},{s(option)}"
        runs = values(obs[family])
        if any("timeout" in v for v in runs):
            out.append(f"  // {family}: reference timed out")
            continue
        if not all("result" in v for v in runs):
            if any("result" in v for v in runs):
                raise ValueError(f"{case['name']} {family}: mixed replay outcomes")
            COUNTS[side + "Errors"] += 1
            out.append(f"  c.replay.push_back({{{head},true,{{}}}});")
            continue
        rows = []
        COUNTS[side + "Rows"] += len(runs[0]["result"])
        for k in range(len(runs[0]["result"])):
            admissible = []
            for v in runs:
                code = replay_row(v["result"][k])
                if code not in admissible:
                    admissible.append(code)
            rows.append("{" + ",".join(admissible) + "}")
        out.append(f"  c.replay.push_back({{{head},false,{{{','.join(rows)}}}}});")
    out.append("  return c;")
    out.append("}")
    return "\n".join(out)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("banks", type=Path, nargs="+")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    lines = ["// Generated by scripts/generate-conformance-parity-fixture.py; do not edit."]
    cases = []
    for path in args.banks:
        raw = path.read_bytes()
        bank = json.loads(gzip.decompress(raw))
        lines += [
            f"// Reference bank {path.name} SHA256 {hashlib.sha256(raw).hexdigest()}",
            f"//   {bank['reference_version']} at {bank['reference_commit']}, {bank.get('corpus', 'trees')} "
            f"corpus seed {bank['seed']}, {len(bank['runs'])} merged capture runs.",
        ]
        cases += bank["cases"]
    lines += [
        "#if defined(_MSC_VER)",
        "#define MINERA_CONFORMANCE_PARITY_NOINLINE __declspec(noinline)",
        "#else",
        "#define MINERA_CONFORMANCE_PARITY_NOINLINE __attribute__((noinline))",
        "#endif",
    ]
    for index, case in enumerate(cases):
        lines.append(case_function(index, case))
    lines += [f"constexpr std::size_t {name} = {value};" for name, value in COUNTS.items()]
    # Indexed by AlignmentVariant, then Outcome (alignment, none, error).
    lines.append("constexpr std::size_t kPetriRows[4][3] = {" + ",".join(
        "{" + ",".join(str(PETRI_ROWS[v][o]) for o in ("kAlignment", "kNone", "kError")) + "}"
        for v in ENUM_VARIANTS.values()) + "};")
    # Indexed by AlignmentVariant minus the four enum variants: rows, empty rows.
    lines.append("constexpr std::size_t kGeneratorRows[2][2] = {" + ",".join(
        "{" + ",".join(map(str, GENERATOR_ROWS[v])) + "}" for v in GENERATOR_VARIANTS.values()) + "};")
    lines.append("inline std::vector<ConformanceParityCase> conformance_parity_cases() {")
    lines.append("  std::vector<ConformanceParityCase> cases;")
    for index in range(len(cases)):
        lines.append(f"  cases.push_back(conformance_parity_case_{index}());")
    lines.append("  return cases;")
    lines.append("}")
    lines.append("#undef MINERA_CONFORMANCE_PARITY_NOINLINE")
    args.output.write_text("\n".join(lines) + "\n", newline="\n")


if __name__ == "__main__":
    main()
