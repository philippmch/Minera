"""Capture public PM4Py case-relation table semantics for EventTable parity.

This probe intentionally uses only the public pandas and PM4Py API.  It records
successful output ordering/schema and public exception type/message, without
reading implementation details from the reference package.
"""

import warnings

warnings.filterwarnings("ignore")

import argparse
import runpy
import json
import math
import numbers
from pathlib import Path

import pandas as pd
import pm4py
from pm4py.algo.merging.case_relations import algorithm


REFERENCE_VERSION = "2.7.22"
TIMESTAMP_PARAMETER = "pm4py:param:timestamp_key"


def normalize(value):
    """Return a JSON-safe public value while preserving useful scalar detail."""

    if value is None:
        return None
    if hasattr(value, "item") and not isinstance(value, pd.Timestamp):
        value = value.item()
    try:
        missing = pd.isna(value)
    except Exception:
        missing = False
    if isinstance(missing, bool) and missing:
        return None
    if hasattr(value, "isoformat"):
        try:
            return value.isoformat()
        except Exception:
            pass
    if isinstance(value, bool):
        return value
    if isinstance(value, numbers.Number):
        try:
            scalar = value.item()
        except AttributeError:
            scalar = value
        if isinstance(scalar, float) and not math.isfinite(scalar):
            return None
        return scalar
    if isinstance(value, str):
        return value
    return str(value)


def scalar_kind(value):
    normalized = normalize(value)
    if normalized is None:
        return "null"
    if hasattr(value, "isoformat"):
        return "date"
    if isinstance(normalized, bool):
        return "bool"
    if isinstance(normalized, int):
        return "int"
    if isinstance(normalized, float):
        return "float"
    return "string"


def describe_frame(frame):
    records = []
    for position, index in enumerate(frame.index):
        records.append({
            "index": normalize(index),
            "values": {str(column): normalize(frame[column].iloc[position]) for column in frame.columns},
            "kinds": {str(column): scalar_kind(frame[column].iloc[position]) for column in frame.columns},
        })
    return {
        "columns": [str(column) for column in frame.columns],
        "dtypes": {str(column): str(frame[column].dtype) for column in frame.columns},
        "shape": [int(frame.shape[0]), int(frame.shape[1])],
        "records": records,
    }


def describe_output(output):
    records = []
    for position, index in enumerate(output.index):
        records.append({
            "index": normalize(index),
            "values": {str(column): normalize(output[column].iloc[position]) for column in output.columns},
            "kinds": {str(column): scalar_kind(output[column].iloc[position]) for column in output.columns},
        })
    return {
        "columns": [str(column) for column in output.columns],
        "dtypes": {str(column): str(output[column].dtype) for column in output.columns},
        "index_dtype": str(output.index.dtype),
        "shape": [int(output.shape[0]), int(output.shape[1])],
        "records": records,
    }


def run_case(name, base, incoming, relations, timestamp_key, parameters=None):
    before = [frame.copy(deep=True) for frame in (base, incoming, relations)]
    row = {
        "name": name,
        "timestamp_key": timestamp_key,
        "base_input": describe_frame(base),
        "incoming_input": describe_frame(incoming),
        "relations_input": describe_frame(relations),
    }
    call_parameters = dict(parameters or {})
    if timestamp_key is not None:
        call_parameters[TIMESTAMP_PARAMETER] = timestamp_key
    try:
        output = algorithm.apply(base, incoming, relations, parameters=call_parameters)
        row["result"] = describe_output(output)
    except Exception as error:
        row["error"] = {
            "type": type(error).__name__,
            "message": str(error),
        }
    row["inputs_unchanged"] = all(
        frame.equals(original)
        for frame, original in zip((base, incoming, relations), before)
    )
    return row


def related_frames(base_rows, incoming_rows, relations, timestamp_key, *, parse_timestamp=False):
    base = pd.DataFrame(base_rows)
    incoming = pd.DataFrame(incoming_rows)
    if parse_timestamp:
        for frame in (base, incoming):
            frame[timestamp_key] = pd.to_datetime(frame[timestamp_key], utc=True)
    return base, incoming, pd.DataFrame(relations)


def capture_time_tables():
    rows = []
    case_key = "case:concept:name"
    relations = pd.DataFrame([{case_key + "_LEFT": "L1", case_key + "_RIGHT": "R1"}] * 2)
    for era, prefix in (("modern", "2024-01-01T00:00:00"), ("pre_epoch", "1969-12-31T23:59:59")):
        for selector, key in (("default", "time:timestamp"), ("custom", "started")):
            base = pd.DataFrame([{case_key: "L1", "concept:name": name, key: prefix + suffix}
                                 for name, suffix in (("base1", ".000000001Z"),
                                     ("base0", ".000000000Z"), ("base-last", ".999999999Z"))])
            incoming = pd.DataFrame([{case_key: "R1", "concept:name": name, key: prefix + suffix}
                                     for name, suffix in (("incoming2", ".000000002Z"),
                                                          ("incoming1", ".000000001+00:00"))])
            for frame in (base, incoming):
                frame[key] = pd.to_datetime(frame[key], format="mixed", utc=True)
            rows.append(run_case("time_" + era + "_" + selector, base, incoming, relations, key))
    for selector, key in (("default", "time:timestamp"), ("custom", "started")):
        for mode in ("mixed", "all-null", "base-column-absent", "incoming-column-absent", "both-columns-absent", "invalid-text"):
            base = pd.DataFrame({case_key: ["L1"] * 3, "concept:name": ["base-null", "base2", "base-missing"]})
            incoming = pd.DataFrame({case_key: ["R1"] * 2, "concept:name": ["incoming-null", "incoming1"]})
            for frame, values in ((base, [None, "2024-01-01T00:00:00.000000002Z", pd.NaT]),
                                  (incoming, [pd.NaT, "2024-01-01T00:00:00.000000001Z"])):
                frame[key] = pd.to_datetime([None] * len(frame) if mode == "all-null" else values, format="mixed", utc=True)
            if mode in ("base-column-absent", "both-columns-absent"):
                del base[key]
            if mode in ("incoming-column-absent", "both-columns-absent"):
                del incoming[key]
            if mode == "invalid-text":
                base[key] = ["invalid", "", None]
            rows.append(run_case("time_" + selector + "_" + mode, base, incoming, relations, key))
    return rows


def capture(checkout):
    if pm4py.__version__ != REFERENCE_VERSION:
        raise RuntimeError("This oracle requires reference version 2.7.22")
    if pd.__version__ != "3.0.5":
        raise RuntimeError("This oracle requires pandas version 3.0.5")

    relation = lambda left, right: {
        "case:concept:name_LEFT": left,
        "case:concept:name_RIGHT": right,
    }
    rows = []

    # A reversed input order across two cases distinguishes per-case grouping,
    # relation order, and one global timestamp sort.
    base, incoming, relations = related_frames(
        [
            {"case:concept:name": "L2", "concept:name": "base-L2-late", "when": "2024-01-01T00:00:05Z"},
            {"case:concept:name": "L1", "concept:name": "base-L1-late", "when": "2024-01-01T00:00:05Z"},
            {"case:concept:name": "L2", "concept:name": "base-L2-early", "when": "2024-01-01T00:00:00Z"},
            {"case:concept:name": "L1", "concept:name": "base-L1-early", "when": "2024-01-01T00:00:00Z"},
        ],
        [
            {"case:concept:name": "R2", "concept:name": "incoming-R2-late", "when": "2024-01-01T00:00:04Z"},
            {"case:concept:name": "R1", "concept:name": "incoming-R1-late", "when": "2024-01-01T00:00:04Z"},
            {"case:concept:name": "R2", "concept:name": "incoming-R2-early", "when": "2024-01-01T00:00:01Z"},
            {"case:concept:name": "R1", "concept:name": "incoming-R1-early", "when": "2024-01-01T00:00:01Z"},
        ],
        [relation("L1", "R1"), relation("L2", "R2")],
        "when",
        parse_timestamp=True,
    )
    rows.append(run_case("reversed_multi_case_global_order", base, incoming, relations, "when"))

    # Reverse the relation rows and include unrelated L0/L3 base groups.  The
    # result establishes whether the final table is globally case-sorted or
    # follows relation-row order when those orders disagree.
    base, incoming, relations = related_frames(
        [
            {"case:concept:name": "L3", "concept:name": "base-L3", "when": "0"},
            {"case:concept:name": "L2", "concept:name": "base-L2", "when": "2"},
            {"case:concept:name": "L0", "concept:name": "base-L0", "when": "0"},
            {"case:concept:name": "L1", "concept:name": "base-L1", "when": "1"},
        ],
        [
            {"case:concept:name": "R2", "concept:name": "incoming-R2", "when": "3"},
            {"case:concept:name": "R1", "concept:name": "incoming-R1", "when": "2"},
        ],
        [relation("L2", "R2"), relation("L1", "R1")],
        "when",
    )
    rows.append(run_case("reversed_relation_order_with_unrelated_base", base, incoming, relations, "when"))

    # Numeric timestamp keys are accepted as ordinary sortable pandas columns.
    base, incoming, relations = related_frames(
        [
            {"case:concept:name": "L1", "concept:name": "base-rank-2", "rank": 2},
            {"case:concept:name": "L1", "concept:name": "base-rank-1", "rank": 1},
            {"case:concept:name": "L1", "concept:name": "base-rank-tie", "rank": 2},
        ],
        [
            {"case:concept:name": "R1", "concept:name": "incoming-rank-3", "rank": 3},
            {"case:concept:name": "R1", "concept:name": "incoming-rank-1", "rank": 1},
            {"case:concept:name": "R1", "concept:name": "incoming-rank-tie", "rank": 2},
        ],
        [relation("L1", "R1")],
        "rank",
    )
    rows.append(run_case("numeric_integer_timestamp", base, incoming, relations, "rank"))

    base, incoming, relations = related_frames(
        [
            {"case:concept:name": "L1", "concept:name": "base-float-1.5", "rank": 1.5},
            {"case:concept:name": "L1", "concept:name": "base-float-1.0", "rank": 1.0},
        ],
        [
            {"case:concept:name": "R1", "concept:name": "incoming-float-2.0", "rank": 2.0},
            {"case:concept:name": "R1", "concept:name": "incoming-float-0.5", "rank": 0.5},
        ],
        [relation("L1", "R1")],
        "rank",
    )
    rows.append(run_case("numeric_float_timestamp", base, incoming, relations, "rank"))

    # These are deliberately non-date strings.  Successful output proves that
    # the reference delegates ordering to pandas' lexical object sort.
    base, incoming, relations = related_frames(
        [
            {"case:concept:name": "L1", "concept:name": "base-z", "when": "z"},
            {"case:concept:name": "L1", "concept:name": "base-a", "when": "a"},
            {"case:concept:name": "L1", "concept:name": "base-m", "when": "m"},
        ],
        [
            {"case:concept:name": "R1", "concept:name": "incoming-b", "when": "b"},
            {"case:concept:name": "R1", "concept:name": "incoming-y", "when": "y"},
        ],
        [relation("L1", "R1")],
        "when",
    )
    rows.append(run_case("string_timestamp_lexical", base, incoming, relations, "when"))

    base, incoming, relations = related_frames(
        [
            {"case:concept:name": "L1", "concept:name": "base-ten", "when": "10"},
            {"case:concept:name": "L1", "concept:name": "base-two", "when": "2"},
        ],
        [
            {"case:concept:name": "R1", "concept:name": "incoming-one", "when": "1"},
            {"case:concept:name": "R1", "concept:name": "incoming-twenty", "when": "20"},
        ],
        [relation("L1", "R1")],
        "when",
    )
    rows.append(run_case("string_numeric_looking_timestamp_lexical", base, incoming, relations, "when"))

    # Typed datetimes retain their pandas datetime dtype and chronological sort.
    base, incoming, relations = related_frames(
        [
            {"case:concept:name": "L1", "concept:name": "base-late", "started": "2024-01-02T00:00:00Z"},
            {"case:concept:name": "L1", "concept:name": "base-early", "started": "2024-01-01T00:00:00Z"},
        ],
        [
            {"case:concept:name": "R1", "concept:name": "incoming-mid", "started": "2024-01-01T12:00:00Z"},
            {"case:concept:name": "R1", "concept:name": "incoming-latest", "started": "2024-01-03T00:00:00Z"},
        ],
        [relation("L1", "R1")],
        "started",
        parse_timestamp=True,
    )
    rows.append(run_case("typed_datetime_timestamp_custom_selector", base, incoming, relations, "started"))

    # A mixed object column is kept as a separate case so its public error can
    # be compared with the successful homogeneous numeric/string cases.
    base, incoming, relations = related_frames(
        [
            {"case:concept:name": "L1", "concept:name": "base-number", "when": 2},
            {"case:concept:name": "L1", "concept:name": "base-number-low", "when": 1},
        ],
        [
            {"case:concept:name": "R1", "concept:name": "incoming-string", "when": "3"},
            {"case:concept:name": "R1", "concept:name": "incoming-string-low", "when": "0"},
        ],
        [relation("L1", "R1")],
        "when",
    )
    rows.append(run_case("mixed_numeric_string_timestamp", base, incoming, relations, "when"))

    base, incoming, relations = related_frames(
        [
            {"case:concept:name": "L1", "concept:name": "base-date", "when": pd.Timestamp("2024-01-02", tz="UTC")},
            {"case:concept:name": "L1", "concept:name": "base-date-early", "when": pd.Timestamp("2024-01-01", tz="UTC")},
        ],
        [
            {"case:concept:name": "R1", "concept:name": "incoming-text", "when": "2024-01-03"},
            {"case:concept:name": "R1", "concept:name": "incoming-text-early", "when": "2023-12-31"},
        ],
        [relation("L1", "R1")],
        "when",
    )
    rows.append(run_case("mixed_datetime_string_timestamp", base, incoming, relations, "when"))

    # Missing IDs are tested on each side and in the relation table.  The null
    # rows are intentionally present in otherwise valid one-to-one inputs.
    base, incoming, relations = related_frames(
        [
            {"case:concept:name": None, "concept:name": "base-null-id", "when": "a"},
            {"case:concept:name": "L1", "concept:name": "base-valid", "when": "b"},
        ],
        [
            {"case:concept:name": "R1", "concept:name": "incoming-valid", "when": "c"},
        ],
        [relation("L1", "R1")],
        "when",
    )
    rows.append(run_case("missing_case_id_in_base", base, incoming, relations, "when"))

    base, incoming, relations = related_frames(
        [
            {"case:concept:name": "L1", "concept:name": "base-valid", "when": "a"},
        ],
        [
            {"case:concept:name": None, "concept:name": "incoming-null-id", "when": "b"},
            {"case:concept:name": "R1", "concept:name": "incoming-valid", "when": "c"},
        ],
        [relation("L1", "R1")],
        "when",
    )
    rows.append(run_case("missing_case_id_in_incoming", base, incoming, relations, "when"))

    base, incoming, relations = related_frames(
        [
            {"case:concept:name": "L1", "concept:name": "base-valid", "when": "a"},
        ],
        [
            {"case:concept:name": "R1", "concept:name": "incoming-valid", "when": "b"},
        ],
        [relation(None, "R1"), relation("L1", None), relation("L1", "R1")],
        "when",
    )
    rows.append(run_case("missing_case_id_in_relations", base, incoming, relations, "when"))

    base, incoming, relations = related_frames(
        [
            {"case:concept:name": "L1", "concept:name": "base-valid", "when": "a"},
        ],
        [
            {"case:concept:name": "R1", "concept:name": "incoming-valid", "when": "b"},
        ],
        [relation(float("nan"), "R1"), relation("L1", float("nan")), relation("L1", "R1")],
        "when",
    )
    rows.append(run_case("nan_case_id_in_relations", base, incoming, relations, "when"))

    # Duplicate valid relations expose whether the result is a multiset of
    # relation expansions; the unmatched row is captured independently.
    base, incoming, relations = related_frames(
        [
            {"case:concept:name": "L1", "concept:name": "base-valid", "when": "a"},
        ],
        [
            {"case:concept:name": "R1", "concept:name": "incoming-valid", "when": "b"},
        ],
        [relation("L1", "R1"), relation("L1", "R1")],
        "when",
    )
    rows.append(run_case("duplicate_valid_relations", base, incoming, relations, "when"))

    base, incoming, relations = related_frames(
        [
            {"case:concept:name": "L1", "concept:name": "base-valid", "when": "a"},
        ],
        [
            {"case:concept:name": "R1", "concept:name": "incoming-valid", "when": "b"},
        ],
        [relation("L9", "R9"), relation("L1", "R1")],
        "when",
    )
    rows.append(run_case("unmatched_relation_with_valid_relation", base, incoming, relations, "when"))

    # A second custom selector exercises a non-default column name with
    # homogeneous lexical values, while the datetime selector above covers a
    # typed custom column.
    base, incoming, relations = related_frames(
        [
            {"case:concept:name": "L1", "concept:name": "base-completed", "completed_at": "late"},
            {"case:concept:name": "L1", "concept:name": "base-completed-early", "completed_at": "early"},
        ],
        [
            {"case:concept:name": "R1", "concept:name": "incoming-completed", "completed_at": "middle"},
        ],
        [relation("L1", "R1")],
        "completed_at",
    )
    rows.append(run_case("custom_string_timestamp_selector", base, incoming, relations, "completed_at"))

    # Relation metadata is appended to every output row.  Name collisions use
    # pandas' _x/_y suffixes: the incoming event field is _x and the relation
    # field is _y when the base already owns that name.
    base, incoming, relations = related_frames(
        [
            {"case:concept:name": "L1", "concept:name": "base", "when": "a", "link": "base-link"},
        ],
        [
            {"case:concept:name": "R1", "concept:name": "incoming", "when": "b", "link": "incoming-link"},
        ],
        [{
            "case:concept:name_LEFT": "L1",
            "case:concept:name_RIGHT": "R1",
            "link": "relation-link",
            "extra": 42,
        }],
        "when",
    )
    rows.append(run_case("relation_extra_with_incoming_collision", base, incoming, relations, "when"))

    # Case identifiers are compared through pandas' merge keys; unlike a
    # numeric-to-numeric relation, a string relation key against int64 event
    # keys raises a public ValueError.
    base, incoming, relations = related_frames(
        [
            {"case:concept:name": 1, "concept:name": "base", "when": "a"},
        ],
        [
            {"case:concept:name": 2, "concept:name": "incoming", "when": "b"},
        ],
        [{"case:concept:name_LEFT": "1", "case:concept:name_RIGHT": "2"}],
        "when",
    )
    rows.append(run_case("numeric_case_ids_string_relation_ids", base, incoming, relations, "when"))

    # With object-typed event IDs, a homogeneous numeric relation table can
    # still match the numeric subset while leaving string-ID events unmatched.
    base = pd.DataFrame({
        "case:concept:name": pd.Series([1, "1"], dtype=object),
        "concept:name": ["base-int", "base-str"],
        "when": ["a", "b"],
    })
    incoming = pd.DataFrame({
        "case:concept:name": pd.Series([2, "2"], dtype=object),
        "concept:name": ["incoming-int", "incoming-str"],
        "when": ["c", "d"],
    })
    relations = pd.DataFrame({
        "case:concept:name_LEFT": pd.Series([1], dtype="int64"),
        "case:concept:name_RIGHT": pd.Series([2], dtype="int64"),
    })
    rows.append(run_case("mixed_object_event_ids_numeric_relations", base, incoming, relations, "when"))

    # Fully object-typed event and relation IDs match both numeric and string
    # relation rows independently.
    relations = pd.DataFrame({
        "case:concept:name_LEFT": pd.Series([1, "1"], dtype=object),
        "case:concept:name_RIGHT": pd.Series([2, "2"], dtype=object),
    })
    rows.append(run_case("mixed_object_event_ids_mixed_relations", base, incoming, relations, "when"))

    # An empty, correctly shaped relation table retains all base events and
    # emits the two canonical relation columns as null-valued columns.
    base, incoming, relations = related_frames(
        [
            {"case:concept:name": "L1", "concept:name": "base-one", "when": "a"},
            {"case:concept:name": "L2", "concept:name": "base-two", "when": "b"},
        ],
        [
            {"case:concept:name": "R1", "concept:name": "incoming", "when": "c"},
        ],
        [],
        "when",
    )
    relations = pd.DataFrame(columns=["case:concept:name_LEFT", "case:concept:name_RIGHT"])
    rows.append(run_case("empty_relation_table", base, incoming, relations, "when"))

    # pandas merge treats null keys as equal.  An object-typed null incoming
    # case therefore matches a null RIGHT relation and receives the LEFT case
    # identifier in the output.
    base = pd.DataFrame([
        {"case:concept:name": "L1", "concept:name": "base", "when": "a"},
    ])
    incoming = pd.DataFrame({
        "case:concept:name": pd.Series([None], dtype=object),
        "concept:name": pd.Series(["incoming-null-right"], dtype=object),
        "when": pd.Series(["b"], dtype=object),
    })
    relations = pd.DataFrame({
        "case:concept:name_LEFT": pd.Series(["L1"], dtype=object),
        "case:concept:name_RIGHT": pd.Series([None], dtype=object),
    })
    rows.append(run_case("null_right_matches_null_incoming", base, incoming, relations, "when"))

    # Datetime and numeric values form an unordered pandas categorical during
    # the public merge/sort path; retain its exact public exception text.
    base, incoming, relations = related_frames(
        [
            {"case:concept:name": "L1", "concept:name": "base-date", "when": pd.Timestamp("2024-01-01")},
        ],
        [
            {"case:concept:name": "R1", "concept:name": "incoming-number", "when": 1},
        ],
        [relation("L1", "R1")],
        "when",
    )
    rows.append(run_case("mixed_datetime_numeric_timestamp", base, incoming, relations, "when"))

    # Boolean timestamps coerce with integer timestamps; False/0 and True/1
    # retain stable base-then-incoming tie order after numeric sorting.
    base = pd.DataFrame({
        "case:concept:name": ["L1", "L1"],
        "concept:name": ["base-false", "base-true"],
        "when": pd.Series([False, True], dtype=bool),
    })
    incoming = pd.DataFrame({
        "case:concept:name": ["R1", "R1"],
        "concept:name": ["incoming-zero", "incoming-one"],
        "when": pd.Series([0, 1], dtype="int64"),
    })
    relations = pd.DataFrame([relation("L1", "R1")])
    rows.append(run_case("bool_numeric_timestamp", base, incoming, relations, "when"))

    # An object column containing an int above 2**53 and a float that rounds to
    # the same comparison value exposes the exact public categorical failure.
    base = pd.DataFrame({
        "case:concept:name": ["L1"],
        "concept:name": ["base-large-int"],
        "when": pd.Series([9007199254740993], dtype=object),
    })
    incoming = pd.DataFrame({
        "case:concept:name": ["R1"],
        "concept:name": ["incoming-large-float"],
        "when": pd.Series([9007199254740992.0], dtype=object),
    })
    relations = pd.DataFrame([relation("L1", "R1")])
    rows.append(run_case("large_int_float_object_timestamp_precision_collision", base, incoming, relations, "when"))

    for name, incoming_extra, relation_extra in (
            ("incoming_preexisting_link_x", {"link_x": "original-x"}, {}),
            ("relation_preexisting_link_y", {}, {"link_y": "original-y"})):
        base = pd.DataFrame([{"case:concept:name": "L1", "concept:name": "base", "when": "a"}])
        incoming = pd.DataFrame([{"case:concept:name": "R1", "concept:name": "incoming", "when": "b",
                                  "link": "incoming-link", **incoming_extra}])
        relations = pd.DataFrame([{**relation("L1", "R1"), "link": "relation-link", **relation_extra}])
        rows.append(run_case(name, base, incoming, relations, "when"))

    time_api = runpy.run_path(str(Path(__file__).with_name("capture-case-relations-time-reference.py")))
    time_reference = time_api["capture"](checkout)
    time_tables = capture_time_tables()
    for observed, legacy in zip(time_tables, time_reference["rows"], strict=True):
        if "error" in legacy:
            if observed.get("error", {}).get("type") != legacy["error"]:
                raise RuntimeError("Time table and timestamp oracle disagree")
        elif [record["values"]["concept:name"] for record in observed["result"]["records"]] != legacy["activities"]:
            raise RuntimeError("Time table and timestamp oracle disagree")
    rows.extend(time_tables)
    return {
        "reference_version": pm4py.__version__,
        "pandas_version": pd.__version__,
        "reference_provenance": time_reference["reference_provenance"],
        "time_reference": time_reference,
        "rows": rows,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--reference-checkout", type=Path, required=True)
    args = parser.parse_args()
    result = capture(args.reference_checkout)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({
        "reference_version": result["reference_version"],
        "pandas_version": result["pandas_version"],
        "row_count": len(result["rows"]),
    }))
