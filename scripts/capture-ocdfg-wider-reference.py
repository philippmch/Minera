"""Capture public OCDFG discovery outputs on the disjoint wider fixture bank.

Only the public ``pm4py.discover_ocdfg`` call and the public OCEL constructor
are used; no reference implementation source is read. Each fixture is built
directly from tables, so the importer's timezone handling stays outside this
gate. The capture runs once per PYTHONHASHSEED and records which outputs are
stable across seeds.
"""
import argparse
import hashlib
import json
import os
import subprocess
import sys
import warnings
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ocdfg_wider_fixtures import PROFILES, fixtures  # noqa: E402

REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"


def _sorted(values):
    return sorted([list(value) if isinstance(value, tuple) else value for value in values])


def _ordered(values):
    return [list(value) if isinstance(value, tuple) else value for value in values]


def _metric_family(raw, keys):
    """Turn {'events': {..}, ...} keyed views into {key path: metric}."""
    result = {}
    for field in ("events", "unique_objects", "total_objects"):
        for path, values in _walk(raw.get(field, {}), keys):
            entry = result.setdefault(path, {})
            entry[field] = _ordered(values) if field == "total_objects" else _sorted(values)
    return {"\u0000".join(path): value for path, value in sorted(result.items())}


def _walk(node, depth):
    if depth == 0:
        yield (), node
        return
    for key, child in node.items():
        for path, value in _walk(child, depth - 1):
            yield (key,) + path, value


def canonical(graph):
    edges = {}
    for field in ("event_couples", "unique_objects", "total_objects"):
        for object_type, per_type in graph["edges"].get(field, {}).items():
            for (source, target), values in per_type.items():
                edges.setdefault(object_type + "\u0000" + source + "\u0000" + target, {})[field] = _sorted(values)
    for field in ("event_couples", "total_objects"):
        for object_type, per_type in graph.get("edges_performance", {}).get(field, {}).items():
            for (source, target), values in per_type.items():
                edges.setdefault(object_type + "\u0000" + source + "\u0000" + target, {})[
                    "performance_" + field] = [float(value).hex() for value in values]
    return {
        "keys": sorted(graph.keys()),
        "activities": _sorted(graph["activities"]),
        "object_types": _sorted(graph["object_types"]),
        "activities_indep": _metric_family(graph["activities_indep"], 1),
        "activities_ot": _metric_family(graph["activities_ot"], 2),
        "start_activities": _metric_family(graph["start_activities"], 2),
        "end_activities": _metric_family(graph["end_activities"], 2),
        "edges": dict(sorted(edges.items())),
    }


def build_ocel(pm4py, pandas, fixture):
    from pm4py.objects.ocel.obj import OCEL
    events = pandas.DataFrame({
        "ocel:eid": [event[0] for event in fixture["events"]],
        "ocel:activity": [event[1] for event in fixture["events"]],
        "ocel:timestamp": pandas.to_datetime([event[2] for event in fixture["events"]], format="ISO8601"),
    })
    objects = pandas.DataFrame({
        "ocel:oid": [obj[0] for obj in fixture["objects"]],
        "ocel:type": [obj[1] for obj in fixture["objects"]],
    })
    activity = {event[0]: event[1] for event in fixture["events"]}
    kind = {obj[0]: obj[1] for obj in fixture["objects"]}
    time = dict(zip(events["ocel:eid"], events["ocel:timestamp"]))
    relations = pandas.DataFrame({
        "ocel:eid": [rel[0] for rel in fixture["relations"]],
        "ocel:oid": [rel[1] for rel in fixture["relations"]],
        "ocel:qualifier": [rel[2] for rel in fixture["relations"]],
        "ocel:activity": [activity[rel[0]] for rel in fixture["relations"]],
        "ocel:timestamp": pandas.Series([time[rel[0]] for rel in fixture["relations"]],
                                        dtype=events["ocel:timestamp"].dtype),
        "ocel:type": [kind[rel[1]] for rel in fixture["relations"]],
    })
    return OCEL(events=events, objects=objects, relations=relations), str(events["ocel:timestamp"].dtype)


def capture_one():
    warnings.filterwarnings("ignore")
    import pandas
    import pm4py
    if (pm4py.__version__, pandas.__version__) != ("2.7.22", "3.0.5"):
        raise RuntimeError("requires pm4py==2.7.22 and pandas==3.0.5")
    rows = []
    for fixture in fixtures():
        ocel, dtype = build_ocel(pm4py, pandas, fixture)
        for profile, (business_hours, slots) in PROFILES.items():
            kwargs = {"business_hours": business_hours}
            if slots is not None:
                kwargs["business_hour_slots"] = [tuple(slot) for slot in slots]
            row = {"fixture": fixture["name"], "profile": profile, "timestamp_dtype": dtype}
            try:
                row["result"] = canonical(pm4py.discover_ocdfg(ocel, **kwargs))
            except Exception as error:  # retained as an observation
                row["error"] = {"type": type(error).__name__, "message": str(error)}
            rows.append(row)
    return {"pm4py": pm4py.__version__, "pandas": pandas.__version__,
            "seed": os.environ.get("PYTHONHASHSEED"), "rows": rows}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    parser.add_argument("--seeds", default="0,1,2,3")
    parser.add_argument("--worker", action="store_true")
    args = parser.parse_args()
    if args.worker:
        Path(args.output).write_text(json.dumps(capture_one(), ensure_ascii=False))
        return
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    captures = []
    for seed in args.seeds.split(","):
        part = output.with_suffix(f".seed{seed}.json")
        subprocess.run([sys.executable, "-I", "-B", __file__, "--worker", "--output", str(part)],
                       env={**os.environ, "PYTHONHASHSEED": seed, "TZ": "UTC"}, check=True)
        captures.append(json.loads(part.read_text()))
        part.unlink()
    first = captures[0]["rows"]
    stability = []
    for index, row in enumerate(first):
        variants = {json.dumps(capture["rows"][index], sort_keys=True) for capture in captures}
        stability.append(len(variants) == 1)
    bank = {
        "reference_commit": REFERENCE_COMMIT,
        "versions": {"pm4py": captures[0]["pm4py"], "pandas": captures[0]["pandas"]},
        "seeds": [capture["seed"] for capture in captures],
        "fixtures_sha256": hashlib.sha256(json.dumps(fixtures(), ensure_ascii=False).encode()).hexdigest(),
        "rows": [dict(row, stable_across_seeds=stable) for row, stable in zip(first, stability)],
        "unstable_variants": {
            f"{row['fixture']}/{row['profile']}": [capture["rows"][index] for capture in captures]
            for index, (row, stable) in enumerate(zip(first, stability)) if not stable},
    }
    output.write_text(json.dumps(bank, ensure_ascii=False, sort_keys=True))
    print(f"{len(first)} rows, {sum(stability)} stable across seeds {bank['seeds']}")


if __name__ == "__main__":
    main()
