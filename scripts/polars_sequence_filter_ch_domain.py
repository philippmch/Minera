"""Round CH: the Polars sequence filter domain with odd float activities.

Loads scripts/polars_sequence_filter_cc_domain.py; Float32 and Float64
activity columns hold odd values (1e-5, 1e-7, 1e16, 0.1, -0.0, 1/3, NaN,
infinities, subnormals), whose casts to String (variant strings, joined
activities) differ from Python's repr. The calls pass the column's floats
(sometimes ints or strs) as activity values."""
import importlib.util
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("polars_sequence_filter_cc_domain",
                                               HERE / "polars_sequence_filter_cc_domain.py")
cc = importlib.util.module_from_spec(_spec)
sys.modules["polars_sequence_filter_cc_domain"] = cc
_spec.loader.exec_module(cc)
codec = cc.codec

ODD = {
    "Float64": {1: 1e-5, 2: 0.1, 3: 1e16, 4: 1e-7, 5: -0.0, 6: 1 / 3, 7: 2.5e-6, 8: 1.5e16},
    "Float32": {1: 0.1, 2: 1e-5, 3: 1e15, 4: 3.3e-7, 5: 1 / 3, 6: 1e-6, 7: 16777217.0, 8: -0.0},
    "Float64x": {1: float("nan"), 2: float("inf"), 3: float("-inf"), 4: 1e-5, 5: 5e-324, 6: 1.7976931348623157e308,
                 7: 1e22, 8: 9.999999999999999e-6},
}
_cc_frame_spec = cc.frame_spec
_cc_act_value = cc.act_value
_cc_dtype = cc._dtype
_cc_numeric_arg = cc._numeric_arg


def odd(kind, c):
    table = ODD[kind]
    return table.get(c, table[1 + c % len(table)])


def frame_spec(r):
    spec = _cc_frame_spec(r)
    if r.random() < 0.6:
        spec["act_kind"] = r.choice(list(ODD))
        spec["arg_mode"] = r.choice(["plain", "value", "value", "mixed"])
    return spec


def act_value(kind, c):
    if kind in ODD:
        return None if c is None else odd(kind, c)
    return _cc_act_value(kind, c)


def _dtype(pl, kind):
    return pl.Float64 if kind == "Float64x" else _cc_dtype(pl, kind)


def _numeric_arg(r, kind, text, mode):
    if kind not in ODD:
        return _cc_numeric_arg(r, kind, text, mode)
    if text not in cc.cb.ORDER:
        return {"s": text}
    c = cc._code(text)
    pick = r.random()
    if mode == "mixed" and pick < 0.2:
        return {"s": text}
    if pick < 0.75:
        return {"f": codec.fbits(odd(kind, c))}
    return {"i": str(c)}


for _kind in ("Float64", "Float64x"):
    if _kind not in cc.NEW_KINDS:
        cc.NEW_KINDS.append(_kind)
    cc.NUMERIC.add(_kind)
cc.NUMERIC.add("Float32")
cc.act_value = act_value
cc._dtype = _dtype
cc._numeric_arg = _numeric_arg
cc.base.frame_spec = frame_spec
cc.typed.frame_spec = frame_spec
cc.cb.frame_spec = frame_spec
for _k in dir(cc):
    if not _k.startswith("__"):
        globals().setdefault(_k, getattr(cc, _k))
globals()["frame_spec"] = frame_spec
