"""Round CK: the Polars sequence filter domain with dict values on
Categorical and Enum activity columns, prefix/suffix cast plans and dicts in
the parameters.

Loads scripts/polars_sequence_filter_cj_domain.py. About 55% of the frames
get a Categorical or Enum activity column (Enum categories sorted, reversed
or with an unused category), where the CJ domain mostly built temporal,
integer and float ones. On top of the CJ dict injection:

- prefixes.apply and suffixes.apply get a few patterns that mix plain lists,
  nested lists, dicts, floats, bools and None (cast failures next to struct
  patterns), on every activity kind;
- rework.apply_activity_set and traces.apply get dicts among their items in
  about a third of the cases;
- a parameter value may become a dict: the case id, activity and timestamp
  keys (CASE_ID_KEY values) of any call, FILTER_TYPE of the timestamp and
  consecutive-activity case grouping, POSITIVE and MIN_OCCURRENCES."""
import importlib.util
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("polars_sequence_filter_cj_domain",
                                               HERE / "polars_sequence_filter_cj_domain.py")
cj = importlib.util.module_from_spec(_spec)
sys.modules["polars_sequence_filter_cj_domain"] = cj
_spec.loader.exec_module(cj)
ch = cj.ch
cc = cj.cc
cb = cj.cb
typed = cb.typed
base = typed.base
codec = cj.codec
S, I, F, B, N = codec.S, codec.I, codec.F, codec.B, codec.N
dict_node = cj.dict_node

_prev_spec = base.frame_spec


def frame_spec(r):
    spec = _prev_spec(r)
    if r.random() < 0.55:
        spec["act_kind"] = r.choice(("cat", "enum", "enumx", "enum", "cat"))
    return spec


base.frame_spec = frame_spec
typed.frame_spec = frame_spec
cb.frame_spec = frame_spec
cc.base.frame_spec = frame_spec

CAST_PATTERNS = [
    [["b"], [["x"]]], [[["x"]], ["b"]], [[1], [["x"]], ["a", "b"]], [["b", "c"], [["x"]], ["d", "e", "f"], ["g"]],
    [[["x"]]], [[1.5, None], [["x"]]], [[True], [["x"]], [2]], [["b"], [["x"]], ["b"]], [["b"], ["b"], [["x"]]],
    [[{"a": 1}], [["x"]]], [[{"a": 1, "b": "x"}], [["x"]]], [[], [["x"]]], [[["x"]], [["y"]]],
    [["a", "b", "c", "d"], [["x"]]], [[["x"], ["y"], ["z"], ["w"]]], [[{"a": 1}], ["x"]], [[1.5], ["x"]],
    [["x"], [["y"]]], [[["y"]], ["x"]], [[None], [["x"]]], [[True, False], ["x", "y"]],
    [[{"a": 1}, {"a": 2}], ["x"]], [[1, None, 0, 2], ["x"]], [["b"], [], [["x"]]], [[["x"]], []],
    [[["x"]], [], ["b"]], [[], ["b"], [["x"]]], [[["x"]], [{"a": 1}]], [[["x"]], ["b"], ["b"]],
    [["b"], ["b"], [["x"]], ["c"], ["c"]], [[["x"]], [["x"]]], [[], ["x"]], [[{"a": 1, "b": 2}], [["x"]]],
    [[{"a": 1}, "q"], [["x"]]], [[["x", 1]]], [[[{"a": 1}, ""]], ["b"]], [["a"], [{"a": "a"}]],
    [[{"a": "b"}], ["a"]], [["a", {"a": 1}], ["b"]], [[1], [1.5], [["x"]]], [[True], ["a"], [["x"]]],
    [["a"], [None], [1]], [[{}], ["a"]], [[{"a": None}], ["b"], [["x"]]],
]

KEY_PARAMS = ("pm4py:param:case_id_key", "pm4py:param:activity_key", "pm4py:param:timestamp_key")


def cast_calls(r):
    calls = []
    for _ in range(r.choice((0, 1, 2, 3))):
        p = r.choice(CAST_PATTERNS)
        fn = r.choice(("prefixes.apply", "suffixes.apply"))
        calls.append({"function": fn, "args": [codec.encode(p), {"m": []} if r.random() < 0.7 else N()],
                      "kwargs": []})
    return calls


def with_dict_param(r, call):
    """A call with one parameter value (or a new key parameter) made a dict."""
    args = [dict(a) for a in call["args"]]
    at = [i for i, a in enumerate(args) if "m" in a and (i > 0 or call["function"] not in cb.VALUE_FUNCTIONS)]
    at += [i for i, a in enumerate(args) if "n" in a and i == len(args) - 1]
    if not at:
        return call
    i = r.choice(at)
    entries = [list(e) for e in args[i].get("m", [])]
    if entries and r.random() < 0.5:
        k = r.randrange(len(entries))
        entries[k][1] = r.choice((dict_node(r), {"l": [dict_node(r)]}, dict_node(r)))
    else:
        entries.append([S(r.choice(KEY_PARAMS + ("filter_type", "positive", "min_occurrences"))), dict_node(r)])
    args[i] = {"m": entries}
    return dict(call, args=args)


def with_dict_items(r, call):
    args = [dict(a) for a in call["args"]]
    (tag, value), = args[0].items()
    if tag in ("l", "S", "t"):
        items = list(value)
        items.insert(r.randint(0, len(items)), dict_node(r))
        args[0] = {"l" if tag == "S" else tag: items}
    return dict(call, args=args)


_cj_generate = cj.generate_cases


def generate_cases(count, bank_seed):
    cases = _cj_generate(count, bank_seed)
    rng = random.Random(bank_seed ^ 0xCB)
    for case in cases:
        r = random.Random(rng.randrange(1 << 30))
        calls = []
        for call in case["calls"]:
            fn = call["function"]
            if fn in ("rework.apply_activity_set", "traces.apply") and call["args"] and r.random() < 0.35:
                call = with_dict_items(r, call)
            if call["args"] and r.random() < (0.5 if "case_grouping" in fn else 0.12):
                call = with_dict_param(r, call)
            calls.append(call)
        if case["spec"]["n"] > 0:
            calls += cast_calls(r)
        case["calls"] = calls
    return cases


for _k in dir(cj):
    if not _k.startswith("__"):
        globals().setdefault(_k, getattr(cj, _k))
globals()["generate_cases"] = generate_cases
globals()["frame_spec"] = frame_spec
