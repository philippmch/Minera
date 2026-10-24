"""Compare native OCDFG discovery with the captured wider public bank.

Rebuilds the deterministic fixtures, runs the compiled ``minera_ocel`` CLI for
every timing profile and compares all discovery fields: set-valued members as
sets, ordered association lists in order, and every performance sample
bitwise and in order. No reference module is imported here.
"""
import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ocdfg_wider_fixtures import PROFILES, fixtures, fixtures_round2, native_ocel2_json  # noqa: E402

BANKS = {"wider": fixtures, "round2": fixtures_round2}


def _pairs(values):
    return [list(value) for value in values]


def _family(node, depth):
    result = {}

    def walk(current, path):
        if len(path) == depth:
            result["\u0000".join(path)] = {
                "events": sorted(current["events"]),
                "unique_objects": sorted(current["unique_objects"]),
                "total_objects": _pairs(current["total_objects"]),
            }
            return
        for key, child in current.items():
            walk(child, path + [key])

    walk(node, [])
    return dict(sorted(result.items()))


def canonical_native(graph):
    edges = {}
    for object_type, per_type in graph["edges"].items():
        for edge in per_type.values():
            key = object_type + "\u0000" + edge["source_activity"] + "\u0000" + edge["target_activity"]
            entry = {
                "event_couples": sorted(_pairs(edge["event_couples"])),
                "unique_objects": sorted(edge["unique_objects"]),
                "total_objects": sorted(_pairs(edge["total_objects"])),
            }
            if graph.get("performance_computed", True):
                entry["performance_event_couples"] = [float(v).hex() for v in edge["performance_event_couples"]]
                entry["performance_total_objects"] = [float(v).hex() for v in edge["performance_total_objects"]]
            edges[key] = entry
    return {
        "activities": sorted(graph["activities"]),
        "object_types": sorted(graph["object_types"]),
        "activities_indep": _family(graph["activities_indep"], 1),
        "activities_ot": _family(graph["activities_ot"], 2),
        "start_activities": _family(graph["start_activities"], 2),
        "end_activities": _family(graph["end_activities"], 2),
        "edges": dict(sorted(edges.items())),
    }


def _text(value):
    if isinstance(value, list):
        return [_text(item) for item in value]
    return value if isinstance(value, str) else str(value)


def textual_reference(result):
    """Native identifiers are text; integer reference IDs compare as decimals.

    Sets are re-sorted after the conversion; ordered association lists keep
    their reference order.
    """
    converted = {}
    for field, value in result.items():
        if field in ("activities_indep", "activities_ot", "start_activities", "end_activities", "edges"):
            converted[field] = {}
            for key, metric in value.items():
                entry = {}
                for name, items in metric.items():
                    items = _text(items)
                    ordered = name == "total_objects" and field != "edges"
                    entry[name] = items if ordered or name.startswith("performance_") else sorted(items)
                converted[field][key] = entry
        else:
            converted[field] = value
    return converted


def native_arguments(profile, legacy):
    business_hours, slots = PROFILES[profile]
    if legacy:
        return []
    arguments = ["--ocdfg-business-hours", "true" if business_hours else "false"]
    if slots is not None:
        arguments += ["--ocdfg-business-hour-slots-json", json.dumps(slots)]
    return arguments


def differences(expected, actual):
    found = []
    for field in sorted(set(expected) | set(actual)):
        if field == "keys":
            continue
        left, right = expected.get(field), actual.get(field)
        if left == right:
            continue
        if isinstance(left, dict) and isinstance(right, dict):
            for key in sorted(set(left) | set(right)):
                if left.get(key) != right.get(key):
                    found.append({"field": field, "key": key.split("\u0000"),
                                  "expected": left.get(key), "actual": right.get(key)})
        else:
            found.append({"field": field, "expected": left, "actual": right})
    return found


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--capture", required=True)
    parser.add_argument("--cli", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--bank", choices=sorted(BANKS), default="wider")
    args = parser.parse_args()
    bank = json.loads(Path(args.capture).read_text())
    bank_fixtures = BANKS[args.bank]()
    digest = hashlib.sha256(json.dumps(bank_fixtures, ensure_ascii=False).encode()).hexdigest()
    if digest != bank["fixtures_sha256"]:
        raise SystemExit("fixture generator no longer matches the captured bank")
    by_name = {fixture["name"]: fixture for fixture in bank_fixtures}
    report = {"capture_sha256": hashlib.sha256(Path(args.capture).read_bytes()).hexdigest(),
              "rows": []}
    with tempfile.TemporaryDirectory() as scratch:
        for row in bank["rows"]:
            path = Path(scratch) / (row["fixture"] + ".json")
            if not path.exists():
                path.write_text(json.dumps(native_ocel2_json(by_name[row["fixture"]]), ensure_ascii=False),
                                encoding="utf-8")
            variants = [False] + ([True] if row["profile"] == "elapsed" else [])
            for legacy in variants:
                command = [args.cli, str(path), "--discover", "ocdfg"] + native_arguments(row["profile"], legacy)
                completed = subprocess.run(command, capture_output=True)
                outcome = {"fixture": row["fixture"], "profile": row["profile"],
                           "native_path": "legacy" if legacy else "typed-timing",
                           "stable_across_seeds": row["stable_across_seeds"]}
                if completed.returncode != 0:
                    outcome["native_error"] = completed.stderr.decode("utf-8", "replace")
                    outcome["match"] = False
                elif "error" in row:
                    outcome["match"] = False
                    outcome["reference_error"] = row["error"]
                else:
                    actual = canonical_native(json.loads(completed.stdout))
                    found = differences(textual_reference(row["result"]), actual)
                    outcome["match"] = not found
                    if found:
                        outcome["differences"] = found[:20]
                        outcome["difference_count"] = len(found)
                report["rows"].append(outcome)
    matched = sum(outcome["match"] for outcome in report["rows"])
    report["summary"] = {"outcomes": len(report["rows"]), "matched": matched}
    Path(args.output).write_text(json.dumps(report, ensure_ascii=False, indent=1))
    print(f"{matched}/{len(report['rows'])} native outcomes match")
    return 0 if matched == len(report["rows"]) else 1


if __name__ == "__main__":
    sys.exit(main())
