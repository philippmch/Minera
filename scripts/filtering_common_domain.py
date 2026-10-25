"""Seeded public-call domain for the common and DFG filtering helpers.

Covers every public callable of

  algo.filtering.common.filtering_constants
  algo.filtering.common.attributes.attributes_common
  algo.filtering.common.timestamp.timestamp_common
  algo.filtering.dfg.dfg_filtering

with generated arguments. Arguments and results travel in one typed JSON form:

  {"s": text} {"i": "decimal"} {"f": "hex of the IEEE double"} {"b": bool}
  {"n": null} {"d": "YYYY-MM-DD HH:MM:SS[.ffffff]+00:00"} (aware UTC datetime)
  {"w": ["YYYY-MM-DD HH:MM:SS.ffffff", offset minutes or null]} (datetime
  argument: wall clock plus its fixed offset, null for naive)
  {"l": [...]} {"t": [...]} {"m": [[key, value], ...]} (list, tuple, dict)
  {"S": [...]} (set, members sorted by their JSON text)
  {"U": [[key, value], ...]} (dict whose order is not part of the contract,
  sorted by key JSON text) {"ns": ["int", ...]} (DatetimeIndex values)
  {"g": [nodes, edges]} (networkx DiGraph, start and end nodes as "<S>"/"<E>")

Domain: activity names never equal the reserved "_S_START_"/"_S_END_" node
names, DFG keys are string pairs and counts are integers, KDE values are
finite-magnitude ints/floats (non-finite ones included) and datetimes are
ASCII-free of exotic digits. Parameters travel as [[key, value], ...] where
key is either an enum member name ("enum:GRAPH_POINTS") or a plain string.
"""
import datetime
import json
import random
import re
import struct

NAMES = ("a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "été", "Z")


def fbits(value):
    if value != value:
        return "7ff8000000000000"
    return struct.pack(">d", value).hex()


def text(node):
    return json.dumps(node, sort_keys=True, separators=(",", ":"))


def wall(value):
    off = value.utcoffset()
    return {"w": [value.replace(tzinfo=None).isoformat(sep=" ", timespec="microseconds"),
                  None if off is None else int(off.total_seconds() // 60)]}


def encode(value, as_arg=False):
    import numpy as np
    import pandas as pd
    if value is None:
        return {"n": None}
    if isinstance(value, (bool, np.bool_)):
        return {"b": bool(value)}
    if isinstance(value, (int, np.integer)):
        return {"i": str(int(value))}
    if isinstance(value, (float, np.floating)):
        return {"f": fbits(float(value))}
    if isinstance(value, str):
        return {"s": value}
    if isinstance(value, pd.DatetimeIndex):
        return {"ns": [str(int(v)) for v in value.asi8]}
    if isinstance(value, datetime.datetime):
        if value.tzinfo is None or as_arg:
            return wall(value)
        return {"d": value.astimezone(datetime.timezone.utc).isoformat(sep=" ")}
    if isinstance(value, np.ndarray):
        return {"l": [encode(v, as_arg) for v in value.tolist()]}
    if isinstance(value, list):
        return {"l": [encode(v, as_arg) for v in value]}
    if isinstance(value, tuple):
        return {"t": [encode(v, as_arg) for v in value]}
    if isinstance(value, (set, frozenset)):
        return {"S": sorted((encode(v, as_arg) for v in value), key=text)}
    if isinstance(value, dict):
        return {"m": [[encode(k, as_arg), encode(v, as_arg)] for k, v in value.items()]}
    raise TypeError(f"unrepresentable {type(value).__name__}")


def decode(node):
    (tag, value), = node.items()
    if tag == "n":
        return None
    if tag in ("b", "s"):
        return value
    if tag == "i":
        return int(value)
    if tag == "f":
        return struct.unpack(">d", bytes.fromhex(value))[0]
    if tag == "w":
        stamp, offset = value
        dt = datetime.datetime.fromisoformat(stamp)
        if offset is not None:
            dt = dt.replace(tzinfo=datetime.timezone(datetime.timedelta(minutes=offset)))
        return dt
    if tag == "l":
        return [decode(v) for v in value]
    if tag == "t":
        return tuple(decode(v) for v in value)
    if tag == "S":
        return {decode(v) for v in value}
    if tag == "m":
        return {decode(k): decode(v) for k, v in value}
    raise ValueError(tag)


def S(v): return {"s": v}
def I(v): return {"i": str(v)}
def F(v): return {"f": fbits(v)}
def B(v): return {"b": v}
def N(): return {"n": None}


def unordered(mapping):
    return {"U": sorted(([encode(k), encode(v)] for k, v in mapping.items()), key=lambda kv: text(kv[0]))}


# ---------------------------------------------------------------- arguments

def number(rng, choices):
    roll = rng.random()
    return choices[int(roll * len(choices))]


def dfg_case(rng, consistent):
    n = rng.randint(0, 7)
    names = rng.sample(NAMES, n)
    act = {a: (rng.randint(0, 6) if rng.random() < 0.3 else rng.randint(1, 40)) for a in names}
    dfg = {}
    for _ in range(rng.randint(0, n * n)):
        a, b = rng.choice(names), rng.choice(names)
        dfg[(a, b)] = rng.randint(0, 12) if rng.random() < 0.1 else rng.randint(1, 12)
    sa = {a: rng.randint(1, 9) for a in names if rng.random() < 0.35}
    ea = {a: rng.randint(1, 9) for a in names if rng.random() < 0.35}
    if n and not sa and rng.random() < 0.8:
        sa[rng.choice(names)] = rng.randint(1, 9)
    if n and not ea and rng.random() < 0.8:
        ea[rng.choice(names)] = rng.randint(1, 9)
    if rng.random() < 0.15:
        for key in rng.sample(list(dfg), min(len(dfg), 2)):
            dfg[key] = rng.choice([-3, 0, 10 ** 6, 2 ** 40])
    if not consistent:
        if rng.random() < 0.3 and n:
            del act[rng.choice(names)]
        if rng.random() < 0.25:
            dfg[("z", rng.choice(names or ["y"]))] = 2
        if rng.random() < 0.15:
            dfg[(rng.choice(names or ["y"]), "x")] = 3
        if rng.random() < 0.2:
            sa["q"] = 1
        if rng.random() < 0.2:
            ea["w"] = 1
    return dfg, sa, ea, act


def fraction(rng):
    return rng.choice([0, 1, 0.0, 0.05, 0.1, 0.2, 0.25, 0.3, 0.4, 0.5, 0.6, 0.75, 0.8, 0.9, 0.95, 1.0,
                       1.3, -0.5, -1, 2, float("nan"), float("inf"), -float("inf"), 1e300, 0.999999,
                       0.5000001, rng.random(), rng.uniform(-2, 2)])


def dfg_calls(rng):
    consistent = rng.random() < 0.6
    dfg, sa, ea, act = dfg_case(rng, consistent)
    args = [encode(dfg), encode(sa), encode(ea), encode(act)]
    calls = []
    kind = rng.random()
    calls.append(("filter_dfg_on_activities_percentage", args + [encode(fraction(rng))]))
    keep_all = [] if rng.random() < 0.3 else [B(rng.random() < 0.5)]
    calls.append(("filter_dfg_on_paths_percentage", args + [encode(fraction(rng))] + keep_all))
    keep_all = [] if rng.random() < 0.3 else [B(rng.random() < 0.5)]
    calls.append(("filter_dfg_keep_connected", args + [encode(fraction(rng))] + keep_all))
    names = sorted(set(act) | set(sa) | set(ea) | {n for k in dfg for n in k}) + ["zz"]
    for fn in ("filter_dfg_to_activity", "filter_dfg_from_activity", "filter_dfg_contain_activity"):
        extra = rng.choice([[], [N()], [{"m": []}], [{"m": [[S("x"), I(1)]]}]])
        calls.append((fn, args + [S(rng.choice(names))] + extra))
    # noise cleaning: activities in several container shapes
    shape = rng.random()
    acts = list(act) if consistent or rng.random() < 0.5 else list(act)[: max(0, len(act) - 1)]
    if shape < 0.3:
        acts_arg = encode({a: act.get(a, 1) for a in acts})
    elif shape < 0.6:
        acts_arg = encode(list(acts))
    elif shape < 0.8:
        acts_arg = encode(tuple(reversed(acts)))
    else:
        acts_arg = encode(set(acts) | {"extra"})
    noise = rng.choice([0, 0.05, 0.16, 0.3, 0.5, 0.9, 1, 2, -0.3, float("inf"), -float("inf"),
                        float("nan"), rng.random()])
    keys = list(dfg)
    roll = rng.random()
    if roll < 0.2:
        params = []
    elif roll < 0.3:
        params = [N()]
    elif roll < 0.4:
        params = [{"m": []}]
    elif roll < 0.5:
        params = [{"m": [[S("most_common_paths"), N()]]}]
    else:
        chosen = rng.sample(keys, min(len(keys), rng.randint(0, 3)))
        if rng.random() < 0.2:
            chosen.append(("q", "r"))
        container = rng.choice([list, tuple, set])
        params = [{"m": [[S("most_common_paths"), encode(container(chosen))]]}]
    calls.append(("clean_dfg_based_on_noise_thresh", [args[0], acts_arg, encode(noise)] + params))
    calls.append(("build_adjacency_structures", args[:3]))
    calls.append(("generate_nx_graph_from_dfg", args))
    # reachability helpers over a built adjacency, sometimes perturbed
    adj, rev = build_adjacency(dfg, sa, ea)
    start, end = "_S_START_", "_S_END_"
    if rng.random() < 0.3:
        victim = rng.choice(sorted(adj))
        target = rng.choice([adj, rev])
        del target[victim]
    if rng.random() < 0.15:
        start, end = rng.choice([(start, end), ("a", end), (start, "b"), ("nope", end)])
    adj_arg = adjacency_arg(rng, adj)
    rev_arg = adjacency_arg(rng, rev)
    calls.append(("bfs_reachable", [S(start), adj_arg]))
    calls.append(("bfs_reachable", [S(end), rev_arg]))
    calls.append(("remove_unreachable_nodes", args + [adj_arg, rev_arg, S(start), S(end)]))
    return calls


def build_adjacency(dfg, sa, ea):
    nodes = set()
    for a, b in dfg:
        nodes.add(a)
        nodes.add(b)
    nodes |= set(sa) | set(ea)
    adj = {n: set() for n in nodes}
    rev = {n: set() for n in nodes}
    for a, b in dfg:
        adj[a].add(b)
        rev[b].add(a)
    adj["_S_START_"] = set()
    rev["_S_START_"] = set()
    adj["_S_END_"] = set()
    rev["_S_END_"] = set()
    for a in sa:
        adj["_S_START_"].add(a)
        rev[a].add("_S_START_")
    for a in ea:
        adj[a].add("_S_END_")
        rev["_S_END_"].add(a)
    return adj, rev


def adjacency_arg(rng, adjacency):
    """Adjacency as a dict of sets, lists or tuples (ordered by sorted node name)."""
    out = []
    for key in sorted(adjacency):
        members = sorted(adjacency[key])
        container = rng.choice([set, set, list, tuple])
        out.append([S(key), encode(container(members))])
    return {"m": out}


def bfs_calls(rng):
    nodes = rng.sample(NAMES, rng.randint(1, 6))
    adj = {}
    for n in nodes:
        if rng.random() < 0.85:
            succ = [m for m in nodes if rng.random() < 0.35]
            if rng.random() < 0.1:
                succ.append("ghost")
            adj[n] = succ
    return [("bfs_reachable", [S(rng.choice(nodes + ["ghost"])), adjacency_arg(rng, adj)])]


def kde_values(rng):
    roll = rng.random()
    n = rng.choice([0, 1, 1, 2, 2, 3, 4, 5, 6, 8, 10, 15, 30, 60])
    if roll < 0.15:
        base = rng.choice([0, 3, -2, 1e-5, -4e-5, 7.5, 10 ** 6])
        return [base] * max(n, 1)
    kind = rng.random()
    vals = []
    for _ in range(n):
        if kind < 0.3:
            vals.append(rng.randint(-20, 40) if rng.random() < 0.5 else rng.randint(1, 50))
        elif kind < 0.55:
            vals.append(rng.uniform(0.01, 100))
        elif kind < 0.7:
            vals.append(-rng.uniform(0.01, 100))
        elif kind < 0.8:
            vals.append(rng.uniform(-5, 5))
        elif kind < 0.9:
            vals.append(rng.choice([1, 2, 2, 3, 5, 5, 5, 8]))
        else:
            vals.append(rng.uniform(1e-6, 1e-3) * rng.choice([1, -1]))
    if vals and rng.random() < 0.05:
        vals[rng.randrange(len(vals))] = rng.choice([float("nan"), float("inf"), -float("inf")])
    if vals and rng.random() < 0.3:
        rng.shuffle(vals)
    return vals


def kde_params(rng, with_sample):
    roll = rng.random()
    if roll < 0.15:
        return []
    if roll < 0.25:
        return [N()]
    entries = []
    if rng.random() < 0.8:
        key = S("graph_points") if rng.random() < 0.4 else S("enum:GRAPH_POINTS")
        entries.append([key, I(rng.choice([200, 1, 0, 2, 3, 4, 5, 6, 7, 9, 13, 20, 50, -1, -4, rng.randint(1, 60)]))])
    if with_sample and rng.random() < 0.7:
        key = S("points_to_sample") if rng.random() < 0.4 else S("enum:POINT_TO_SAMPLE")
        entries.append([key, I(rng.choice([400, 1, 2, 3, 4, 5, 7, 10, 25, 100, 0, -2, rng.randint(1, 80)]))])
    return [{"m": entries}]


def kde_dates(rng):
    n = rng.choice([0, 1, 1, 2, 2, 3, 4, 5, 6, 8, 12, 20, 40])
    roll = rng.random()
    if roll < 0.15:
        base = datetime.datetime(2021, 3, 28, 2, 30) if rng.random() < 0.3 else datetime.datetime(2020, 6, 1, 12)
        return [base] * max(n, 1)
    if roll < 0.3:
        base = rng.choice([datetime.datetime(2021, 3, 28, 1, 0), datetime.datetime(2021, 10, 31, 1, 30)])
        span = 3 * 3600
    else:
        base = datetime.datetime(2019 + rng.randint(0, 3), rng.randint(1, 12), rng.randint(1, 28))
        span = rng.choice([60, 3600, 86400, 30 * 86400, 400 * 86400])
    zones = [None, None, datetime.timezone.utc, datetime.timezone(datetime.timedelta(hours=2)),
             datetime.timezone(datetime.timedelta(hours=-5, minutes=-30))]
    vals = []
    for _ in range(n):
        dt = base + datetime.timedelta(seconds=rng.randint(0, span),
                                       microseconds=rng.randint(0, 999999) if rng.random() < 0.4 else 0)
        zone = rng.choice(zones)
        vals.append(dt if zone is None else dt.replace(tzinfo=zone))
    return vals


def attributes_calls(rng):
    calls = []
    vals = kde_values(rng)
    for fn in ("get_kde_numeric_attribute", "get_kde_numeric_attribute_json"):
        calls.append((fn, [encode(vals)] + kde_params(rng, False)))
    dates = kde_dates(rng)
    for fn in ("get_kde_date_attribute", "get_kde_date_attribute_json"):
        calls.append((fn, [encode(dates, True)] + kde_params(rng, True)))
    n = rng.randint(0, 9)
    counts = {}
    for name in rng.sample(NAMES, min(n, len(NAMES))):
        counts[name] = rng.choice([rng.randint(0, 30), rng.randint(0, 4), rng.uniform(0, 30)]) \
            if rng.random() < 0.2 else rng.randint(0, 30)
    calls.append(("get_sorted_attributes_list", [encode(counts)]))
    alist = [(k, v) for k, v in counts.items()]
    if rng.random() < 0.6:
        alist = sorted(alist, key=lambda x: x[1], reverse=True)
    factor = rng.choice([0.6, 0.6, 0, 1, 0.3, 0.9, 1.5, -1, float("nan"), float("inf"), rng.random()])
    extra = rng.choice([[], [I(rng.randint(-3, 10))], [I(rng.randint(-3, 10)), I(rng.randint(-3, 10))]])
    calls.append(("get_attributes_threshold", [encode(alist), encode(factor)] + extra))
    return calls


def dt_string(rng):
    y, m, d = rng.randint(1, 9999), rng.randint(1, 12), rng.randint(1, 28)
    H, M, s = rng.randint(0, 23), rng.randint(0, 59), rng.randint(0, 59)
    roll = rng.random()
    if roll < 0.35:
        return f"{y:04d}-{m:02d}-{d:02d} {H:02d}:{M:02d}:{s:02d}"
    if roll < 0.45:
        return f"{y:04d}-{m}-{d} {H}:{M}:{s}"
    pieces = [f"{y:04d}", "-", f"{m:02d}", "-", f"{d:02d}", " ", f"{H:02d}", ":", f"{M:02d}", ":", f"{s:02d}"]
    mutate = rng.randrange(len(pieces))
    pieces[mutate] = rng.choice(["", "0", "00", "1", "9", "13", "24", "29", "30", "31", "32", "59", "60",
                                 "61", "62", "99", "123", "0000", "02020", " 5", "x", "T", "  ", "\t",
                                 "\n", "\x1c", "-", ":", "1a", "0x"])
    tail = rng.choice(["", "", "", "", " ", "Z", ".123", "+01:00", "\n", "x"])
    head = rng.choice(["", "", "", "", "", " ", "\t", "x"])
    return head + "".join(pieces) + tail


def timestamp_calls(rng):
    roll = rng.random()
    if roll < 0.75:
        return [("get_dt_from_string", [S(dt_string(rng))])]
    if roll < 0.9:
        dt = datetime.datetime(rng.randint(1, 9999), rng.randint(1, 12), rng.randint(1, 28), rng.randint(0, 23),
                               rng.randint(0, 59), rng.randint(0, 59),
                               rng.randint(0, 999999) if rng.random() < 0.4 else 0)
        zone = rng.choice([None, datetime.timezone.utc, datetime.timezone(datetime.timedelta(hours=3))])
        return [("get_dt_from_string", [encode(dt if zone is None else dt.replace(tzinfo=zone), True)])]
    return [("get_dt_from_string", [rng.choice([I(5), N(), F(1.5), B(True), {"l": []}])])]


def generate_cases(count, bank_seed):
    rng = random.Random(bank_seed)
    cases = []
    for index in range(count):
        seed = rng.randrange(1 << 30)
        r = random.Random(seed)
        roll = r.random()
        calls = []
        if index == 0:
            calls += [("filtering_constants." + name, []) for name in
                      ("CASE_CONCEPT_NAME", "DECREASING_FACTOR", "DEFAULT_NOISE_THRESH_DF")]
            calls.append(("dfg_filtering.DEFAULT_NOISE_THRESH_DF", []))
        if roll < 0.5:
            calls += [("dfg_filtering." + fn, args) for fn, args in dfg_calls(r)]
        elif roll < 0.55:
            calls += [("dfg_filtering." + fn, args) for fn, args in bfs_calls(r)]
        elif roll < 0.85:
            calls += [("attributes_common." + fn, args) for fn, args in attributes_calls(r)]
        else:
            for _ in range(4):
                calls += [("timestamp_common." + fn, args) for fn, args in timestamp_calls(r)]
        cases.append({"seed": seed, "calls": [{"function": fn, "args": args} for fn, args in calls]})
    return cases


# ---------------------------------------------------------------- execution

MODULES = {
    "filtering_constants": "pm4py.algo.filtering.common.filtering_constants",
    "attributes_common": "pm4py.algo.filtering.common.attributes.attributes_common",
    "timestamp_common": "pm4py.algo.filtering.common.timestamp.timestamp_common",
    "dfg_filtering": "pm4py.algo.filtering.dfg.dfg_filtering",
}

UUID4 = re.compile(r"^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$")


def argument(node):
    """Decodes an argument; parameter dicts turn "enum:X" keys into members."""
    from pm4py.statistics.attributes.common.get import Parameters
    value = decode(node)
    if isinstance(value, dict):
        out = {}
        for key, item in value.items():
            if isinstance(key, str) and key.startswith("enum:"):
                key = getattr(Parameters, key[5:])
            out[key] = item
        return out
    return value


def result_node(function, value):
    if function == "dfg_filtering.build_adjacency_structures":
        adj, rev, start, end = value
        return {"t": [{"U": [[S(k), encode(v)] for k, v in sorted(adj.items())]},
                      {"U": [[S(k), encode(v)] for k, v in sorted(rev.items())]}, S(start), S(end)]}
    if function == "dfg_filtering.generate_nx_graph_from_dfg":
        graph, start, end = value
        name = lambda n: "<S>" if n == start else "<E>" if n == end else n
        nodes = [S(name(n)) for n in graph.nodes]
        edges = [{"t": [S(name(a)), S(name(b))]} for a, b in graph.edges]
        attrs = any(graph.nodes[n] for n in graph.nodes) or any(graph.edges[e] for e in graph.edges)
        ok = bool(UUID4.match(start)) and bool(UUID4.match(end)) and start != end and not attrs
        return {"g": [nodes, edges, ok]}
    return encode(value)


def run_call(call):
    import importlib
    function = call["function"]
    module_name, name = function.split(".")
    module = importlib.import_module(MODULES[module_name])
    target = getattr(module, name)
    if not callable(target):
        return {"value": encode(target)}
    args = [argument(a) for a in call["args"]]
    before = [repr(a) for a in args]
    try:
        value = target(*args)
    except Exception as error:  # noqa: BLE001 - every public outcome is evidence
        outcome = {"error": [type(error).__name__, str(error)]}
    else:
        try:
            outcome = {"value": result_node(function, value)}
        except TypeError as error:
            outcome = {"unrepresentable": str(error)}
    changed = [i for i, a in enumerate(args) if repr(a) != before[i]]
    if changed:
        outcome["mutated"] = True
        outcome["after"] = [[i, encode(args[i])] for i in changed]
    return outcome
