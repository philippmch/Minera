"""Round CA: the pandas sequence filter domain with typed key columns.

Loads scripts/pandas_sequence_filter_domain.py and replaces frame_spec/build_frame so that the case and activity columns
take pandas dtypes beyond str/int64: categorical (str, int categories, with
gaps), Int64/UInt8/Float64 (with <NA>), string[python] (with <NA>), boolean,
object. Categorical gaps travel as absent cells (as the table transport
sends them)."""
import importlib.util, os, random, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("pandas_sequence_filter_domain", HERE / "pandas_sequence_filter_domain.py")
base = importlib.util.module_from_spec(_spec)
sys.modules["pandas_sequence_filter_domain"] = base
_spec.loader.exec_module(base)

CASE_KINDS = ["cat", "cati", "Int64", "UInt8", "Float64", "string", "boolean", "object", "str", "int"]
ACT_KINDS = ["cat", "string", "object", "str", "catord", "catx"]
_orig_spec = base.frame_spec
def frame_spec(r):
    spec = _orig_spec(r)
    spec["case_kind"] = r.choice(CASE_KINDS)
    spec["act_kind"] = r.choice(ACT_KINDS)
    # gaps in typed case columns
    if spec["n"] and spec["case_kind"] not in ("int",) and r.random() < 0.3:
        for _ in range(r.randint(1, 2)):
            spec["cases"][r.randrange(spec["n"])] = None
    if spec["case_kind"] == "int":
        spec["cases"] = [1 if c is None else c for c in spec["cases"]]
    spec["unused_cat"] = r.random() < 0.4
    return spec

def _case_series(spec, pd):
    k, cs = spec["case_kind"], spec["cases"]
    if k == "int":
        return pd.Series(cs, dtype="int64")
    if k == "str":
        return pd.Series([None if c is None else str(c) for c in cs], dtype="str")
    if k == "object":
        return pd.Series([None if c is None else (c if c % 2 else str(c)) for c in cs], dtype="object")
    if k == "cat":
        cats = ["1", "2", "3", "4"] + (["9"] if spec["unused_cat"] else [])
        return pd.Series(pd.Categorical([None if c is None else str(c) for c in cs], categories=cats))
    if k == "cati":
        cats = [4, 3, 2, 1] + ([7] if spec["unused_cat"] else [])
        return pd.Series(pd.Categorical(cs, categories=cats))
    if k == "boolean":
        return pd.Series(pd.array([None if c is None else bool(c % 2) for c in cs], dtype="boolean"))
    if k == "Float64":
        return pd.Series(pd.array([None if c is None else c + 0.5 for c in cs], dtype="Float64"))
    return pd.Series(pd.array(cs, dtype=k))

def _act_series(spec, pd):
    k, acts = spec["act_kind"], spec["acts"]
    if k == "str":
        return pd.Series(acts, dtype="str")
    if k == "object":
        return pd.Series(acts, dtype="object")
    if k == "string":
        return pd.Series(pd.array(acts, dtype="string"))
    pool = sorted({a for a in acts if a is not None} | {"a", "b"})
    if spec["unused_cat"]:
        pool = pool + ["zz"]
    if k == "catx":
        pool = list(reversed(pool))
    return pd.Series(pd.Categorical(acts, categories=pool, ordered=(k == "catord")))

def build_frame(spec, sort_by_case=False):
    import pandas as pd
    if "act_kind" not in spec:
        return _orig_build(spec, sort_by_case)
    names = spec["names"]
    base_ts = pd.Timestamp("2020-01-01", tz="UTC")
    data = {names["case"]: _case_series(spec, pd),
            names["act"]: _act_series(spec, pd),
            names["ts"]: pd.Series([base_ts + pd.Timedelta(hours=h) for h in spec["hours"]], dtype="datetime64[us, UTC]"),
            "rd": pd.Series(range(spec["n"]), dtype="int64")}
    if spec["extra"] is not None:
        data["x"] = pd.Series([float("nan") if v is None else v for v in spec["extra"]], dtype="float64")
    df = pd.DataFrame(data)
    if spec["index"] is not None:
        df.index = pd.Index(spec["index"], dtype="int64")
    if spec["order"] == "shuffled" and len(df):
        df = df.sample(frac=1, random_state=spec["shuffle_seed"] % (1 << 31))
    if sort_by_case:
        try:
            df = df.sort_values(names["case"], kind="stable")
        except TypeError:
            pass
    return df

_orig_build = base.build_frame
base.frame_spec = frame_spec
base.build_frame = build_frame

_orig_table_json = base.tables.pandas_table_json
def pandas_table_json(df):
    import pandas as pd, math
    out = _orig_table_json(df)
    for i, c in enumerate(df.columns):
        if isinstance(df.dtypes.iloc[i], pd.CategoricalDtype):
            for row in out["rows"]:
                v = row[i]
                if isinstance(v, dict) and v.get("kind") == "float" and v.get("value") == "NaN":
                    row[i] = None
                elif isinstance(v, dict) and v.get("null"):
                    row[i] = None
    return out
base.tables.pandas_table_json = pandas_table_json
for _k in dir(base):
    if not _k.startswith("__"):
        globals().setdefault(_k, getattr(base, _k))
globals()["frame_spec"] = frame_spec
globals()["build_frame"] = build_frame
