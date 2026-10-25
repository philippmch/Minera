"""Shared generator and native transport for the repetition domain banks.

Builds disjoint, seeded inputs for the public pandas, Polars and EventLog
attribute-value repetition filters, converts every input and public output to
the native transport (EventTable JSON, raw EventLog JSON, typed target JSON
and bound tokens), and summarizes public outcomes in the form compared by
check-repetition-domain-bank.py. Only public objects and documented entry
points are used; no reference implementation is read.
"""
import datetime as dt
import json
import math
import random

CASE = "case:concept:name"
ACT = "concept:name"
# Labels are prefixed so that no generated value overlaps earlier banks.
LABELS = ("rd-A", "rd-B", "rd-C", "")
CASES = ("rd-c0", "rd-c1", "rd-c2", "rd-c3")
FIXED_ZONES = (("UTC", 0), ("UTC+02:00", 120), ("UTC-05:30", -330))
NAMED_ZONES = ("Europe/Berlin", "America/New_York", "Asia/Kolkata", "Australia/Lord_Howe")
INSTANTS = ("2021-03-28 01:30:00", "2021-03-28 02:30:00", "2021-10-31 02:30:00",
            "2021-11-07 01:30:00", "2024-02-29 23:59:59.999999", "2021-03-28 00:00:00",
            "1969-12-31 23:59:59.000000001", "2021-06-01 12:00:00.000000500")


# --------------------------------------------------------------------------
# Native transport


def int_cell(key, value):
    storage = "int64" if -(1 << 63) <= value < (1 << 63) else "uint64"
    return {"key": key, "value": str(value), "kind": "int", "typed_value": value, "integer_storage": storage}


def float_text(value):
    if math.isnan(value):
        return "NaN"
    if math.isinf(value):
        return "INF" if value > 0 else "-INF"
    return repr(float(value))


def iso(value):
    """Exact ISO text of a pandas/py datetime, keeping nanoseconds and offset."""
    import pandas as pd
    stamp = pd.Timestamp(value)
    text = stamp.strftime("%Y-%m-%dT%H:%M:%S")
    frac = stamp.microsecond * 1000 + stamp.nanosecond
    if frac:
        text += "." + f"{frac:09d}"
    if stamp.tzinfo is not None:
        offset = stamp.utcoffset()
        minutes = int(offset.total_seconds() // 60)
        sign = "+" if minutes >= 0 else "-"
        minutes = abs(minutes)
        text += f"{sign}{minutes // 60:02d}:{minutes % 60:02d}"
    return text


def cell(key, value, zone=None):
    """Native EventAttribute JSON for one ordinary Python/pandas scalar."""
    import numpy as np
    import pandas as pd
    if value is None:
        return {"key": key, "null": True}
    if value is pd.NA:
        return {"key": key, "kind": "na"}
    if value is pd.NaT or (isinstance(value, np.datetime64) and np.isnat(value)):
        return {"key": key, "kind": "date", "null": True, "value": "NaT"}
    if isinstance(value, (bool, np.bool_)):
        return {"key": key, "kind": "boolean", "typed_value": bool(value), "value": "true" if value else "false"}
    if isinstance(value, (int, np.integer)):
        return int_cell(key, int(value))
    if isinstance(value, (float, np.floating)):
        return {"key": key, "kind": "float", "typed_value": float_text(float(value)), "value": float_text(float(value))}
    if isinstance(value, (dt.datetime, pd.Timestamp, np.datetime64)):
        stamp = pd.Timestamp(value)
        text = iso(stamp)
        result = {"key": key, "kind": "date", "typed_value": text, "value": text,
                  "date_resolution": "ns" if stamp.nanosecond else "us"}
        if zone:
            result["date_timezone"] = zone
        return result
    if isinstance(value, dt.date):
        text = value.isoformat()
        return {"key": key, "kind": "date", "typed_value": text, "value": text}
    if isinstance(value, str):
        return {"key": key, "value": value}
    if isinstance(value, bytes):
        return {"key": key, "kind": "bytes", "value": value.hex()}
    raise TypeError(f"no native cell for {type(value).__name__}")


def target_json(value):
    """Typed repetition target JSON for the workbench target_json control."""
    import pandas as pd
    if value is None:
        return {"type": "null"}
    if value is pd.NA:
        return {"type": "na"}
    if value is pd.NaT:
        raise TypeError("no target JSON for NaT")
    if isinstance(value, bool):
        return {"type": "boolean", "value": value}
    if isinstance(value, int):
        return {"type": "integer", "value": str(value)}
    if isinstance(value, float):
        return {"type": "decimal", "value": float_text(value)}
    if isinstance(value, (dt.datetime, pd.Timestamp)):
        out = {"type": "date", "value": iso(value)}
        # A named zone (zoneinfo or pytz) is kept; fixed offsets are not named.
        name = getattr(value.tzinfo, "key", None) or getattr(value.tzinfo, "zone", None)
        if name is not None:
            out["timezone"] = name
        return out
    if isinstance(value, dt.date):
        raise TypeError("no target JSON for a calendar date")
    if isinstance(value, str):
        return {"type": "string", "value": value}
    if isinstance(value, bytes):
        return {"type": "bytes", "value": value.hex()}
    if isinstance(value, dict):
        return {"type": "map", "value": {k: target_json(v) for k, v in value.items()}}
    for kind, cls in (("list", list), ("tuple", tuple), ("set", set), ("frozenset", frozenset)):
        if isinstance(value, cls):
            return {"type": kind, "value": [target_json(v) for v in value]}
    raise TypeError(f"no target JSON for {type(value).__name__}")


def bound_token(value):
    if value is None:
        return "null"
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        return float_text(value) if not math.isfinite(value) else repr(value)
    return json.dumps(value)


def zone_of(dtype_text):
    if dtype_text.startswith("datetime64[") and ", " in dtype_text:
        return dtype_text[dtype_text.index(", ") + 2:-1]
    if dtype_text.startswith("Datetime(") and "time_zone='" in dtype_text:
        return dtype_text.split("time_zone='")[1].split("'")[0]
    return None


def pandas_table_json(df):
    import pandas as pd
    columns = [str(c) for c in df.columns]
    dtypes = [str(t) for t in df.dtypes]
    rows = []
    for position in range(len(df)):
        rows.append([cell(c, df.iloc[position, i], zone_of(dtypes[i])) for i, c in enumerate(columns)])
    categorical = []
    for i, c in enumerate(columns):
        series = df.iloc[:, i]
        if isinstance(series.dtype, pd.CategoricalDtype):
            cats = series.dtype.categories
            categorical.append({"categories_dtype": str(cats.dtype), "ordered": bool(series.dtype.ordered),
                                "categories": [cell("", v) for v in cats.tolist()]})
        else:
            categorical.append(None)
    index = df.index
    labels = [{"kind": "scalar", "value": cell("", v)} for v in index.tolist()]
    return {"columns": columns, "dtypes": dtypes, "rows": rows,
            "categorical_dtypes": categorical if any(categorical) else [],
            "index": {"class": type(index).__name__, "dtype": str(index.dtype),
                      "names": [None if n is None else str(n) for n in index.names], "labels": labels}}


def polars_table_json(df):
    import polars as pl
    columns = list(df.columns)
    dtypes = [str(t) for t in df.dtypes]
    values = [polars_python_values(df[c]) for c in columns]
    rows = [[cell(c, values[i][r], zone_of(dtypes[i])) for i, c in enumerate(columns)] for r in range(df.height)]
    categorical = []
    for c in columns:
        dtype = df[c].dtype
        if isinstance(dtype, pl.Enum):
            categorical.append({"categories_dtype": "String", "ordered": False,
                                "categories": [cell("", v) for v in dtype.categories.to_list()]})
        else:
            categorical.append(None)
    out = {"columns": columns, "dtypes": dtypes, "rows": rows}
    if any(categorical):
        out["categorical_dtypes"] = categorical
    return out


# --------------------------------------------------------------------------
# Public outcome summaries


def row_key(cells):
    return json.dumps(cells, sort_keys=True)


def retained_positions(source_rows, result_rows):
    """Order-preserving subsequence positions, or None if not a subsequence."""
    keys = [row_key(r) for r in source_rows]
    positions, at = [], 0
    for row in result_rows:
        key = row_key(row)
        while at < len(keys) and keys[at] != key:
            at += 1
        if at == len(keys):
            return None
        positions.append(at)
        at += 1
    return positions


def table_summary(source_json, result_json):
    """What native must reproduce: columns, dtypes, categories, retained rows and index."""
    src_rows = source_json["rows"]
    if source_json.get("index"):
        src_rows = [r + [l] for r, l in zip(src_rows, source_json["index"]["labels"])]
        res_rows = [r + [l] for r, l in zip(result_json["rows"], result_json["index"]["labels"])]
    else:
        res_rows = result_json["rows"]
    summary = {"columns": result_json["columns"], "dtypes": result_json["dtypes"],
               "categorical": result_json.get("categorical_dtypes", []),
               "positions": retained_positions(src_rows, res_rows)}
    if source_json.get("index"):
        summary["index_class"] = result_json["index"]["class"]
        summary["index_names"] = result_json["index"]["names"]
    if summary["positions"] is None:
        summary["rows"] = result_json["rows"]
    return summary


# --------------------------------------------------------------------------
# pandas bank

PANDAS_INT = ("int8", "int16", "int32", "int64", "uint8", "uint16", "uint32", "uint64",
              "Int8", "Int16", "Int32", "Int64", "UInt8", "UInt16", "UInt32", "UInt64")
PANDAS_FLOAT = ("float32", "float64", "Float32", "Float64")
PANDAS_OTHER = ("object", "str", "string", "bool", "boolean", "category")
PANDAS_TIME_UNITS = ("s", "ms", "us", "ns")


def int_range(dtype):
    import numpy as np
    info = np.iinfo(dtype.lower())
    return int(info.min), int(info.max)


def pandas_column(rng, kind, n):
    """Return (values, dtype argument, zone-or-None) for one generated column."""
    import numpy as np
    import pandas as pd
    if kind in PANDAS_INT:
        lo, hi = int_range(kind)
        pool = sorted({v for v in (0, 1, 2, -1, 3, lo, hi, hi - 1, 1 << 24, (1 << 53) + 1, 127, 255)
                       if lo <= v <= hi})
        values = [rng.choice(pool[:6]) if rng.random() < 0.7 else rng.choice(pool) for _ in range(n)]
        if kind[0].isupper() and rng.random() < 0.6:
            values = [pd.NA if rng.random() < 0.2 else v for v in values]
        return values, kind
    if kind in PANDAS_FLOAT:
        pool = (0.0, -0.0, 0.1, 0.5, 1.0, 2.0, 16777217.0, 1e300, float("inf"), float("-inf"), float("nan"), 2.5)
        values = [rng.choice(pool[:6]) if rng.random() < 0.6 else rng.choice(pool) for _ in range(n)]
        if kind[0].isupper() and rng.random() < 0.5:
            values = [pd.NA if rng.random() < 0.2 else v for v in values]
        return values, kind
    if kind == "bool":
        return [rng.random() < 0.5 for _ in range(n)], "bool"
    if kind == "boolean":
        return [rng.choice((True, False, pd.NA)) for _ in range(n)], "boolean"
    if kind in ("str", "string"):
        pool = LABELS + ("1", "rd-é ", "NaN", "True")
        missing = pd.NA if kind == "string" else None
        return [missing if rng.random() < 0.1 else rng.choice(pool[:3] if rng.random() < 0.7 else pool)
                for _ in range(n)], kind
    if kind == "category":
        base = rng.choice(("str", "int", "float", "bool", "datetime"))
        pool = {"str": list(LABELS[:3]), "int": [1, 2, 3], "float": [0.1, 1.0, 2.5], "bool": [True, False],
                "datetime": [pd.Timestamp(INSTANTS[0]), pd.Timestamp(INSTANTS[5])]}[base]
        values = [None if rng.random() < 0.1 else rng.choice(pool) for _ in range(n)]
        extra = {"str": ["rd-unused"], "int": [9], "float": [9.5], "bool": [],
                 "datetime": [pd.Timestamp(INSTANTS[2])]}[base]
        cats = pool + extra if rng.random() < 0.5 else pool
        if rng.random() < 0.3:
            cats = list(reversed(cats))
        return values, pd.CategoricalDtype(cats, ordered=rng.random() < 0.3)
    if kind == "object":
        pool = (LABELS[0], LABELS[1], 1, 1.0, True, 0, False, None, float("nan"), pd.NA, (1 << 63) + 5,
                (1 << 53) + 1, float(1 << 53), b"rd-A", dt.datetime(2021, 6, 1, 12),
                pd.Timestamp("2021-06-01 12:00:00", tz="UTC"), "2021-06-01 12:00:00", -0.0)
        return [rng.choice(pool[:5]) if rng.random() < 0.6 else rng.choice(pool) for _ in range(n)], "object"
    if kind == "datetime":
        unit = rng.choice(PANDAS_TIME_UNITS)
        mode = rng.choice(("naive", "fixed", "named"))
        stamps = [pd.Timestamp(rng.choice(INSTANTS[:3] if rng.random() < 0.6 else INSTANTS)) for _ in range(n)]
        if mode == "naive":
            dtype, zone = f"datetime64[{unit}]", None
            values = stamps
        else:
            if mode == "fixed":
                name, minutes = rng.choice(FIXED_ZONES)
                tz = dt.timezone.utc if minutes == 0 else dt.timezone(dt.timedelta(minutes=minutes))
            else:
                tz = name = rng.choice(NAMED_ZONES)
            values = []
            for s in stamps:
                try:
                    values.append(s.tz_localize(tz, ambiguous=True, nonexistent="shift_forward"))
                except Exception:
                    values.append(s.tz_localize("UTC").tz_convert(tz))
            dtype = pd.DatetimeTZDtype(unit=unit, tz=tz)
        values = [pd.NaT if rng.random() < 0.1 else v for v in values]
        return values, dtype
    raise ValueError(kind)


def make_series(values, dtype):
    import pandas as pd
    return pd.Series(values, dtype=dtype)


def pandas_targets(rng, series, n):
    """Candidate targets: present values, numeric/text/temporal neighbours and other shapes."""
    import numpy as np
    import pandas as pd
    present = [v for v in series.tolist() if not (v is None or v is pd.NA or v is pd.NaT or
                                                 (isinstance(v, float) and math.isnan(v)))]
    out = []

    def native(v):
        if isinstance(v, np.generic):
            v = v.item()
        if isinstance(v, pd.Timestamp):
            return v.to_pydatetime() if v.nanosecond == 0 else v
        return v

    if present:
        v = native(rng.choice(present))
        out.append(v)
        if isinstance(v, bool):
            out.append(int(v))
        elif isinstance(v, int):
            out += [float(v), str(v), v + (1 << 64) if v < 0 else v + 1]
        elif isinstance(v, float):
            if math.isfinite(v):
                out += [float(np.float32(v)), str(v), float(str(np.float32(v)))]
                if v == int(v):
                    out += [int(v), int(v) + 1]
        elif isinstance(v, (dt.datetime, pd.Timestamp)):
            stamp = pd.Timestamp(v)
            out.append(str(stamp.tz_localize(None)) if stamp.tzinfo else str(stamp))
            if stamp.tzinfo:
                out.append(stamp.tz_convert(dt.timezone(dt.timedelta(hours=3))).to_pydatetime()
                           if stamp.nanosecond == 0 else stamp.tz_convert("UTC"))
                out.append(stamp.tz_localize(None).to_pydatetime() if stamp.nanosecond == 0 else stamp.tz_localize(None))
                out.append(stamp.isoformat())
            else:
                out.append(stamp.tz_localize("UTC").to_pydatetime() if stamp.nanosecond == 0 else stamp.tz_localize("UTC"))
        elif isinstance(v, str):
            out.append(v + " ")
    out += [rng.choice((None, float("nan"), pd.NA, "rd-A", 1, True, b"rd-A", 0.1, (1 << 64) + 3,
                        16777217, float(1 << 64), (1 << 53) + 1, 1.0, False, "2021-03-28 01:30:00"))]
    if rng.random() < 0.2:
        row = [native(x) if x is not pd.NA else None for x in series.tolist()]
        numbers = [x for x in row if isinstance(x, (int, float)) and not isinstance(x, bool)]
        shape = rng.random()
        if shape < 0.3:
            out.append(row)
        elif shape < 0.5 and numbers and len(numbers) == len(row):
            # Float and integer arrays of the column's own values.
            out.append([float(x) for x in row] if rng.random() < 0.5 else
                       [int(x) if isinstance(x, int) or (math.isfinite(x) and x == int(x)) else x for x in row])
        elif shape < 0.65 and row:
            out.append(row[:-1] + [None])
        else:
            out.append(["rd-A"] * (n + 1))
    if rng.random() < 0.05:
        out.append(rng.choice(((1, 2), {"a": 1}, frozenset({1}))))
    return out


def pandas_bounds(rng):
    lo = rng.choice((0, 1, 1, 2, 2, 2, 3, 1.5, -1, True, float("nan"), None, "1", (1 << 64) + 1, float("inf")))
    hi = rng.choice((9223372036854775807, 2, 3, 1, 2.5, False, float("inf"), float("nan"), None, "3", 0, 1 << 70))
    return lo, hi


def generate_pandas_cases(count, bank_seed):
    rng = random.Random(bank_seed)
    kinds = PANDAS_INT + PANDAS_FLOAT + PANDAS_OTHER + ("datetime",) * 6
    case_kinds = ("str", "object", "int64", "Int32", "uint8", "category", "string", "float64", "bool", "datetime")
    cases = []
    while len(cases) < count:
        n = rng.randint(0, 9)
        attr_kind = rng.choice(kinds)
        case_kind = rng.choice(case_kinds)
        spec = {"n": n, "attr_kind": attr_kind, "case_kind": case_kind,
                "seed": rng.randrange(1 << 30)}
        cases.append(spec)
    return cases


def build_pandas_case(spec):
    """Rebuild the frame, targets and parameters of one case from its spec."""
    import pandas as pd
    rng = random.Random(spec["seed"])
    n = spec["n"]
    if spec["case_kind"] == "datetime":
        case_values, case_dtype = pandas_column(rng, "datetime", n)
    elif spec["case_kind"] == "category":
        case_values = [rng.choice(CASES[:3]) for _ in range(n)]
        case_dtype = pd.CategoricalDtype(list(CASES), ordered=False)
    elif spec["case_kind"] in ("str", "object", "string"):
        case_values = [rng.choice(CASES) if rng.random() > 0.08 else None for _ in range(n)]
        case_dtype = spec["case_kind"]
    elif spec["case_kind"] == "float64":
        case_values = [rng.choice((0.0, 1.0, -0.0, float("nan"), 2.0)) for _ in range(n)]
        case_dtype = "float64"
    elif spec["case_kind"] == "bool":
        case_values = [rng.random() < 0.5 for _ in range(n)]
        case_dtype = "bool"
    else:
        case_values = [rng.choice((0, 1, 2, 3)) for _ in range(n)]
        case_dtype = spec["case_kind"]
        if case_dtype == "Int32":
            case_values = [pd.NA if rng.random() < 0.1 else v for v in case_values]
    attr_values, attr_dtype = pandas_column(rng, spec["attr_kind"], n)
    frame = pd.DataFrame({CASE: make_series(case_values, case_dtype), ACT: make_series(attr_values, attr_dtype)})
    if rng.random() < 0.3:
        frame["rd-other"] = make_series([rng.choice((1, 2)) for _ in range(n)], "int64")
    index_mode = rng.choice(("range", "range", "shifted", "labels", "duplicate"))
    if index_mode == "shifted":
        frame.index = pd.RangeIndex(10, 10 + 2 * n, 2)
    elif index_mode == "labels":
        frame.index = pd.Index([f"rd-i{i}" for i in range(n)], dtype="object")
    elif index_mode == "duplicate" and n:
        frame.index = pd.Index([i // 2 for i in range(n)], dtype="int64")
    targets = pandas_targets(rng, frame[ACT], n)
    calls = []
    for target in targets:
        params = {}
        if rng.random() < 0.7:
            params["min_rep"], params["max_rep"] = pandas_bounds(rng)
        if rng.random() < 0.05:
            params["attribute_key"] = rng.choice(("rd-missing", "", "concept:name\x00"))
        if rng.random() < 0.05:
            params["case_id_key"] = rng.choice(("rd-missing", "case:concept:name "))
        calls.append((target, params))
    return frame, calls


def native_call(case, spec, source, target, params):
    """Workbench controls for one public call. Unrepresentable shapes are marked."""
    try:
        target_text = json.dumps(target_json(target), ensure_ascii=False)
    except TypeError as error:
        target_text, unrepresentable = None, str(error)
    else:
        unrepresentable = None
    controls = {}
    for name in ("case_id_key", "attribute_key"):
        if name in params:
            controls[name] = params[name]
    for name in ("min_rep", "max_rep"):
        if name in params:
            controls[name] = bound_token(params[name])
    if not isinstance(source, str):
        source = json.dumps(source, ensure_ascii=False, sort_keys=True)
    return {"case": case, "spec": spec, "source": source,
            "target_json": target_text, "controls": controls, "unrepresentable": unrepresentable}


# --------------------------------------------------------------------------
# Polars bank

POLARS_INT = ("Int8", "Int16", "Int32", "Int64", "UInt8", "UInt16", "UInt32", "UInt64")
POLARS_FLOAT = ("Float32", "Float64")
POLARS_OTHER = ("String", "Boolean", "Categorical", "Enum", "Null", "Date")
POLARS_ZONES = (None, "UTC", "Europe/Berlin", "America/New_York", "Asia/Kolkata", "Australia/Lord_Howe")


def polars_dtype(kind, zone=None, unit="us", categories=None):
    import polars as pl
    if kind == "Datetime":
        return pl.Datetime(time_unit=unit, time_zone=zone)
    if kind == "Enum":
        return pl.Enum(categories)
    return getattr(pl, kind)


def polars_column(rng, kind, n):
    """Return (python values, polars dtype) for one generated column."""
    if kind in POLARS_INT:
        import numpy as np
        info = np.iinfo(kind.lower())
        pool = sorted({v for v in (0, 1, 2, -1, 3, int(info.min), int(info.max), 1 << 24, (1 << 53) + 1, 127, 255)
                       if info.min <= v <= info.max})
        return [None if rng.random() < 0.1 else (rng.choice(pool[:6]) if rng.random() < 0.7 else rng.choice(pool))
                for _ in range(n)], polars_dtype(kind)
    if kind in POLARS_FLOAT:
        pool = (0.0, -0.0, 0.1, 0.5, 1.0, 2.0, 16777217.0, 1e300, float("inf"), float("-inf"), float("nan"), 2.5)
        return [None if rng.random() < 0.1 else (rng.choice(pool[:6]) if rng.random() < 0.6 else rng.choice(pool))
                for _ in range(n)], polars_dtype(kind)
    if kind == "Boolean":
        return [rng.choice((True, False, None)) for _ in range(n)], polars_dtype(kind)
    if kind in ("String", "Categorical"):
        pool = LABELS + ("1", "rd-é ", "NaN", "true", "2021-03-28")
        return [None if rng.random() < 0.1 else rng.choice(pool[:3] if rng.random() < 0.7 else pool)
                for _ in range(n)], polars_dtype(kind)
    if kind == "Enum":
        cats = list(LABELS[:3]) + (["rd-unused"] if rng.random() < 0.5 else [])
        if rng.random() < 0.3:
            cats.reverse()
        return [None if rng.random() < 0.1 else rng.choice(LABELS[:3]) for _ in range(n)], polars_dtype(kind, categories=cats)
    if kind == "Null":
        return [None] * n, polars_dtype(kind)
    if kind == "Date":
        pool = (dt.date(2021, 3, 28), dt.date(2021, 10, 31), dt.date(1969, 12, 31), dt.date(2024, 2, 29))
        return [None if rng.random() < 0.1 else rng.choice(pool) for _ in range(n)], polars_dtype(kind)
    if kind == "Datetime":
        unit = rng.choice(("ms", "us", "ns"))
        zone = rng.choice(POLARS_ZONES)
        stamps = [None if rng.random() < 0.1 else rng.choice(INSTANTS[:3] if rng.random() < 0.6 else INSTANTS)
                  for _ in range(n)]
        return [s and ("utc", s) for s in stamps], polars_dtype(kind, zone, unit)
    raise ValueError(kind)


def polars_series(name, values, dtype):
    """Build a Series; datetime values are ('utc', text) instants, exact to the column unit."""
    import pandas as pd
    import polars as pl
    if isinstance(dtype, pl.Datetime):
        epochs = []
        for v in values:
            if v is None:
                epochs.append(None)
                continue
            stamp = pd.Timestamp(v[1])
            if dtype.time_zone is None:
                ns = stamp.value
            else:
                ns = stamp.tz_localize("UTC").value
            scale = {"ms": 1_000_000, "us": 1_000, "ns": 1}[dtype.time_unit]
            epochs.append(ns // scale)
        series = pl.Series(name, epochs, dtype=pl.Int64).cast(pl.Datetime(time_unit=dtype.time_unit))
        if dtype.time_zone is not None:
            series = series.dt.replace_time_zone("UTC").dt.convert_time_zone(dtype.time_zone)
        return series
    return pl.Series(name, values, dtype=dtype)


def polars_targets(rng, series, n):
    import polars as pl
    values = polars_python_values(series)
    present = [v for v in values if v is not None and not (isinstance(v, float) and math.isnan(v))]
    out = []
    if present:
        v = rng.choice(present)
        out.append(v)
        if isinstance(v, bool):
            out += [int(v), str(v).lower(), str(v)]
        elif isinstance(v, int):
            out += [float(v), str(v), v + 1 if v >= 0 else v - (1 << 64)]
        elif isinstance(v, float):
            if math.isfinite(v):
                import numpy as np
                out += [float(np.float32(v)), str(v)]
                if v == int(v):
                    out.append(int(v))
        elif isinstance(v, dt.datetime):
            out += [v.replace(tzinfo=None) if v.tzinfo else v.replace(tzinfo=dt.timezone.utc), v.isoformat(),
                    v.date()]
            if v.tzinfo:
                out.append(v.astimezone(dt.timezone(dt.timedelta(hours=3))))
        elif isinstance(v, dt.date):
            out += [dt.datetime(v.year, v.month, v.day), v.isoformat()]
        elif isinstance(v, str):
            out.append(v + " ")
    out.append(rng.choice((None, float("nan"), "rd-A", 1, True, 0.1, (1 << 64) + 3, b"rd-A",
                           dt.date(2021, 3, 28), dt.datetime(2021, 3, 28, 1, 30))))
    if rng.random() < 0.08:
        out.append(rng.choice((["rd-A"], [1, 2], (1,))))
    return out


def polars_python_values(series):
    """Python values with exact nanosecond datetimes as pandas Timestamps."""
    import pandas as pd
    import polars as pl
    if isinstance(series.dtype, pl.Datetime):
        out = []
        for e in series.dt.epoch(time_unit="ns").to_list():
            if e is None:
                out.append(None)
                continue
            stamp = pd.Timestamp(e, unit="ns")
            if series.dtype.time_zone is not None:
                stamp = stamp.tz_localize("UTC").tz_convert(series.dtype.time_zone)
            out.append(stamp.to_pydatetime() if stamp.nanosecond == 0 else stamp)
        return out
    return series.to_list()


def polars_bounds(rng):
    lo = rng.choice((0, 1, 1, 2, 2, 2, 3, 1.5, -1, True, float("nan"), None, "1", (1 << 64) + 1, float("inf"), 1 << 33))
    hi = rng.choice((9223372036854775807, 2, 3, 1, 2.5, False, float("inf"), float("nan"), None, "3", 0, 1 << 70, 1 << 33))
    return lo, hi


def generate_polars_cases(count, bank_seed):
    rng = random.Random(bank_seed)
    kinds = POLARS_INT + POLARS_FLOAT + POLARS_OTHER + ("Datetime",) * 5
    case_kinds = ("String", "Int64", "UInt32", "Int8", "Categorical", "Enum", "Float64", "Boolean", "Date", "Datetime")
    return [{"n": rng.randint(0, 9), "attr_kind": rng.choice(kinds), "case_kind": rng.choice(case_kinds),
             "seed": rng.randrange(1 << 30)} for _ in range(count)]


def build_polars_case(spec):
    import polars as pl
    rng = random.Random(spec["seed"])
    n = spec["n"]
    kind = spec["case_kind"]
    if kind in ("String", "Categorical"):
        case_values, case_dtype = [rng.choice(CASES) if rng.random() > 0.08 else None for _ in range(n)], polars_dtype(kind)
    elif kind == "Enum":
        case_values, case_dtype = [rng.choice(CASES[:3]) for _ in range(n)], polars_dtype(kind, categories=list(CASES))
    elif kind == "Float64":
        case_values, case_dtype = [rng.choice((0.0, 1.0, -0.0, float("nan"), 2.0, None)) for _ in range(n)], pl.Float64
    elif kind == "Boolean":
        case_values, case_dtype = [rng.choice((True, False, None)) for _ in range(n)], pl.Boolean
    elif kind in ("Date", "Datetime"):
        case_values, case_dtype = polars_column(rng, kind, n)
    else:
        case_values, case_dtype = [rng.choice((0, 1, 2, 3, None)) for _ in range(n)], polars_dtype(kind)
    attr_values, attr_dtype = polars_column(rng, spec["attr_kind"], n)
    columns = [polars_series(CASE, case_values, case_dtype), polars_series(ACT, attr_values, attr_dtype)]
    if rng.random() < 0.3:
        columns.append(pl.Series("rd-other", [rng.choice((1, 2)) for _ in range(n)], dtype=pl.Int64))
    frame = pl.DataFrame(columns)
    targets = polars_targets(rng, frame[ACT], n)
    calls = []
    for target in targets:
        params = {}
        if rng.random() < 0.7:
            params["min_rep"], params["max_rep"] = polars_bounds(rng)
        if rng.random() < 0.05:
            params["attribute_key"] = rng.choice(("rd-missing", "", "concept:name\x00"))
        if rng.random() < 0.05:
            params["case_id_key"] = rng.choice(("rd-missing", "case:concept:name "))
        calls.append((target, params))
    return frame, calls


# --------------------------------------------------------------------------
# Log bank: XES documents imported by the public default importer, so the
# event values are exactly what that importer stores, including value/children
# wrappers for scalar attributes with nested elements.

LOG_KEYS = ("concept:name", "rd-k")
LOG_SCALARS = {
    "string": ("rd-A", "rd-B", "", "1", "True", "rd-\u00e9"),
    "int": ("1", "2", "0", "-1", "3"),
    "float": ("1.0", "0.1", "2.5", "-0.0", "1e3"),
    "boolean": ("true", "false"),
    "date": ("2021-03-28T01:30:00.000+00:00", "2021-03-28T01:30:00", "2021-03-28T03:30:00+02:00",
             "2021-03-28T01:30:00.500+00:00"),
    "id": ("rd-A", "rd-id"),
}


def xml_attr(text):
    from xml.sax.saxutils import quoteattr
    return quoteattr(text)


def log_element(rng, key, depth=0):
    """One typed XES attribute, sometimes a scalar wrapper with nested children."""
    kind = rng.choice(("string", "string", "string", "int", "int", "float", "boolean", "date", "id"))
    value = rng.choice(LOG_SCALARS[kind])
    head = f"<{kind} key={xml_attr(key)} value={xml_attr(value)}"
    if depth < 2 and rng.random() < 0.25:
        # The public importer cannot nest a wrapper inside <values> pairs.
        pairs = rng.random() < 0.4
        children = [log_element(rng, rng.choice(("a", "b", "a")), 2 if pairs else depth + 1)
                    for _ in range(rng.randint(0, 2))]
        if pairs:
            return f"{head}><values>{''.join(children)}</values></{kind}>"
        return f"{head}>{''.join(children)}</{kind}>"
    return head + "/>"


def log_list_element(rng, key, depth=0, in_pairs=False):
    """An XES <list> or <container> in one of the shapes the public importer
    distinguishes. Inside <values> pairs nothing may gain child elements,
    which the public importer rejects."""
    def child(pairs):
        roll = rng.random()
        name = rng.choice(("a", "b", "a", "n"))
        if roll < 0.15:
            return f"<container key={xml_attr(name)}>{log_element(rng, 'x', 2)}</container>"
        if roll < 0.3 and depth < 2:
            return log_list_element(rng, name, depth + 1, pairs)
        return log_element(rng, name, 2 if pairs else depth + 1)
    head = f"<list key={xml_attr(key)}"
    form = rng.choice(("empty", "open", "mapping", "mapping", "pairs", "pairs", "container", "values-late"))
    if in_pairs and form not in ("empty", "open", "container"):
        form = "empty"
    if form == "empty":
        return head + "/>"
    if form == "open":
        return head + "></list>"
    if form == "container":
        return f"<container key={xml_attr(key)}>{''.join(child(False) for _ in range(rng.randint(0, 2)))}</container>"
    if form == "mapping":
        return f"{head}>{''.join(child(False) for _ in range(rng.randint(1, 3)))}</list>"
    if form == "pairs":
        inner = "".join(child(True) for _ in range(rng.randint(0, 3)))
        after = "".join(log_element(rng, rng.choice(("a", "b")), 2) for _ in range(rng.randint(0, 1)))
        return f"{head}><values>{inner}</values>{after}</list>"
    # <values> after a stored child is ignored by the public importer.
    first = log_element(rng, "a", 2)
    late = "".join(log_element(rng, "b", 2) for _ in range(rng.randint(0, 2)))
    return f"{head}>{first}<values>{late}</values></list>"


def generate_log_cases(count, bank_seed, lists=False):
    rng = random.Random(bank_seed)
    cases = [{"traces": rng.randint(0, 5), "key": rng.choice(LOG_KEYS), "seed": rng.randrange(1 << 30)}
             for _ in range(count)]
    if lists:
        for case in cases:
            case["lists"] = True
    return cases


def build_log_xes(spec):
    rng = random.Random(spec["seed"])
    element = log_element
    if spec.get("lists"):
        element = lambda rng, key: (log_list_element(rng, key) if rng.random() < 0.6
                                    else log_element(rng, key))
    pool = [element(rng, spec["key"]) for _ in range(rng.randint(1, 4))]
    lines = ['<?xml version="1.0" encoding="UTF-8"?>', '<log xes.version="1.0">']
    for t in range(spec["traces"]):
        lines.append(f' <trace><string key="concept:name" value="rd-t{t}"/>')
        for _ in range(rng.randint(0, 5)):
            parts = []
            if spec["key"] != "concept:name":
                parts.append(f'<string key="concept:name" value={xml_attr(rng.choice(LABELS[:3]))}/>')
            if rng.random() < 0.85:
                parts.append(rng.choice(pool) if rng.random() < 0.8 else element(rng, spec["key"]))
            lines.append("  <event>" + "".join(parts) + "</event>")
        lines.append(" </trace>")
    lines.append("</log>")
    return "\n".join(lines) + "\n"


def import_log(xes):
    import os
    import tempfile
    from pm4py.objects.log.importer.xes import importer as xes_importer
    handle, path = tempfile.mkstemp(suffix=".xes")
    try:
        with os.fdopen(handle, "w", encoding="utf-8") as out:
            out.write(xes)
        return xes_importer.apply(path, parameters={"show_progress_bar": False})
    finally:
        os.unlink(path)


def log_targets(rng, log, key):
    import copy
    present = [e[key] for t in log for e in t if key in e]
    out = []
    if present:
        v = rng.choice(present)
        out.append(copy.deepcopy(v))
        if isinstance(v, dict):
            out.append(v.get("value"))
            changed = copy.deepcopy(v)
            changed["children"] = {} if changed.get("children") else {"a": "rd-A"}
            out.append(changed)
            if isinstance(v.get("children"), list):
                out.append({"value": v.get("value"), "children": dict(v["children"])})
        elif isinstance(v, bool):
            out.append(int(v))
        elif isinstance(v, int):
            out += [float(v), str(v)]
        elif isinstance(v, float):
            out.append(str(v))
            if math.isfinite(v) and v == int(v):
                out.append(int(v))
        elif isinstance(v, dt.datetime):
            out += [v.replace(tzinfo=None) if v.tzinfo else v.replace(tzinfo=dt.timezone.utc), v.isoformat()]
        elif isinstance(v, str):
            out.append(v + " ")
    out.append(rng.choice((None, float("nan"), "rd-A", 1, True, 1.0, {"value": "rd-A", "children": {}}, ["rd-A"])))
    return out


def log_bounds(rng):
    lo = rng.choice((0, 1, 1, 2, 2, 3, 1.5, -1, True, float("nan"), None, "1", (1 << 64) + 1, float("inf")))
    hi = rng.choice((9223372036854775807, 2, 3, 1, 2.5, False, float("inf"), float("nan"), None, "3", 0, 1 << 70))
    return lo, hi


def build_log_case(spec):
    """(XES text, calls); the public side imports the text for every call."""
    rng = random.Random(spec["seed"] ^ 0x5EED)
    xes = build_log_xes(spec)
    log = import_log(xes)
    calls = []
    for target in log_targets(rng, log, spec["key"]):
        params = {"attribute_key": spec["key"]} if spec["key"] != "concept:name" or rng.random() < 0.3 else {}
        if rng.random() < 0.7:
            params["min_rep"], params["max_rep"] = log_bounds(rng)
        if rng.random() < 0.04:
            params["attribute_key"] = "rd-missing"
        calls.append((target, params))
    return xes, calls


def log_fingerprint(log):
    return repr([(dict(t.attributes), [dict(e) for e in t]) for t in log])


def log_retained(source, result):
    return [t.attributes.get("concept:name") for t in result]


def public_log_summary(outcome):
    if "error" in outcome:
        return {"error": outcome["message"], "source_unchanged": outcome["source_unchanged"]}
    return {"retained": outcome["retained"], "source_unchanged": outcome["source_unchanged"]}


def native_log_summary(call, outcome):
    if "error" in outcome:
        return {"error": outcome["error"], "source_unchanged": outcome["source_unchanged"]}
    log = outcome["retained"]
    unhex = None
    if "native_byte_strings" in log:
        log = log["value"]
        unhex = lambda text: bytes.fromhex(text).decode("utf-8", "surrogateescape")
    retained = [unhex(t["case_id"]) if unhex else t["case_id"] for t in log.get("traces", [])]
    return {"retained": retained, "source_unchanged": outcome["source_unchanged"]}
