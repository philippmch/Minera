"""Merge independent conformance captures into one stability-classified bank.

Each input is a capture-conformance-parity-reference.py output produced in a
fresh process (different PYTHONHASHSEED). An observation is *stable* when every
run produced the same value after canonical renaming; only stable observations
bind exact native equality. Unstable observations keep every distinct value so
native results can be checked against the contract invariants instead.

Generated transition identifiers (UUIDs) differ between runs; visible
transitions are renamed to "t:<label>" (labels are unique in this corpus) and
silent transitions keep their structural names.
"""
import argparse
import gzip
import hashlib
import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path

UUID = re.compile(r"[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}")


def local(tag):
    return tag.rsplit("}", 1)[-1]


def parse_net(pnml):
    root = ET.fromstring(pnml)
    places, transitions, arcs, initial, final = [], [], [], {}, {}
    for node in root.iter():
        kind = local(node.tag)
        if kind == "place" and "id" in node.attrib:
            places.append(node.attrib["id"])
            for child in node:
                if local(child.tag) == "initialMarking":
                    initial[node.attrib["id"]] = int(child.find("{*}text").text if child.find("{*}text") is not None else child[0].text)
        elif kind == "transition" and "id" in node.attrib:
            silent = any(local(c.tag) == "toolspecific" and c.attrib.get("activity") == "$invisible$"
                         for c in node)
            name = node.find(".//{*}text") if node.find(".//{*}text") is not None else node.find(".//text")
            transitions.append([node.attrib["id"], None if silent else name.text])
        elif kind == "arc":
            arcs.append([node.attrib["source"], node.attrib["target"]])
    for marking in root.iter():
        if local(marking.tag) == "finalmarkings":
            for place in marking.iter():
                if local(place.tag) == "place":
                    text = [t for t in place.iter() if local(t.tag) == "text"][0].text
                    final[place.attrib["idref"]] = int(text)
    rename = {}
    for name, label in transitions:
        rename[name] = name if label is None or not UUID.fullmatch(name) else "t:" + label
    if len(set(rename.values())) != len(rename):
        raise ValueError("canonical transition names collide")
    # Element order follows generated identifiers, so it is canonicalized.
    return dict(places=sorted(places),
                transitions=sorted([[rename[n], l] for n, l in transitions]),
                arcs=sorted([[rename.get(s, s), rename.get(t, t)] for s, t in arcs]),
                initial=sorted(initial.items()), final=sorted(final.items())), rename


def canonical(value, rename):
    if isinstance(value, str):
        return rename.get(value, value)
    if isinstance(value, list):
        return [canonical(v, rename) for v in value]
    if isinstance(value, dict):
        return {k: canonical(v, rename) for k, v in value.items()}
    return value


def canonical_replay(value, rename):
    if not isinstance(value, dict) or "result" not in value:
        return value
    rows = []
    for row in canonical(value["result"], rename):
        row = dict(row)
        row["enabled_transitions_in_marking"] = sorted(row["enabled_transitions_in_marking"])
        rows.append(row)
    return {"result": rows}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("runs", nargs="+",
                        help="capture files, optionally FILE=PREFIX[,PREFIX] to ignore "
                             "observation families with those prefixes from that file")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    runs = []
    for spec in args.runs:
        path, _, ignored = spec.partition("=")
        run = json.load(gzip.open(path))
        prefixes = tuple(p for p in ignored.split(",") if p)
        for case in run["cases"]:
            for family in list(case["observations"]):
                if prefixes and family.startswith(prefixes):
                    del case["observations"][family]
        run["source"] = Path(path).name
        runs.append(run)
    first = runs[0]
    for run in runs[1:]:
        if run["runtime"] != first["runtime"] or run["seed"] != first["seed"]:
            raise ValueError("runs differ in runtime or corpus seed")
    cases = []
    summary = {}
    for parts in zip(*[run["cases"] for run in runs]):
        base = parts[0]
        if any(p["tree"] != base["tree"] or p["traces"] != base["traces"] for p in parts):
            raise ValueError("runs differ in corpus")
        nets = [parse_net(p["pnml"]) for p in parts]
        if any(n[0] != nets[0][0] for n in nets):
            raise ValueError(f"{base['name']}: canonical nets differ between runs")
        families = sorted({f for p in parts for f in p["observations"]})
        observations = {}
        for family in families:
            values = []
            for part, (_, rename) in zip(parts, nets):
                if family not in part["observations"]:
                    continue
                value = part["observations"][family]
                if family.startswith("replay:"):
                    value = canonical_replay(value, rename)
                values.append(value)
            per_trace = isinstance(values[0], list)
            rows = list(zip(*values)) if per_trace else [tuple(values)]
            merged = []
            for row in rows:
                distinct = []
                for item in row:
                    if item not in distinct:
                        distinct.append(item)
                stable = len(distinct) == 1
                key = family + (":stable" if stable else ":unstable")
                summary[key] = summary.get(key, 0) + 1
                merged.append({"stable": distinct[0]} if stable else {"unstable": distinct})
            observations[family] = merged if per_trace else merged[0]
        cases.append(dict(name=base["name"], tree=base["tree"], traces=base["traces"],
                          dfg=base["dfg"], net=nets[0][0], observations=observations))
    payload = dict(reference_version=first["reference_version"],
                   reference_commit=first["reference_commit"], runtime=first["runtime"],
                   python=first["python"], seed=first["seed"],
                   runs=[dict(source=run["source"], hash_seed=run["hash_seed"],
                              permutation=run.get("permutation", 0)) for run in runs],
                   summary=summary, cases=cases)
    data = json.dumps(payload, sort_keys=True, separators=(",", ":")).encode()
    with gzip.GzipFile(filename="", mode="wb", fileobj=args.output.open("wb"), mtime=0) as sink:
        sink.write(data)
    print(json.dumps(summary, indent=1, sort_keys=True))
    print(f"sha256 {hashlib.sha256(data).hexdigest()}")


if __name__ == "__main__":
    main()
