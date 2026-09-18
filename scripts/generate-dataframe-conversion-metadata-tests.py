"""Generate metadata-only conversion checks from pinned public observations."""

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import runpy

FMT = runpy.run_path(
    str(Path(__file__).with_name("generate-format-dataframe-tests.py"))
)
Q = FMT["QUOTE"]
NAMES = tuple(
    name + "_" + target
    for name in (
        "raw",
        "case_property",
        "nested_attrs",
        "origin_property",
        "formatted",
        "custom_case",
        "custom_formatted",
        "empty",
        "empty_attrs",
        "empty_formatted",
        "existing_stream",
    )
    for target in ("log", "stream")
)
MODULES = {
    "convert_to_event_log": (
        "pm4py.convert",
        "1c3db08a9f48110a2309ee8fedec64c3138f7c16164573a1c2827f1df67605db",
        "(obj: Union[pandas.DataFrame, pm4py.objects.log.obj.EventStream], case_id_key: str = 'case:concept:name', **kwargs) -> pm4py.objects.log.obj.EventLog",
    ),
    "convert_to_event_stream": (
        "pm4py.convert",
        "1c3db08a9f48110a2309ee8fedec64c3138f7c16164573a1c2827f1df67605db",
        "(obj: Union[pm4py.objects.log.obj.EventLog, pandas.DataFrame], case_id_key: str = 'case:concept:name', **kwargs) -> pm4py.objects.log.obj.EventStream",
    ),
    "format_dataframe": (
        "pm4py.utils",
        FMT["MODULE_HASH"],
        "(df, case_id: str = 'case:concept:name', activity_key: str = 'concept:name', timestamp_key: str = 'time:timestamp', start_timestamp_key: str = 'start_timestamp', timest_format: Optional[str] = None) -> Any",
    ),
}


def attrs(values):
    return (
        "{"
        + ", ".join(FMT["property_value"](key, value) for key, value in values.items())
        + "}"
    )


def metadata(name, value):
    if value["classifiers"] or value["omni_present"]:
        raise ValueError("Unmapped metadata")
    result = "  EventDataMetadata " + name + ";\n"
    result += "  " + name + ".properties = " + attrs(value["properties"]) + ";\n"
    result += (
        "  "
        + name
        + ".extensions = {"
        + ", ".join(
            "{" + ", ".join(Q(v) for v in (key, ext["prefix"], ext["uri"])) + "}"
            for key, ext in value["extensions"].items()
        )
        + "};\n"
    )
    return result


def generate(raw):
    report = json.loads(raw)
    if (
        report["capture_id"] != "dataframe_conversion_metadata"
        or report["reference_version"] != "2.7.22"
        or report["pandas_version"] != "3.0.5"
    ):
        raise ValueError("Wrong capture/runtime")
    proof = report["reference_provenance"]
    if [v["function"] for v in proof] != list(MODULES):
        raise ValueError("Incomplete public provenance")
    for value in proof:
        module, digest, signature = MODULES[value["function"]]
        if (
            value["module"] != module
            or value["installed_sha256"] != digest
            or value["pinned_sha256"] != digest
            or value["commit"] != FMT["PIN"]
            or value["signature"] != signature
            or value["method"]
            != "opaque module bytes, CRLF normalized to LF, no source decoding"
        ):
            raise ValueError("Unpinned public callable")
    rows = report["rows"]
    if report["case_count"] != len(NAMES) or tuple(r["name"] for r in rows) != NAMES:
        raise ValueError("Incomplete manifest")
    output = (
        '#include "minera/log_conversion.hpp"\n// Metadata public report SHA256: '
        + hashlib.sha256(raw).hexdigest()
        + "\n"
    )
    output += (
        FMT["DTYPE"]["PREAMBLE"]
        + "\n}\n"
        + FMT["PREAMBLE"].replace(
            "namespace format_dataframe_reference",
            "namespace dataframe_conversion_metadata_reference",
        )
    )
    for i, row in enumerate(rows):
        if "error" in row or row["input"] != row["input_after"]:
            raise ValueError("Unexpected failure/input mutation")
        target = row["target"]
        source = row["source"]
        if target not in ("log", "stream") or source not in ("dataframe", "stream"):
            raise ValueError("Unmapped conversion route")
        expected = row["result"]
        if (
            expected["type"] != ("EventLog" if target == "log" else "EventStream")
            or type(expected["length"]) is not int
            or expected["length"] < 0
        ):
            raise ValueError("Wrong public output type/length")
        output += "inline void check_metadata_case_" + str(i) + "() {\n"
        output += (
            "  LogConversionOptions options; options.case_id_key = "
            + Q(row["case_id_key"])
            + ";\n"
        )
        output += metadata("expected_metadata", expected)
        output += (
            "  const std::vector<EventAttribute> expected_attributes = "
            + attrs(expected["attributes"])
            + ";\n"
        )
        output += (
            "  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != "
            + str(expected["length"])
            + ") throw std::runtime_error("
            + Q(row["name"] + ": metadata/length differs")
            + "); };\n"
        )
        field = "traces" if target == "log" else "events"
        if source == "dataframe":
            output += FMT["frame_code"]("input", row["input"]) + FMT["frame_code"](
                "expected_input", row["input_after"]
            )
            output += (
                "  const auto eager = event_table_to_event_"
                + target
                + "(input, options); check(eager, eager."
                + field
                + ".size());\n"
            )
            output += (
                "  const auto lazy = lazy_event_table_to_event_"
                + target
                + "(LazyEventTable::from_table(input), options); check(lazy, lazy."
                + field
                + ".size());\n"
            )
            output += (
                "  check_format_table("
                + Q(row["name"] + ": input after")
                + ", input, expected_input);\n"
            )
        else:
            output += (
                "  EventStream input; input.attributes = "
                + attrs(row["input"]["attributes"])
                + ";\n"
                + metadata("input_metadata", row["input"])
            )
            output += '  input.metadata = input_metadata; input.events = {{{EventAttribute{"case:concept:name", "C"}, EventAttribute{"concept:name", "A"}}}};\n'
            output += (
                "  const auto actual = event_data_to_event_"
                + target
                + "(EventDataView(input), options); check(actual, actual."
                + field
                + ".size());\n"
            )
            output += '  if (input.metadata != input_metadata || input.attributes != expected_attributes) throw std::runtime_error("stream input changed");\n'
        output += "}\n"
    output += "}\ninline void check_public_dataframe_conversion_metadata() {\n"
    for i in range(len(rows)):
        output += (
            "  dataframe_conversion_metadata_reference::check_metadata_case_"
            + str(i)
            + "();\n"
        )
    return output + "}\n"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    raw = args.reference.read_bytes()
    if args.reference.suffix == ".gz":
        raw = gzip.decompress(raw)
    args.output.write_text(generate(raw), encoding="utf-8", newline="\n")
