"""Round CK: the pandas table filter domain with dict values.

Builds the frames and calls of scripts/pandas_filter_domain.py. In about
70% of the cases some calls gain dicts where the base domain passes plain
values: a dict in place of a whole positional or keyword argument, a dict
among the items of a list or tuple argument, a list of dicts, a dict nested
in a list item, an empty dict, or a dict as a parameter value (column keys,
bounds, stream filter values, boundaries, flags). Dict fields hold ints,
floats, strs, None, bools, a nested list or a nested dict. The parameters
mapping itself stays a mapping (a dict there is the mapping)."""
import importlib.util
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("pandas_filter_domain", HERE / "pandas_filter_domain.py")
base = importlib.util.module_from_spec(_spec)
sys.modules["pandas_filter_domain"] = base
_spec.loader.exec_module(base)
codec = base.codec
S, I, F, B, N = codec.S, codec.I, codec.F, codec.B, codec.N

# Positional arguments that are the parameters mapping (index per function).
PARAMS_AT = {
    "attributes.apply": 1, "attributes.apply_events": 1, "attributes.apply_numeric": 2,
    "attributes.apply_numeric_events": 2, "attributes.filter_df_relative_occurrence_event_attribute": 1,
    "between.apply": 2, "cases.filter_case_performance": 2, "cases.apply": 0, "cases.apply_auto_filter": 0,
    "ltl.eventually_follows": 1, "ltl.A_next_B_next_C": 3, "ltl.four_eyes_principle": 2,
    "ltl.attr_value_different_persons": 1, "paths.apply": 1, "paths.apply_performance": 1,
    "timestamp.filter_traces_contained": 2, "timestamp.filter_traces_intersecting": 2,
    "timestamp.apply_events": 2, "timestamp.filter_traces_starting_in_timeframe": 2,
    "timestamp.filter_traces_completing_in_timeframe": 2, "timestamp.filter_traces_attribute_in_timeframe": 4,
    "timestamp.apply": 0, "timestamp.apply_auto_filter": 0, "variants.apply": 1,
    "variants.filter_variants_top_k": 1, "variants.filter_variants_by_coverage_percentage": 1,
    "variants.filter_variants_by_maximum_coverage_percentage": 1,
}


def field(r, depth):
    pick = r.random()
    if pick < 0.3:
        return I(r.choice((0, 1, 2, -1, 60, 3600)))
    if pick < 0.42:
        return F(r.choice((0.5, 1.0, 1e-5, 1e16, 0.1)))
    if pick < 0.65:
        return S(r.choice(("a", "b", "", "1", "r1", "x")))
    if pick < 0.72:
        return N()
    if pick < 0.8:
        return B(r.random() < 0.5)
    if pick < 0.9 and depth < 1:
        return {"l": [field(r, depth + 1) for _ in range(r.choice((0, 1, 2)))]}
    if depth < 1:
        return dict_node(r, depth + 1)
    return I(1)


def dict_node(r, depth=0):
    fields = []
    for _ in range(r.choice((0, 1, 1, 1, 2, 2))):
        key = r.choice(("a", "b", "x", "concept:name", "r1"))
        if any(k["s"] == key for k, _ in fields):
            continue
        fields.append([S(key), field(r, depth)])
    return {"m": fields}


def inject(r, node, depth=0):
    """A copy of an argument with a dict put in somewhere."""
    (tag, value), = node.items()
    if tag in ("l", "t") and depth < 2:
        items = list(value)
        pick = r.random()
        if items and pick < 0.35:
            k = r.randrange(len(items))
            items[k] = inject(r, items[k], depth + 1)
        elif pick < 0.6 or not items:
            items.insert(r.randint(0, len(items)), dict_node(r))
        elif pick < 0.75:
            items = [dict_node(r) for _ in range(r.choice((1, 1, 2)))]
        elif pick < 0.85:
            items.append({"l": [dict_node(r)]})
        else:
            items[r.randrange(len(items))] = dict_node(r)
        return {tag: items}
    pick = r.random()
    if pick < 0.65:
        return dict_node(r)
    if pick < 0.85:
        return {"l": [dict_node(r)]}
    return {"l": [node, dict_node(r)] if r.random() < 0.5 else [dict_node(r), node]}


def inject_params(r, node):
    """A parameters mapping with one value made a dict (or a dict-valued key added)."""
    if "m" not in node:
        return node
    entries = [list(e) for e in node["m"]]
    if entries and r.random() < 0.75:
        k = r.randrange(len(entries))
        entries[k][1] = inject(r, entries[k][1])
    else:
        key = r.choice(("pm4py:param:case_id_key", "positive", "pm4py:param:activity_key",
                        "pm4py:param:timestamp_key", "pm4py:param:attribute_key", "keep_once_per_case",
                        "keep_nan_values"))
        entries.append([S(key), dict_node(r)])
    return {"m": entries}


def mutate(r, call):
    function = call["function"]
    args = [dict(a) for a in call["args"]]
    kwargs = [list(k) for k in call["kwargs"]]
    at = PARAMS_AT.get(function)
    slots = [("a", i) for i in range(len(args))] + [("k", i) for i in range(len(kwargs))]
    if not slots:
        return call
    for _ in range(r.choice((1, 1, 1, 2))):
        kind, i = r.choice(slots)
        if kind == "a":
            if i == at:
                args[i] = inject_params(r, args[i])
            else:
                args[i] = inject(r, args[i])
        else:
            kwargs[i][1] = inject(r, kwargs[i][1])
    return {"function": function, "args": args, "kwargs": kwargs}


def generate_cases(count, bank_seed):
    cases = base.generate_cases(count, bank_seed)
    rng = random.Random(bank_seed ^ 0xC6)
    for case in cases:
        r = random.Random(rng.randrange(1 << 30))
        if r.random() >= 0.7:
            continue
        case["calls"] = [mutate(r, call) if r.random() < 0.5 else call for call in case["calls"]]
    return cases


for _k in dir(base):
    if not _k.startswith("__"):
        globals().setdefault(_k, getattr(base, _k))
globals()["generate_cases"] = generate_cases
