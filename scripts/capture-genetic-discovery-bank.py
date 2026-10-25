#!/usr/bin/env python3
"""Capture a black-box bank of the PM4Py 2.7.22 classic genetic miner.

Run inside an environment with the pinned reference release installed:

    PYTHONHASHSEED=0 python scripts/capture-genetic-discovery-bank.py out0.json
    PYTHONHASHSEED=1 python scripts/capture-genetic-discovery-bank.py --replay out0.json out1.json
    python scripts/capture-genetic-discovery-bank.py --merge out0.json out1.json \
        docs/genetic-discovery-bank.json.gz

Only public functions are called. Case kinds:
  sources   util.get_src_sink_sets_for_wfnet
  net       classic.matrix2petrinet (place names canonicalized: the reference
            spells them with frozenset reprs whose member order follows the
            string-hash seed)
  partition util.add_singleton_partition / util.remove_transition_from_partitions
  fitness   classic.tournament, unsorted and sorted
  operator  outputs of classic.individuals / crossover / mutate / repair
  apply     algorithm.apply with a log_csv buffer
The replay recomputes the deterministic kinds on the stored inputs under
another hash seed; the merge requires them to agree. Operator and apply
outputs are seed dependent and are kept from the first capture only.
"""

import ast
import copy
import datetime
import gzip
import io
import json
import random
import re
import sys
import warnings

warnings.filterwarnings("ignore")

SEED = 20261025


def load_reference():
    import numpy as np
    import pm4py
    from pm4py.algo.discovery.genetic import algorithm as A
    from pm4py.algo.discovery.genetic import util as U
    from pm4py.algo.discovery.genetic.variants import classic as C
    from pm4py.objects.log.obj import Event, EventLog, Trace
    return np, pm4py, A, U, C, Event, EventLog, Trace


def event_log(traces):
    _, _, _, _, _, Event, EventLog, Trace = REF
    log = EventLog()
    start = datetime.datetime(2024, 1, 1)
    for i, activities in enumerate(traces):
        trace = Trace()
        trace.attributes["concept:name"] = str(i)
        for j, activity in enumerate(activities):
            trace.append(Event({"concept:name": activity,
                                "time:timestamp": start + datetime.timedelta(minutes=100 * i + j)}))
        log.append(trace)
    return log


def as_set(partition):
    return {partition} if isinstance(partition, str) else set(partition)


def plain(matrix, transitions):
    """A JSON form of an (I, O) pair: ordered partition lists of sorted members."""
    inputs, outputs = matrix
    def side(m):
        return {key: [sorted(as_set(p)) for p in m[key]] for key in m}
    return {"T": list(transitions), "I": side(inputs), "O": side(outputs)}


def frozen(matrix):
    inputs = {k: [frozenset(p) for p in v] for k, v in matrix["I"].items()}
    outputs = {k: [frozenset(p) for p in v] for k, v in matrix["O"].items()}
    return inputs, outputs


def literal(text):
    """Python value of a reference place-name fragment (sets, frozensets, strings)."""
    return ast.literal_eval(re.sub(r"frozenset\((\{[^}]*\})\)", r"\1", text))


def place_key(name):
    if name in ("i", "o"):
        return name
    if name[:2] in ("o(", "i("):
        transition, partition = literal(name[1:])
        return name[0] + ":" + transition + ":" + ",".join(sorted(as_set(partition)))
    producers, consumers = literal(name)
    return "p:" + ",".join(sorted(as_set(producers))) + ":" + ",".join(sorted(as_set(consumers)))


def describe_net(net):
    places = sorted(place_key(p.name) for p in net.places)
    transitions = []
    for t in net.transitions:
        transitions.append([t.label if t.label is not None else None,
                            sorted(place_key(a.source.name) for a in t.in_arcs),
                            sorted(place_key(a.target.name) for a in t.out_arcs)])
    transitions.sort(key=lambda x: json.dumps(x))
    return {"places": places, "transitions": transitions}


def transition_key(t):
    return " ".join([t.label if t.label is not None else "-",
                     ";".join(sorted(place_key(a.source.name) for a in t.in_arcs)) or "-",
                     ";".join(sorted(place_key(a.target.name) for a in t.out_arcs)) or "-"])


def arc_order(net):
    """Each place's outgoing arcs in the reference's iteration order.

    Token replay breaks equal-length silent-path ties by this order, which
    follows object identity for nets built in memory."""
    return {place_key(p.name): [transition_key(a.target) for a in p.out_arcs] for p in net.places}


def place_names(net):
    """Token replay also orders equal silent paths by place name."""
    return {place_key(p.name): p.name for p in net.places}


def error_of(exc):
    return {"error": [type(exc).__name__, str(exc)]}


def random_matrix(rng, well_formed=False):
    transitions = [chr(97 + i) for i in range(rng.randint(1, 6))]
    rng.shuffle(transitions)
    inputs = {t: [] for t in transitions}
    outputs = {t: [] for t in transitions}
    if well_formed:
        links = {(x, y) for x in transitions for y in transitions if rng.random() < 0.35}
        for t in transitions:
            for side, pool in ((outputs, [y for x, y in links if x == t]),
                               (inputs, [x for x, y in links if y == t])):
                pool = sorted(pool)
                rng.shuffle(pool)
                while pool:
                    k = rng.randint(1, len(pool))
                    side[t].append(sorted(pool[:k]))
                    pool = pool[k:]
    else:
        for t in transitions:
            for side in (inputs, outputs):
                for _ in range(rng.choice((0, 0, 1, 1, 2))):
                    side[t].append(sorted(rng.sample(transitions, rng.randint(1, min(3, len(transitions))))))
    return {"T": transitions, "I": inputs, "O": outputs}


def random_traces(rng, alphabet=6):
    acts = "abcdefgh"[: rng.randint(2, alphabet)]
    return [[rng.choice(acts) for _ in range(rng.randint(1, 7))] for _ in range(rng.randint(1, 6))]


def ga_matrices(rng, count):
    """Matrices produced by the reference's own operators on random logs."""
    np, pm4py, _, _, C, *_ = REF
    result = []
    while len(result) < count:
        traces = random_traces(rng)
        frame = pm4py.convert_to_dataframe(event_log(traces))
        transitions = sorted({a for t in traces for a in t})
        rng.shuffle(transitions)
        random.seed(rng.randrange(1 << 30)); np.random.seed(rng.randrange(1 << 30))
        population = C.individuals(frame, sample_size=4, T=transitions)
        children = list(C.crossover(copy.deepcopy(population[0]), copy.deepcopy(population[1]), transitions))
        children += [C.mutate(copy.deepcopy(p), rate=0.4) for p in population[2:]]
        result += [plain(p, transitions) for p in population + children]
    return result[:count]


def capture():
    np, pm4py, A, U, C, *_ = REF
    rng = random.Random(SEED)
    cases = []

    def add(kind, payload):
        payload["kind"] = kind
        payload["name"] = f"{kind}{len(cases)}"
        cases.append(payload)

    # sources/sinks and conversion on arbitrary, mirrored and GA-made matrices
    matrices = [random_matrix(rng) for _ in range(700)]
    matrices += [random_matrix(rng, well_formed=True) for _ in range(500)]
    matrices += ga_matrices(rng, 700)
    for matrix in matrices:
        add("sources", {"matrix": matrix})
        add("net", {"matrix": matrix})
    for missing in ("z", "q"):
        matrix = random_matrix(rng, well_formed=True)
        matrix["O"][matrix["T"][0]].append([missing])
        add("net", {"matrix": matrix})

    # partition helpers
    for _ in range(400):
        matrix = random_matrix(rng)
        side = dict(matrix["I"])
        key = rng.choice(matrix["T"] + ["z"])
        transition = rng.choice(matrix["T"] + ["y"])
        op = rng.choice(("add", "remove"))
        add("partition", {"op": op, "map": side, "key": key, "transition": transition})

    # tournament fitness
    for _ in range(260):
        traces = random_traces(rng)
        frame = pm4py.convert_to_dataframe(event_log(traces))
        transitions = sorted({a for t in traces for a in t})
        rng.shuffle(transitions)
        random.seed(rng.randrange(1 << 30)); np.random.seed(rng.randrange(1 << 30))
        population = [plain(p, transitions) for p in C.individuals(frame, sample_size=3, T=transitions)]
        population += [random_matrix_over(rng, transitions) for _ in range(2)]
        population.append(population[0])  # the same individual twice keeps both places
        add("fitness", {"traces": traces, "population": population})

    # operator outputs
    for _ in range(150):
        traces = random_traces(rng)
        frame = pm4py.convert_to_dataframe(event_log(traces))
        transitions = sorted({a for t in traces for a in t})
        random.seed(rng.randrange(1 << 30)); np.random.seed(rng.randrange(1 << 30))
        population = C.individuals(frame, sample_size=4, T=transitions)
        for p in population:
            add("operator", {"op": "individuals", "matrix": plain(p, transitions)})
        for child in C.crossover(copy.deepcopy(population[0]), copy.deepcopy(population[1]), transitions):
            add("operator", {"op": "crossover", "matrix": plain(child, transitions)})
        add("operator", {"op": "mutate", "matrix": plain(C.mutate(copy.deepcopy(population[2]), rate=0.5), transitions)})
        causal = np.array([[int(rng.random() < 0.4) for _ in transitions] for _ in transitions])
        repaired = C.repair(*copy.deepcopy(population[3]), causal, transitions)
        add("operator", {"op": "repair", "matrix": plain(repaired, transitions)})

    # apply
    for _ in range(80):
        traces = random_traces(rng, alphabet=5)
        size, generations = rng.randint(6, 12), rng.randint(1, 4)
        random.seed(rng.randrange(1 << 30)); np.random.seed(rng.randrange(1 << 30))
        buffer = io.StringIO()
        try:
            net, im, fm = A.apply(event_log(traces), parameters={
                "population_size": size, "generations": generations, "log_csv": buffer})
            rows = [line.split(",") for line in buffer.getvalue().strip().split("\n")[1:]]
            result = describe_net(net)
            result["arc_order"] = arc_order(net)
            result["place_names"] = place_names(net)
            result["best"] = repr(max(float(v) for v in rows[-1][2:]))
            result["generations"] = len(rows)
        except Exception as exc:  # noqa: BLE001
            result = error_of(exc)
        add("apply", {"traces": traces, "population_size": size, "generations": generations, "result": result})
    return cases


def random_matrix_over(rng, transitions):
    inputs = {t: [] for t in transitions}
    outputs = {t: [] for t in transitions}
    links = {(x, y) for x in transitions for y in transitions if rng.random() < 0.4}
    for t in transitions:
        for side, pool in ((outputs, [y for x, y in links if x == t]), (inputs, [x for x, y in links if y == t])):
            pool = sorted(pool)
            rng.shuffle(pool)
            while pool:
                k = rng.randint(1, len(pool))
                side[t].append(sorted(pool[:k]))
                pool = pool[k:]
    return {"T": list(transitions), "I": inputs, "O": outputs}


DETERMINISTIC = ("sources", "net", "partition", "fitness")


def evaluate(case):
    """The deterministic result of a stored case."""
    np, pm4py, A, U, C, *_ = REF
    kind = case["kind"]
    if kind == "sources":
        inputs, outputs = frozen(case["matrix"])
        sources, sinks = U.get_src_sink_sets_for_wfnet(inputs, outputs, case["matrix"]["T"])
        return [list(sources), list(sinks)]
    if kind == "net":
        inputs, outputs = frozen(case["matrix"])
        try:
            net, im, fm = C.matrix2petrinet(C.GeneticMatrix(inputs, outputs, case["matrix"]["T"]))
        except Exception as exc:  # noqa: BLE001 - reference errors are data
            return error_of(exc)
        result = describe_net(net)
        result["im"] = sorted(place_key(p.name) for p in im)
        result["fm"] = sorted(place_key(p.name) for p in fm)
        return result
    if kind == "partition":
        edited = {k: [frozenset(p) for p in v] for k, v in case["map"].items()}
        helper = U.add_singleton_partition if case["op"] == "add" else U.remove_transition_from_partitions
        helper(edited, case["key"], case["transition"])
        return {k: [sorted(as_set(p)) for p in v] for k, v in edited.items()}
    frame = pm4py.convert_to_dataframe(event_log(case["traces"]))
    transitions = case["population"][0]["T"]
    objects = [frozen(m) for m in case["population"]]
    objects[-1] = objects[0]
    _, fitness = C.tournament(objects, frame, transitions, sort=False)
    ordered, sorted_fitness = C.tournament(objects, frame, transitions, sort=True)
    order = [next(i for i, o in enumerate(objects) if o is p) for p in ordered]
    nets = [C.matrix2petrinet(C.GeneticMatrix(*o, transitions))[0] for o in objects]
    return {"fitness": [repr(f) for f in fitness], "order": order,
            "sorted": [repr(f) for f in sorted_fitness], "arc_orders": [arc_order(n) for n in nets],
            "place_names": [place_names(n) for n in nets]}


def replay(source, out):
    cases = json.load(open(source))
    for case in cases:
        if case["kind"] in DETERMINISTIC:
            case["result"] = evaluate(case)
    json.dump(cases, open(out, "w"))


def merge(paths, out):
    runs = [json.load(open(p)) for p in paths]
    first = runs[0]
    def comparable(case):
        # Arc orders follow object identity; the fitness they produce must not.
        case = copy.deepcopy(case)
        if isinstance(case.get("result"), dict):
            case["result"].pop("arc_orders", None)
            case["result"].pop("place_names", None)
        return json.dumps(case, sort_keys=True)

    for run in runs[1:]:
        for a, b in zip(first, run):
            if a["kind"] in DETERMINISTIC and comparable(a) != comparable(b):
                raise SystemExit(f"{a['name']} varies across hash seeds")
    with gzip.open(out, "wt") as handle:
        json.dump({"reference": "pm4py 2.7.22", "seed": SEED, "cases": first}, handle, sort_keys=True)
    kinds = {}
    for case in first:
        kinds[case["kind"]] = kinds.get(case["kind"], 0) + 1
    print(out, kinds)


if __name__ == "__main__":
    if sys.argv[1] == "--merge":
        merge(sys.argv[2:-1], sys.argv[-1])
    else:
        REF = load_reference()
        if sys.argv[1] == "--replay":
            replay(sys.argv[2], sys.argv[3])
        else:
            cases = capture()
            for case in cases:
                if case["kind"] in DETERMINISTIC:
                    case["result"] = evaluate(case)
            json.dump(cases, open(sys.argv[1], "w"))
