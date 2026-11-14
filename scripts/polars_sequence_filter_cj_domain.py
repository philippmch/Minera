"""Round CJ: the Polars sequence filter domain with dict activity values.

Loads scripts/polars_sequence_filter_ch_domain.py (typed, temporal and odd
float activity columns). In about 60% of the cases the activity values of
the value calls (start/end activities, traces, starts_with, ends_with,
prefixes, suffixes, rework.apply_activity_set, activity_split) gain dicts:
a dict in place of the whole value, a dict among the items, a list of
dicts, a dict nested in a list, an empty dict, or a dict next to a value of
another type. Dict fields hold ints, floats, strs, None, nested lists and
nested dicts."""
import importlib.util
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("polars_sequence_filter_ch_domain",
                                               HERE / "polars_sequence_filter_ch_domain.py")
ch = importlib.util.module_from_spec(_spec)
sys.modules["polars_sequence_filter_ch_domain"] = ch
_spec.loader.exec_module(ch)
cc = ch.cc
cb = cc.cb
codec = ch.codec
S, I, F, B, N = codec.S, codec.I, codec.F, codec.B, codec.N


def field(r, depth):
    pick = r.random()
    if pick < 0.3:
        return I(r.choice((0, 1, 2, -1, 40)))
    if pick < 0.45:
        return F(r.choice((0.5, 1.0, 1e-5, 1e16, 0.1)))
    if pick < 0.65:
        return S(r.choice(("a", "b", "", "1")))
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
        fields.append([S(r.choice(("a", "b", "x"))), field(r, depth)])
    return {"m": fields}


def inject(r, node, depth=0):
    """A copy of an activity argument with a dict put in somewhere."""
    (tag, value), = node.items()
    if tag in ("l", "t", "S"):
        items = list(value)
        pick = r.random()
        if items and pick < 0.35 and depth < 2:
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
        return {"t" if tag == "t" else "l": items}
    pick = r.random()
    if pick < 0.6:
        return dict_node(r)
    if pick < 0.8:
        return {"l": [dict_node(r)]}
    return {"l": [node, dict_node(r)] if r.random() < 0.5 else [dict_node(r), node]}


_ch_generate = ch.generate_cases if hasattr(ch, "generate_cases") else cc.generate_cases


def generate_cases(count, bank_seed):
    cases = _ch_generate(count, bank_seed)
    rng = random.Random(bank_seed ^ 0xCA)
    for case in cases:
        r = random.Random(rng.randrange(1 << 30))
        if r.random() >= 0.6:
            continue
        for call in case["calls"]:
            if call["function"] in cb.VALUE_FUNCTIONS and call["args"] and "m" not in call["args"][0] \
                    and r.random() < 0.7:
                call["args"][0] = inject(r, call["args"][0])
    return cases


for _k in dir(ch):
    if not _k.startswith("__"):
        globals().setdefault(_k, getattr(ch, _k))
globals()["generate_cases"] = generate_cases
