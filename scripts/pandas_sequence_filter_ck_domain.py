"""Round CK: the pandas sequence filter domain with dict values.

Loads scripts/pandas_sequence_filter_cc_domain.py (typed, integer and
datetime64 keys). In about 60% of the cases the calls gain dicts: the
activity values of the value calls (start/end activities, traces,
starts_with, ends_with, prefixes, suffixes, rework, activity_split) get a
dict in place of the whole value, a dict among the items, a list of dicts,
a dict nested in a list or a dict next to a value of another type; and a
parameter value (column keys, positive, min_occurrences, first_or_last,
strict, filter_type, cut_mode, the count dicts and thresholds of the *_nocc
calls, including the timestamp and consecutive-activity case grouping
parameters) may become a dict. Dict fields hold ints, floats, strs, None,
bools, a nested list or a nested dict."""
import importlib.util
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("pandas_sequence_filter_cc_domain",
                                               HERE / "pandas_sequence_filter_cc_domain.py")
cc = importlib.util.module_from_spec(_spec)
sys.modules["pandas_sequence_filter_cc_domain"] = cc
_spec.loader.exec_module(cc)
cb = cc.cb
base = cc.base
codec = base.codec
S, I, F, B, N = codec.S, codec.I, codec.F, codec.B, codec.N

_dspec = importlib.util.spec_from_file_location("pandas_filter_ck_domain", HERE / "pandas_filter_ck_domain.py")
tbl = importlib.util.module_from_spec(_dspec)
_dspec.loader.exec_module(tbl)
dict_node = tbl.dict_node
inject = tbl.inject


def inject_params(r, node):
    """A parameters mapping with one value made a dict."""
    if "m" not in node or not node["m"]:
        return node
    entries = [list(e) for e in node["m"]]
    k = r.randrange(len(entries))
    entries[k][1] = inject(r, entries[k][1])
    return {"m": entries}


def mutate(r, call):
    function = call["function"]
    args = [dict(a) for a in call["args"]]
    kwargs = [list(k) for k in call["kwargs"]]
    if function in cb.VALUE_FUNCTIONS and args and "m" not in args[0] and r.random() < 0.75:
        args[0] = inject(r, args[0])
        if r.random() < 0.8:
            return {"function": function, "args": args, "kwargs": kwargs}
    pick = r.random()
    params = [i for i, a in enumerate(args) if "m" in a and (i > 0 or function not in cb.VALUE_FUNCTIONS)]
    if params and pick < 0.6:
        i = r.choice(params)
        args[i] = inject_params(r, args[i])
    elif kwargs and pick < 0.85:
        i = r.randrange(len(kwargs))
        kwargs[i][1] = inject(r, kwargs[i][1])
    elif args and "m" not in args[0]:
        args[0] = inject(r, args[0])
    return {"function": function, "args": args, "kwargs": kwargs}


_cc_generate = cc.generate_cases


def generate_cases(count, bank_seed):
    cases = _cc_generate(count, bank_seed)
    rng = random.Random(bank_seed ^ 0xC7)
    for case in cases:
        r = random.Random(rng.randrange(1 << 30))
        if r.random() >= 0.6:
            continue
        case["calls"] = [mutate(r, call) if (call["args"] or call["kwargs"]) and r.random() < 0.7 else call
                         for call in case["calls"]]
    return cases


for _k in dir(cc):
    if not _k.startswith("__"):
        globals().setdefault(_k, getattr(cc, _k))
globals()["generate_cases"] = generate_cases
