"""Round CA: the Polars sequence filter domain with typed key columns.

Loads scripts/polars_sequence_filter_domain.py and replaces frame_spec and
build_frame: Categorical and Enum activities and cases (Enum categories in a
non-sorted order, unused categories), UInt8/Boolean/Float64 cases, nulls in
the case column."""
import importlib.util, os, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("polars_sequence_filter_domain", HERE / "polars_sequence_filter_domain.py")
base = importlib.util.module_from_spec(_spec)
sys.modules["polars_sequence_filter_domain"] = base
_spec.loader.exec_module(base)
CASE_KINDS = ["cat", "enum", "Int64", "UInt8", "Float64", "Boolean", "String", "int"]
ACT_KINDS = ["cat", "enum", "enumx", "String"]
_orig_spec = base.frame_spec
def frame_spec(r):
    spec = _orig_spec(r)
    spec["case_kind"] = r.choice(CASE_KINDS)
    spec["act_kind"] = r.choice(ACT_KINDS)
    if spec["n"] and spec["case_kind"] != "int" and r.random() < 0.3:
        for _ in range(r.randint(1, 2)):
            spec["cases"][r.randrange(spec["n"])] = None
    if spec["case_kind"] == "int":
        spec["cases"] = [1 if c is None else c for c in spec["cases"]]
    spec["unused_cat"] = r.random() < 0.4
    return spec
def _case(spec, pl, name):
    k, cs = spec["case_kind"], spec["cases"]
    if k == "int": return pl.Series(name, cs, dtype=pl.Int64)
    if k == "String": return pl.Series(name, [None if c is None else str(c) for c in cs], dtype=pl.String)
    if k == "cat": return pl.Series(name, [None if c is None else str(c) for c in cs], dtype=pl.Categorical)
    if k == "enum":
        cats = ["4", "3", "2", "1"] + (["9"] if spec["unused_cat"] else [])
        return pl.Series(name, [None if c is None else str(c) for c in cs], dtype=pl.Enum(cats))
    if k == "Boolean": return pl.Series(name, [None if c is None else bool(c % 2) for c in cs], dtype=pl.Boolean)
    if k == "Float64": return pl.Series(name, [None if c is None else c + 0.5 for c in cs], dtype=pl.Float64)
    return pl.Series(name, cs, dtype=getattr(pl, k))
def _act(spec, pl, name):
    k, acts = spec["act_kind"], spec["acts"]
    if k == "String": return pl.Series(name, acts, dtype=pl.String)
    if k == "cat": return pl.Series(name, acts, dtype=pl.Categorical)
    pool = sorted({a for a in acts if a is not None} | {"a", "b"})
    if spec["unused_cat"]: pool.append("zz")
    if k == "enumx": pool = list(reversed(pool))
    return pl.Series(name, acts, dtype=pl.Enum(pool))
_orig_build = base.build_frame
def build_frame(spec, sort_by_case=False):
    import datetime as dt
    import polars as pl
    if "act_kind" not in spec: return _orig_build(spec, sort_by_case)
    names = spec["names"]
    b = dt.datetime(2020, 1, 1, tzinfo=dt.timezone.utc)
    columns = [_case(spec, pl, names["case"]), _act(spec, pl, names["act"]),
               pl.Series(names["ts"], [b + dt.timedelta(hours=h) for h in spec["hours"]], dtype=pl.Datetime("us", "UTC")),
               pl.Series("rd", list(range(spec["n"])), dtype=pl.Int64)]
    if spec["extra"] is not None:
        columns.append(pl.Series("x", [float("nan") if v == "nan" else v for v in spec["extra"]], dtype=pl.Float64))
    df = pl.DataFrame(columns)
    if spec["order"] == "shuffled" and df.height:
        df = df.sample(fraction=1.0, shuffle=True, seed=spec["shuffle_seed"] % (1 << 31))
    if sort_by_case:
        df = df.sort(names["case"], maintain_order=True, nulls_last=False)
    return df
base.frame_spec = frame_spec
base.build_frame = build_frame
for _k in dir(base):
    if not _k.startswith("__"): globals().setdefault(_k, getattr(base, _k))
globals()["frame_spec"] = frame_spec
globals()["build_frame"] = build_frame
