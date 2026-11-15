"""Round CN: Polars table filter calls with list and tuple case keys (built
strictly as Series literals before pl.col reads them), None items in pl.col
lists, and dict or list timestamp keys the sorts read as struct or list
literals (paths, variants, eventually_follows). Explicit calls on frames
of the base domain."""
import importlib.util, struct, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("polars_filter_domain", HERE / "polars_filter_domain.py")
base = importlib.util.module_from_spec(_spec)
sys.modules["polars_filter_domain"] = base
_spec.loader.exec_module(base)


def enc(v):
    if v is None: return {"n": None}
    if isinstance(v, bool): return {"b": v}
    if isinstance(v, int): return {"i": str(v)}
    if isinstance(v, float): return {"f": struct.pack(">d", v).hex()}
    if isinstance(v, str): return {"s": v}
    if isinstance(v, tuple): return {"t": [enc(x) for x in v]}
    if isinstance(v, list): return {"l": [enc(x) for x in v]}
    if isinstance(v, dict): return {"m": [[enc(k), enc(x)] for k, x in v.items()]}
    raise TypeError(v)


def call(fn, args, p=None):
    params = {"m": [[{"s": k}, enc(v)] for k, v in (p or {}).items()]}
    return {"function": fn, "args": [enc(x) for x in args] + [params], "kwargs": []}


def _groups():
    groups = []
    # probe q1
    L = ["case:concept:name", {"b": 1}]
    PROBES = [(0, [
     call("ltl.four_eyes_principle", [["a", "b"]], {"enum:RESOURCE_KEY": 5, "enum:CASE_ID_KEY": None}),
     call("ltl.four_eyes_principle", [["a", "b"]], {"enum:RESOURCE_KEY": ["x"], "enum:CASE_ID_KEY": None}),
     call("ltl.four_eyes_principle", [["a", "b"]], {"enum:RESOURCE_KEY": 5, "enum:CASE_ID_KEY": L}),
     call("ltl.four_eyes_principle", [[[1, "x"], "b"]], {"enum:RESOURCE_KEY": 5}),
     call("ltl.four_eyes_principle", [["a", "b"]], {"enum:CASE_ID_KEY": L, "enum:ATTRIBUTE_KEY": {"x": 1}}),
     call("ltl.attr_value_different_persons", ["a"], {"enum:RESOURCE_KEY": 5, "enum:CASE_ID_KEY": None}),
     call("ltl.attr_value_different_persons", ["a"], {"enum:RESOURCE_KEY": ["x"], "enum:CASE_ID_KEY": L}),
     call("ltl.eventually_follows", [["a", "b"]], {"enum:ATTRIBUTE_KEY": 5, "enum:CASE_ID_KEY": L}),
     call("ltl.A_next_B_next_C", [["a", "b", "c"]], {"enum:ATTRIBUTE_KEY": None, "enum:CASE_ID_KEY": L}),
     call("between.apply", ["a", "b"], {"enum:CASE_ID_KEY": L}),
     call("between.apply", ["a", "b"], {"enum:CASE_ID_KEY": [{"b": 1}]}),
     call("between.apply", ["a", "b"], {"enum:CASE_ID_KEY": ["case:concept:name", None]}),
    ])]
    groups += PROBES
    # probe bt1
    C = "case:concept:name"
    keys = [[C, 1], [C, None, 1], [None], [None, C], [C, C], ["zz"], [C, 1.5], [C, True], [], (C, None), (C,), (None, C), [C, "zz"], [1], [1, C], [C, [1]], [[1]], [C, None, None]]
    PROBES = [(0, [call("between.apply", ["a", "b"], {"enum:CASE_ID_KEY": k}) for k in keys])]
    groups += PROBES
    # probe sb1
    C = "case:concept:name"
    L = [C, {"b": 1}]
    PROBES = [(0, [
     call("variants.apply", [[[1, 2]]], {"pm4py:param:case_id_key": L}),
     call("variants.apply", [[["a", "b"]]], {"pm4py:param:case_id_key": L, "pm4py:param:activity_key": [1]}),
     call("variants.apply", [[["a", "b"]]], {"pm4py:param:case_id_key": L, "pm4py:param:activity_key": 5}),
     call("variants.apply", [[["a", "b"]]], {"pm4py:param:case_id_key": L}),
     call("variants.apply", [[["a", "b"]]], {"pm4py:param:case_id_key": [C, None]}),
     call("variants.apply", [[["a", "b"]]], {"pm4py:param:case_id_key": [{"b": 1}]}),
     call("variants.filter_variants_top_k", [2], {"pm4py:param:case_id_key": L}),
     call("variants.filter_variants_top_k", [2], {"pm4py:param:case_id_key": L, "pm4py:param:activity_key": 5}),
     call("variants.filter_variants_top_k", [2], {"pm4py:param:case_id_key": [C, None]}),
     call("paths.apply", [[("a", "b")]], {"pm4py:param:case_id_key": L}),
     call("paths.apply", [[("a", "b")]], {"pm4py:param:case_id_key": L, "pm4py:param:attribute_key": 5}),
     call("paths.apply", [[("a", "b")]], {"pm4py:param:case_id_key": L, "pm4py:param:timestamp_key": 5}),
     call("paths.apply", [[("a", 1)]], {"pm4py:param:case_id_key": L}),
     call("paths.apply", [[("a", "b")]], {"pm4py:param:case_id_key": [C, None]}),
     call("paths.apply", [[("a", "b")]], {"pm4py:param:case_id_key": [{"b": 1}]}),
     call("paths.apply_performance", [("a", "b")], {"pm4py:param:case_id_key": L, "max_performance": []}),
     call("paths.apply_performance", [("a", "b")], {"pm4py:param:case_id_key": L, "max_performance": [1, "x"]}),
     call("paths.apply_performance", [("a", "b")], {"pm4py:param:case_id_key": [C, None]}),
     call("paths.apply_performance", [("a", "b")], {"pm4py:param:case_id_key": "cid", "pm4py:param:timestamp_key": {"b": 1}, "pm4py:param:attribute_key": "zz2"}),
    ])]
    groups += PROBES
    # probe ef1
    T = "pm4py:param:timestamp_key"
    PROBES = [(0, [
     call("ltl.eventually_follows", [["a"]], {T: {"zz": 1}}),
     call("ltl.eventually_follows", [["a"]], {T: "zzz"}),
     call("ltl.eventually_follows", [["a"]], {T: [{"a": 1}]}),
     call("ltl.eventually_follows", [[]], {T: {"zz": 1}}),
     call("ltl.eventually_follows", [["a", "b"]], {T: {"zz": 1}, "enum:ATTRIBUTE_KEY": "zz2"}),
     call("ltl.eventually_follows", [["a", "b"]], {T: {"zz": 1}, "enum:ATTRIBUTE_KEY": "time:timestamp"}),
     call("ltl.eventually_follows", [["a", 1]], {T: {"zz": 1}}),
     call("ltl.eventually_follows", [["a", "b"]], {T: [{"a": 1}]}),
     call("ltl.eventually_follows", [["a", "b"]], {T: [{"a": 1}, "x"]}),
     call("ltl.eventually_follows", [["a", "b"]], {T: ["x", {"a": 1}]}),
     call("ltl.eventually_follows", [["a", "b"]], {T: [1, 2]}),
     call("ltl.eventually_follows", [["a", "b"]], {T: {"zz": 1}, "enum:TIMESTAMP_DIFF_BOUNDARIES": [(0, 60)]}),
     call("ltl.eventually_follows", [["a", "b"]], {T: {"time:timestamp": 1}, "enum:TIMESTAMP_DIFF_BOUNDARIES": [(0, 60)]}),
     call("ltl.A_next_B_next_C", [["a", "b", "c"]], {T: {"zz": 1}}),
     call("ltl.A_next_B_next_C", [["a", "b", "c"]], {T: [{"a": 1}, "x"]}),
     call("ltl.A_next_B_next_C", [["a", "b", "c"]], {T: {"zz": 1}, "enum:ATTRIBUTE_KEY": "zz2"}),
    ])]
    groups += PROBES
    # probe ts1
    T = "time:timestamp"
    P = [("a", "b")]
    PROBES = [(f, [
     call("paths.apply", [[]], {"pm4py:param:timestamp_key": {"a": None}}),
     call("paths.apply", [[]], {"pm4py:param:timestamp_key": {"a": None}, "positive": True}),
     call("paths.apply", [[]], {"pm4py:param:timestamp_key": "zzz"}),
     call("paths.apply", [[]], {"pm4py:param:timestamp_key": {}}),
     call("paths.apply", [P], {"pm4py:param:timestamp_key": {"a": None}}),
     call("paths.apply", [P], {"pm4py:param:timestamp_key": {T: 1}}),
     call("paths.apply", [P], {"pm4py:param:timestamp_key": {}}),
     call("paths.apply", [P], {"pm4py:param:timestamp_key": {"a": None}, "positive": False}),
     call("paths.apply_performance", [("a", "b")], {"pm4py:param:timestamp_key": {T: 1}}),
     call("paths.apply_performance", [("a", "b")], {"pm4py:param:timestamp_key": {}}),
     call("paths.apply_performance", [("a", "b")], {"pm4py:param:timestamp_key": {"zz": 1}}),
     call("variants.filter_variants_top_k", [2], {"pm4py:param:timestamp_key": {T: 1}}),
     call("variants.filter_variants_top_k", [2], {"pm4py:param:timestamp_key": {}}),
     call("variants.filter_variants_top_k", [2], {"pm4py:param:timestamp_key": {"zz": 1}}),
     call("variants.apply", [[["a", "b"]]], {"pm4py:param:timestamp_key": {T: 1}}),
     call("ltl.eventually_follows", [["a", "b"]], {"pm4py:param:timestamp_key": {T: 1}}),
     call("ltl.eventually_follows", [["a", "b"]], {"pm4py:param:timestamp_key": {}}),
     call("ltl.eventually_follows", [["a", "b"]], {"pm4py:param:timestamp_key": {"zz": 1}}),
    ]) for f in (0, 1)]
    groups += PROBES
    # probe bt2
    C = "case:concept:name"
    keys = [(C, 1), (1, C), (C, [1]), (C, {"b": 1}), ({"b": 1},), (C, None, 1)]
    built = [(C, 1), (1, C), (C, [1]), (C, {"b": 1}), (C, None, 1)]
    PROBES = [(0, [call("between.apply", ["a", "b"], {"enum:CASE_ID_KEY": k}) for k in keys]
     + [call("ltl.eventually_follows", [["a", "b"]], {"enum:CASE_ID_KEY": k}) for k in built]
     + [call("ltl.four_eyes_principle", [["a", "b"]], {"enum:CASE_ID_KEY": k}) for k in built]
    )]
    groups += PROBES
    return groups


def generate_cases(count, bank_seed):
    frames = [c for c in base.generate_cases(200, 7)
              if c["spec"]["n"] >= 6 and c["spec"]["names"]["case"] == "case:concept:name" and None not in c["spec"]["cases"]]
    out = [{"spec": dict(frames[f]["spec"]), "calls": calls} for f, calls in _groups()]
    return out[:count]


for _k in dir(base):
    if not _k.startswith("__"):
        globals().setdefault(_k, getattr(base, _k))
globals()["generate_cases"] = generate_cases
