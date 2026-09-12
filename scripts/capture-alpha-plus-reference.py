"""Capture public Alpha+ graph and marking boundaries without source inspection."""
import argparse
import ast
import itertools
import json
import os
import subprocess
import tempfile
import warnings
from pathlib import Path
from xml.sax.saxutils import quoteattr


REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"


def _load_public_reference_api():
    """Load the public reference implementation only for capture mode.

    The structural summarizers in this module are also used by the native-only
    support checker.  Keeping the PM4Py imports here means that checker can
    read a captured artifact without requiring the reference environment.
    Warnings are muted before importing any public reference modules because
    their import-time notices are not part of the captured observations.
    """
    warnings.filterwarnings("ignore")
    import pm4py
    from pm4py.algo.discovery.alpha.variants import plus
    from pm4py.objects.log.obj import Event, EventLog, Trace

    return pm4py, plus, Event, EventLog, Trace


def place_name(value):
    if isinstance(value, str) and value.startswith("("):
        try:
            parsed = ast.literal_eval(value)
            if isinstance(parsed, tuple) and len(parsed) == 2 and all(isinstance(side, set) for side in parsed):
                return [sorted(side) for side in parsed]
        except (ValueError, SyntaxError):
            pass
    if isinstance(value, (set, frozenset)):
        return sorted(value)
    if isinstance(value, (tuple, list)):
        return [place_name(item) for item in value]
    return value


def summarize(net, initial, final):
    def place(p):
        labels = lambda arcs, endpoint: sorted(
            [getattr(arc, endpoint).label for arc in arcs],
            key=lambda label: (label is not None, label or ""))
        return dict(name=place_name(p.name), incoming=labels(p.in_arcs, "source"),
                    outgoing=labels(p.out_arcs, "target"),
                    initial=initial.get(p, 0), final=final.get(p, 0))
    ordered = lambda values: sorted(values, key=lambda value: json.dumps(value, sort_keys=True))
    return dict(
        places=ordered([place(p) for p in net.places]),
        transitions=ordered([dict(label=t.label,
                                 inputs=ordered([place_name(a.source.name) for a in t.in_arcs]),
                                 outputs=ordered([place_name(a.target.name) for a in t.out_arcs]))
                             for t in net.transitions]),
        detached_initial=ordered([place(p) for p in initial if p not in net.places]),
        detached_final=ordered([place(p) for p in final if p not in net.places]))


def summarize_native(value):
    validate_native_result(value)
    ordered = lambda values: sorted(values, key=lambda item: json.dumps(item, sort_keys=True))
    name = lambda value: value["boundary"] if "boundary" in value else [sorted(value["before"]), sorted(value["after"])]
    by_id = {place["id"]: name(place["name"]) for place in value["places"]}
    labels = lambda values: sorted(values, key=lambda label: (label is not None, label or ""))
    return dict(
        places=ordered([dict(name=name(p["name"]),
                             incoming=labels([t["label"] for t in value["transitions"] for ref in t["outputs"] if ref == p["id"]]),
                             outgoing=labels([t["label"] for t in value["transitions"] for ref in t["inputs"] if ref == p["id"]]),
                             initial=p["initial"], final=p["final"]) for p in value["places"]]),
        transitions=ordered([dict(label=t["label"], inputs=ordered([by_id[p] for p in t["inputs"]]),
                                 outputs=ordered([by_id[p] for p in t["outputs"]])) for t in value["transitions"]]),
        detached_initial=ordered([dict(name=name(p["name"]), incoming=[], outgoing=[], initial=p["tokens"], final=0)
                                  for p in value["detached_initial_marking"]]),
        detached_final=ordered([dict(name=name(p["name"]), incoming=[], outgoing=[], initial=0, final=p["tokens"])
                                for p in value["detached_final_marking"]]))


def validate_native_result(value):
    """Reject malformed raw Alpha+ JSON before it can enter support.

    This is deliberately a transport/schema check.  It does not compare the
    graph to a public observation or infer a particular implementation.
    """
    if not isinstance(value, dict) or value.get("family") != "ALPHA_PLUS":
        raise ValueError("native Alpha+ output is not an ALPHA_PLUS result")
    required = {"family", "places", "transitions", "detached_initial_marking", "detached_final_marking"}
    if not required.issubset(value):
        raise ValueError("native Alpha+ output is missing required fields")
    places = value["places"]
    transitions = value["transitions"]
    detached_initial = value["detached_initial_marking"]
    detached_final = value["detached_final_marking"]
    if not all(isinstance(items, list) for items in (places, transitions, detached_initial, detached_final)):
        raise ValueError("native Alpha+ result fields must be arrays")

    def integer(value, *, positive=False):
        if isinstance(value, bool) or not isinstance(value, int) or value < (1 if positive else 0):
            raise ValueError("native Alpha+ marking values must be nonnegative integers")

    def place_name(name):
        if not isinstance(name, dict):
            raise ValueError("native Alpha+ place name must be an object")
        if set(name) == {"boundary"}:
            if not isinstance(name["boundary"], str) or not name["boundary"]:
                raise ValueError("native Alpha+ boundary name must be non-empty text")
            return
        if set(name) != {"before", "after"}:
            raise ValueError("native Alpha+ place name has an unexpected shape")
        if not all(isinstance(side, list) and all(isinstance(label, str) for label in side)
                   for side in (name["before"], name["after"])):
            raise ValueError("native Alpha+ place pair must contain text arrays")

    for index, place in enumerate(places):
        if not isinstance(place, dict) or set(place) != {"id", "name", "initial", "final"}:
            raise ValueError("native Alpha+ place has an unexpected shape")
        if place["id"] != index:
            raise ValueError("native Alpha+ place ids must be contiguous")
        place_name(place["name"])
        integer(place["initial"])
        integer(place["final"])

    place_count = len(places)
    for index, transition in enumerate(transitions):
        if not isinstance(transition, dict) or set(transition) != {"id", "label", "inputs", "outputs"}:
            raise ValueError("native Alpha+ transition has an unexpected shape")
        if transition["id"] != index:
            raise ValueError("native Alpha+ transition ids must be contiguous")
        if transition["label"] is not None and not isinstance(transition["label"], str):
            raise ValueError("native Alpha+ transition labels must be text or null")
        for arcs in (transition["inputs"], transition["outputs"]):
            if not isinstance(arcs, list) or any(
                    isinstance(place_id, bool) or not isinstance(place_id, int) or
                    place_id < 0 or place_id >= place_count for place_id in arcs):
                raise ValueError("native Alpha+ transition arc references an invalid place")

    def detached(items):
        for marking in items:
            if not isinstance(marking, dict) or set(marking) != {"name", "tokens"}:
                raise ValueError("native Alpha+ detached marking has an unexpected shape")
            place_name(marking["name"])
            integer(marking["tokens"], positive=True)

    detached(detached_initial)
    detached(detached_final)
    if (sum(place["initial"] for place in places) +
            sum(marking["tokens"] for marking in detached_initial) != 1):
        raise ValueError("native Alpha+ initial marking does not contain one token")
    if (sum(place["final"] for place in places) +
            sum(marking["tokens"] for marking in detached_final) != 1):
        raise ValueError("native Alpha+ final marking does not contain one token")
    return value


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--native", type=Path, help="Compare the minera_export --alpha-plus-result output")
    parser.add_argument("--extended", action="store_true", help="Include ordinary short traces and pairs of traces")
    parser.add_argument("--reference-samples", type=int, default=1,
                        help="Repeated public observations per fixture; compare against sampled output support")
    args = parser.parse_args()
    if args.reference_samples < 1:
        parser.error("--reference-samples must be positive")
    pm4py, plus, Event, EventLog, Trace = _load_public_reference_api()
    assert pm4py.__version__ == "2.7.22"
    fixtures = [[]] + [[list(word)] for length in range(4)
                      for word in itertools.product(("a", "artificial_start", "artificial_end"), repeat=length)]
    fixtures += [[list(word)] for word in ["aa", "aLLa", "aLLab", "LLa", "aLL", "aLLb"]]
    fixtures += [[[], ["a"]], [["a", "a"], ["b"]], [["a", "L", "L", "a"], ["a", "b"]]]
    if args.extended:
        fixtures += [[list(word)] for length in range(1, 6)
                     for word in itertools.product(("a", "b"), repeat=length)]
        words = [list(word) for length in range(4)
                 for word in itertools.product(("a", "b"), repeat=length)]
        fixtures += [[left, right] for left, right in itertools.combinations_with_replacement(words, 2)]
        boundary_words = [list(word) for length in range(3)
                          for word in itertools.product(("a", "artificial_start", "artificial_end"), repeat=length)]
        fixtures += [[left, right] for left, right in itertools.product(boundary_words, repeat=2)
                     if any(label != "a" for label in left + right)]
    rows = []
    for traces in fixtures:
        for cleanup in (False, True):
            observations = {}
            for _ in range(args.reference_samples):
                log = EventLog([Trace([Event({"concept:name": label}) for label in word]) for word in traces])
                try:
                    observed = summarize(*plus.apply(log, parameters={"remove_unconnected": cleanup}))
                except Exception as error:
                    observed = dict(error=type(error).__name__, message=str(error))
                observations[json.dumps(observed, sort_keys=True)] = observed
            result = next(iter(observations.values()))
            row = dict(traces=traces, remove_unconnected=cleanup, result=result)
            if args.reference_samples > 1:
                row["reference_support"] = list(observations.values())
            if args.native:
                xml = '<log>' + ''.join('<trace>' + ''.join(
                    '<event><string key="concept:name" value=' + quoteattr(label) + '/></event>'
                    for label in word) + '</trace>' for word in traces) + '</log>'
                with tempfile.NamedTemporaryFile(mode="w", suffix=".xes", encoding="utf-8", delete=False) as source:
                    source.write(xml)
                    filename = source.name
                try:
                    output = subprocess.run([str(args.native.resolve()), filename, "--alpha-plus-result",
                                             "--alpha-remove-unconnected", str(cleanup).lower()],
                                            capture_output=True, text=True, encoding="utf-8", timeout=30,
                                            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
                    if output.returncode not in (0, 1) or "Usage:" in output.stderr:
                        raise RuntimeError(f"Native audit executable failed before discovery: {output.stderr[:400]}")
                    actual = summarize_native(json.loads(output.stdout)) if output.returncode == 0 else dict(error=True, message=output.stderr)
                finally:
                    Path(filename).unlink()
                row["native"] = actual
                row["matches"] = any(("error" in expected and "error" in actual) or actual == expected
                                     for expected in observations.values())
            rows.append(row)
    args.output.write_text(json.dumps(dict(reference=pm4py.__version__,
                                          reference_commit=REFERENCE_COMMIT,
                                          reference_samples=args.reference_samples,
                                          environment=dict(PYTHONHASHSEED=os.environ.get("PYTHONHASHSEED")),
                                          rows=rows), indent=2) + "\n", encoding="utf-8")
    print(f"captured {len(rows)} public Alpha+ graph/marking observations")
    if args.native:
        criterion = "matches" if args.reference_samples == 1 else "within sampled reference support"
        print(f"native {criterion}: {sum(row['matches'] for row in rows)}/{len(rows)}")
        ordinary = [row for row in rows if not any(label in ("artificial_start", "artificial_end")
                    for trace in row["traces"] for label in trace)]
        print(f"without reserved labels: {sum(row['matches'] for row in ordinary)}/{len(ordinary)}")


if __name__ == "__main__":
    main()
