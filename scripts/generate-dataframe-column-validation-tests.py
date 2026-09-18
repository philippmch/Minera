"""Generate public column-validation and validated replay checks."""

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import runpy

FMT = runpy.run_path(
    str(Path(__file__).with_name("generate-format-dataframe-tests.py"))
)
REPLAY = runpy.run_path(
    str(Path(__file__).with_name("generate-token-replay-dataframe-grouping-tests.py"))
)
Q = FMT["QUOTE"]
NAMES = (
    "valid",
    "valid_no_selectors",
    "empty_no_columns",
    "two_strings_only",
    "one_string_date",
    "case_object",
    "case_str",
    "case_string",
    "case_int64",
    "case_float64",
    "case_bool",
    "case_category",
    "case_none",
    "case_nan",
    "case_empty",
    "case_space",
    "activity_object",
    "activity_str",
    "activity_string",
    "activity_int64",
    "activity_float64",
    "activity_bool",
    "activity_category",
    "activity_none",
    "activity_nan",
    "activity_empty",
    "activity_space",
    "timestamp_object",
    "timestamp_str",
    "timestamp_string",
    "timestamp_int64",
    "timestamp_float64",
    "timestamp_bool",
    "timestamp_category",
    "timestamp_none",
    "timestamp_nan",
    "timestamp_empty",
    "timestamp_space",
    "missing_case_id_key",
    "missing_activity_key",
    "missing_timestamp_key",
    "selected_case_int64",
    "selected_case_category",
    "selected_case_object",
    "selected_activity_int64",
    "selected_activity_category",
    "selected_activity_object",
    "selected_timestamp_int64",
    "selected_timestamp_category",
    "selected_timestamp_object",
    "selected_timestamp_nat",
    "selected_timestamp_valid",
    "start_valid",
    "start_null",
    "start_text",
    "start_absent",
    "empty_selector_case_id_key",
    "empty_selector_activity_key",
    "empty_selector_timestamp_key",
    "all_null",
    "null_no_selectors",
    "typed_empty",
    "attrs_preserved",
    "shared_selector",
)
ERRORS = {
    "empty_no_columns": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column for the case identifier, a column for the activity and a column for the timestamp.",
    },
    "two_strings_only": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column for the case identifier, a column for the activity and a column for the timestamp.",
    },
    "one_string_date": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column for the case identifier, a column for the activity and a column for the timestamp.",
    },
    "case_int64": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column of type string for the case identifier and a column of type string for the activity.",
    },
    "case_float64": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column of type string for the case identifier and a column of type string for the activity.",
    },
    "case_bool": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column of type string for the case identifier and a column of type string for the activity.",
    },
    "case_category": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column of type string for the case identifier and a column of type string for the activity.",
    },
    "case_none": {
        "type": "Exception",
        "message": "the case ID column should not contain any empty value.",
    },
    "case_nan": {
        "type": "Exception",
        "message": "the case ID column should not contain any empty value.",
    },
    "activity_int64": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column of type string for the case identifier and a column of type string for the activity.",
    },
    "activity_float64": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column of type string for the case identifier and a column of type string for the activity.",
    },
    "activity_bool": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column of type string for the case identifier and a column of type string for the activity.",
    },
    "activity_category": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column of type string for the case identifier and a column of type string for the activity.",
    },
    "activity_none": {
        "type": "Exception",
        "message": "the activity column should not contain any empty value.",
    },
    "activity_nan": {
        "type": "Exception",
        "message": "the activity column should not contain any empty value.",
    },
    "timestamp_object": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column of type date",
    },
    "timestamp_str": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column of type date",
    },
    "timestamp_string": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column of type date",
    },
    "timestamp_int64": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column of type date",
    },
    "timestamp_float64": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column of type date",
    },
    "timestamp_bool": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column of type date",
    },
    "timestamp_category": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column of type date",
    },
    "timestamp_none": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column of type date",
    },
    "timestamp_nan": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column of type date",
    },
    "timestamp_empty": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column of type date",
    },
    "timestamp_space": {
        "type": "Exception",
        "message": "the dataframe should (at least) contain a column of type date",
    },
    "missing_case_id_key": {
        "type": "Exception",
        "message": "the specified case ID column is not contained in the dataframe. Available columns: ['case:concept:name', 'concept:name', 'time:timestamp']",
    },
    "missing_activity_key": {
        "type": "Exception",
        "message": "the specified activity column is not contained in the dataframe. Available columns: ['case:concept:name', 'concept:name', 'time:timestamp']",
    },
    "missing_timestamp_key": {
        "type": "Exception",
        "message": "the specified timestamp column is not contained in the dataframe. Available columns: ['case:concept:name', 'concept:name', 'time:timestamp']",
    },
    "selected_case_int64": {
        "type": "Exception",
        "message": "the case ID column should be of type string.",
    },
    "selected_case_category": {
        "type": "Exception",
        "message": "the case ID column should be of type string.",
    },
    "selected_case_object": {
        "type": "Exception",
        "message": "the case ID column should not contain any empty value.",
    },
    "selected_activity_int64": {
        "type": "Exception",
        "message": "the activity column should be of type string.",
    },
    "selected_activity_category": {
        "type": "Exception",
        "message": "the activity column should be of type string.",
    },
    "selected_activity_object": {
        "type": "Exception",
        "message": "the activity column should not contain any empty value.",
    },
    "selected_timestamp_int64": {
        "type": "Exception",
        "message": "the timestamp column should be of time datetime. Use the function pandas.to_datetime",
    },
    "selected_timestamp_category": {
        "type": "Exception",
        "message": "the timestamp column should be of time datetime. Use the function pandas.to_datetime",
    },
    "selected_timestamp_object": {
        "type": "Exception",
        "message": "the timestamp column should be of time datetime. Use the function pandas.to_datetime",
    },
    "selected_timestamp_nat": {
        "type": "Exception",
        "message": "the timestamp column should not contain any empty value.",
    },
    "start_null": {
        "type": "Exception",
        "message": "the start timestamp column should not contain any empty value.",
    },
    "start_text": {
        "type": "Exception",
        "message": "the start timestamp column should be of time datetime. Use the function pandas.to_datetime",
    },
    "start_absent": {
        "type": "Exception",
        "message": "the specified start timestamp column is not contained in the dataframe. Available columns: ['case:concept:name', 'concept:name', 'extra_date', 'extra_string', 'time:timestamp']",
    },
    "empty_selector_case_id_key": {
        "type": "Exception",
        "message": "the specified case ID column is not contained in the dataframe. Available columns: ['case:concept:name', 'concept:name', 'extra_date', 'extra_string', 'time:timestamp']",
    },
    "empty_selector_activity_key": {
        "type": "Exception",
        "message": "the specified activity column is not contained in the dataframe. Available columns: ['case:concept:name', 'concept:name', 'extra_date', 'extra_string', 'time:timestamp']",
    },
    "empty_selector_timestamp_key": {
        "type": "Exception",
        "message": "the specified timestamp column is not contained in the dataframe. Available columns: ['case:concept:name', 'concept:name', 'extra_date', 'extra_string', 'time:timestamp']",
    },
    "all_null": {
        "type": "Exception",
        "message": "the case ID column should not contain any empty value.",
    },
}


def generate(raw, replay_raw):
    report = json.loads(raw)
    replay = json.loads(replay_raw)
    REPLAY["generate"](replay_raw)
    if (
        report["capture_id"] != "dataframe_column_validation"
        or report["reference_version"] != "2.7.22"
        or report["pandas_version"] != "3.0.5"
    ):
        raise ValueError("Wrong capture/runtime")
    digest = "c5959354a50cba2b9792c40db0012813d4281f61d11a8fa0da5ee9db5860ed64"
    if report["reference_provenance"] != {
        "commit": FMT["PIN"],
        "function": "check_pandas_dataframe_columns",
        "module": "pm4py.util.pandas_utils",
        "signature": "(df, activity_key=None, case_id_key=None, timestamp_key=None, start_timestamp_key=None)",
        "method": "opaque module bytes, CRLF normalized to LF, no source decoding",
        "installed_sha256": digest,
        "pinned_sha256": digest,
    }:
        raise ValueError("Unpinned callable")
    rows = report["rows"]
    if report["case_count"] != len(NAMES) or tuple(r["name"] for r in rows) != NAMES:
        raise ValueError("Incomplete manifest")
    out = (
        "// Validation report SHA256: "
        + hashlib.sha256(raw).hexdigest()
        + "\n// Replay report SHA256: "
        + hashlib.sha256(replay_raw).hexdigest()
        + '\n#include "minera/dataframe_validation.hpp"\n#include "minera/conformance.hpp"\n'
        + FMT["DTYPE"]["PREAMBLE"]
        + "\n}\n"
        + FMT["PREAMBLE"].replace(
            "namespace format_dataframe_reference",
            "namespace dataframe_validation_reference",
        )
    )
    names = []
    for i, row in enumerate(rows):
        if (
            row["input"] != row["input_after"]
            or row.get("error") != ERRORS.get(row["name"])
            or ("result" in row) == ("error" in row)
            or ("result" in row and row["result"] is not None)
        ):
            raise ValueError("Unexpected outcome/mutation")
        name = "check_validation_" + str(i)
        names.append(name)
        out += (
            "inline void "
            + name
            + "() {\n"
            + FMT["frame_code"]("input", row["input"])
            + FMT["frame_code"]("expected_input", row["input_after"])
            + "  DataFrameColumnOptions options;\n"
        )
        for key, value in row["parameters"].items():
            if key not in (
                "case_id_key",
                "activity_key",
                "timestamp_key",
                "start_timestamp_key",
            ):
                raise ValueError("Unmapped selector")
            out += (
                "  options."
                + key
                + " = "
                + ("std::nullopt" if value is None else Q(value))
                + ";\n"
            )
        out += "  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }\n"
        expected = row.get("error", {}).get("message", "")
        out += (
            "  if (error != "
            + Q(expected)
            + ") throw std::runtime_error("
            + Q(row["name"] + ": validation outcome differs: ")
            + " + error);\n  check_format_table("
            + Q(row["name"] + ": input after")
            + ", input, expected_input);\n}\n"
        )
    for i, row in enumerate(replay["rows"]):
        name = "check_validated_replay_" + str(i)
        names.append(name)
        out += (
            "inline void "
            + name
            + "() {\n"
            + FMT["frame_code"]("input", row["input"])
            + FMT["frame_code"]("expected_input", row["input_after"])
        )
        out += (
            "  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"
            + ", ".join(map(Q, REPLAY["NET"]["labels"]))
            + "}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});\n"
        )
        if "error" in row:
            out += (
                "  std::string error; try { (void)replay_dataframe_traces(input,net); } catch (const std::exception& caught) { error=caught.what(); }\n  if (error != "
                + Q(row["error"]["message"])
                + ") throw std::runtime_error("
                + Q(row["name"] + ": validated replay rejection differs")
                + ");\n"
            )
        else:
            out += (
                "  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != "
                + str(len(row["result"]))
                + ") throw std::runtime_error("
                + Q(row["name"] + ": replay count differs")
                + ");\n"
            )
            for j, trace in enumerate(row["result"]):
                indices = [
                    REPLAY["NET"]["labels"].index(label)
                    for label in trace["activities"]
                ]
                out += (
                    "  { const auto& trace=actual["
                    + str(j)
                    + "]; const std::vector<std::size_t> path={"
                    + ",".join(map(str, indices))
                    + "};\n"
                )
                terms = [
                    "trace.activated_transitions != path",
                    "trace.trace_is_fit != " + str(trace["fit"]).lower(),
                    "trace.trace_fitness != " + repr(trace["fitness"]),
                ] + [
                    "trace." + k + " != " + str(trace[k])
                    for k in ("consumed", "produced", "missing", "remaining")
                ]
                out += (
                    "    if ("
                    + " || ".join(terms)
                    + ") throw std::runtime_error("
                    + Q(row["name"] + ": replay diagnostics differ")
                    + "); }\n"
                )
        out += (
            "  check_format_table("
            + Q(row["name"] + ": input after")
            + ", input, expected_input);\n}\n"
        )
    out += "}\ninline void check_public_dataframe_validation() {\n"
    for name in names:
        out += "  dataframe_validation_reference::" + name + "();\n"
    return out + "}\n"


def read(path):
    raw = path.read_bytes()
    return gzip.decompress(raw) if path.suffix == ".gz" else raw


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("replay_reference", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_text(
        generate(read(args.reference), read(args.replay_reference)),
        encoding="utf-8",
        newline="\n",
    )
