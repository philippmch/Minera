"""Black-box LINE_BY_LINE XES lexical observations; no reference source reads."""
import argparse
import datetime
import gzip
import hashlib
import json
import os
import subprocess
import tempfile
import warnings
from pathlib import Path


REFERENCE_VERSION = "2.7.22"
REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"
REFERENCE_COMMIT_SCOPE = "configured_reference_pin"


def _load_public_reference_api():
    warnings.filterwarnings("ignore")
    import pm4py
    from pm4py.objects.log.importer.xes import importer
    return pm4py, importer


def _require_version(pm4py):
    actual = getattr(pm4py, "__version__", None)
    if actual != REFERENCE_VERSION:
        raise RuntimeError(
            f"This oracle requires pm4py=={REFERENCE_VERSION}; got {actual!r}")


def _environment():
    return {"PYTHONHASHSEED": os.environ.get("PYTHONHASHSEED"),
            "TZ": os.environ.get("TZ")}


def _file_metadata(path):
    resolved = Path(path).resolve()
    digest = hashlib.sha256()
    try:
        with resolved.open("rb") as source:
            for chunk in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(chunk)
    except OSError as error:
        raise RuntimeError(f"Unable to hash {resolved}: {error}") from error
    return {"path": str(resolved), "sha256": digest.hexdigest()}


def _module_metadata(module):
    filename = getattr(module, "__file__", None)
    return _file_metadata(filename) if filename else None


def _native_failure(kind, message, process=None):
    detail = {"error": True, "type": kind, "message": str(message)}
    record = {"kind": kind, "message": str(message)}
    if process is not None:
        stdout = process.stdout or ""
        stderr = process.stderr or ""
        detail.update(returncode=process.returncode, stdout=stdout, stderr=stderr)
        record.update(returncode=process.returncode, stdout=stdout, stderr=stderr)
    return detail, record


def _native_attributes(items):
    if not isinstance(items, list):
        raise ValueError("native attributes must be an array")
    return {item["key"]: (item["typed_value"] if "typed_value" in item
                           else item["value"]) for item in items}


def _decode_native_import(process):
    stderr = process.stderr or ""
    if process.returncode not in (0, 1) or "Usage:" in stderr:
        return _native_failure(
            "native_process", f"Native executable failed before import: {stderr[:400]}", process)
    if process.returncode:
        return {"error": True, "message": stderr}, None
    try:
        log = json.loads(process.stdout)["log"]
        traces = log["traces"]
        result = dict(attributes=_native_attributes(log["attributes"]), traces=[])
        for trace in traces:
            events = []
            for event in trace["events"]:
                events.append(_native_attributes(event["attributes"]))
            result["traces"].append(dict(
                attributes=_native_attributes(trace["attributes"]), events=events))
        return result, None
    except Exception as error:
        return _native_failure("native_malformed_output", error, process)


def normalize(value):
    if isinstance(value, dict):
        return {str(k): normalize(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [normalize(v) for v in value]
    if isinstance(value, datetime.datetime):
        return {"date": value.isoformat()}
    return value


def fixtures():
    first = '<string key="a" value="A"/>'
    second = '<string key="b" value="B"/>'
    base = ['<log>', '<trace>', '<event>', first, second, '</event>', '</trace>', '</log>']
    result = {"ordinary": '\n'.join(base), "compact": ''.join(base),
              "indented": '\n'.join('  ' + line for line in base),
              "crlf": '\r\n'.join(base), "cr": '\r'.join(base)}
    for index in range(len(base) - 1):
        result[f"join_{index}_{index+1}"] = '\n'.join(base[:index] + [base[index] + base[index+1]] + base[index+2:])
    bodies = {
        "single_quotes": "<string key='a' value='A'/>",
        "spaces_equal": '<string key = "a" value = "A"/>',
        "swapped_attributes": '<string value="A" key="a"/>',
        "extra_attribute": '<string key="a" extra="X" value="A"/>',
        "spaces_tag": '<string   key="a" value="A" />',
        "multiline_attribute": '<string key="a"\nvalue="A"/>',
        "explicit_close": '<string key="a" value="A"></string>',
        "separate_close": '<string key="a" value="A">\n</string>',
        "nested_separate": '<boolean key="probe" value="true">\n' + first + '\n' + second + '\n</boolean>',
        "nested_packed": '<boolean key="probe" value="true">' + first + second + '</boolean>',
        "nested_open_packed": '<boolean key="probe" value="true">' + first + '\n' + second + '\n</boolean>',
        "values_separate": '<boolean key="probe" value="true">\n<values>\n' + first + '\n</values>\n</boolean>',
        "entities": '<string key="a&amp;b" value="&lt;x&gt;&quot;&apos;&amp;"/>',
        "numeric_entities": '<string key="a" value="&#65;&#x42;"/>',
        "namespaced": '<xes:string xmlns:xes="urn:x" key="a" value="A"/>',
        "comment": '<!-- <string key="a" value="A"/> -->',
        "empty_value": '<string key="a" value=""/>',
        "missing_value": '<string key="a"/>',
        "empty_key": '<string key="" value="A"/>',
        "list_separate": '<list key="probe">\n<values>\n' + first + '\n</values>\n</list>',
        "int_bad": '<int key="a" value="bad"/>',
        "boolean_explicit_close": '<boolean key="a" value="true"></boolean>',
        "boolean_wrong_close": '<boolean key="a" value="true"></unknown>',
        "unknown_boolean_close": '<unknown key="a" value="true"></boolean>',
        "boolean_prefix": 'prefix <boolean key="a" value="true"/>',
        "boolean_comment": '<!-- <boolean key="a" value="true"/> -->',
        "float_bad": '<float key="a" value="bad"/>',
        "unknown_tag": '<unknown key="a" value="A"/>',
        "id_tag": '<id key="a" value="A"/>',
        "prefix_text": 'prefix ' + first,
        "tag_name_text": '<stringish key="a" value="A"/>',
        "trace_close_spaces": first + '\n</event >\n<event>',
    }
    for name, body in bodies.items():
        result[name] = '<log>\n<trace>\n<event>\n' + body + '\n</event>\n</trace>\n</log>'
    result.update({
        "trace_attribute": '<log>\n<trace>\n' + first + '\n<event>\n' + second + '\n</event>\n</trace>\n</log>',
        "log_attribute": '<log>\n' + first + '\n<trace>\n<event>\n' + second + '\n</event>\n</trace>\n</log>',
        "event_without_trace": '<log>\n<event>\n' + first + '\n</event>\n</log>',
        "unclosed_trace": '<log>\n<trace>\n<event>\n' + first + '\n</event>\n</log>',
        "invalid_unclosed_trace": '<log>\n<trace>\n<event>\n<int key="a" value="bad"/>\n</event>\n</log>',
        "invalid_outside_trace": '<log>\n<int key="a" value="bad"/>\n<trace>\n<event>\n' + first + '\n</event>\n</trace>\n</log>',
        "unclosed_event": '<log>\n<trace>\n<event>\n' + first + '\n</trace>\n</log>',
        "self_closing_trace": '<log>\n<trace/>\n</log>',
        "self_closing_event": '<log>\n<trace>\n<event/>\n</trace>\n</log>',
        "metadata": '<log>\n<extension name="Concept" prefix="concept" uri="urn:concept"/>\n<classifier name="Activity" keys="concept:name"/>\n<global scope="event">\n' + first + '\n</global>\n<trace>\n<event>\n' + second + '\n</event>\n</trace>\n</log>',
    })
    for name, opening, closing in (
        ("struct_prefix", 'noise <event>', 'noise </event>'),
        ("struct_suffix", '<event> noise', '</event> noise'),
        ("struct_spaces", '<event >', '</event >'),
        ("struct_comment", '<!-- <event> -->', '<!-- </event> -->'),
        ("struct_two_open", '<trace><event>', '</event>'),
        ("struct_two_close", '<event>', '</trace></event>'),
    ):
        result[name] = '<log>\n<trace>\n' + opening + '\n' + first + '\n' + closing + '\n</trace>\n</log>'
    result["two_trace_open"] = '<log>\n<trace>\n<trace>\n<event>\n' + first + '\n</event>\n</trace>\n</trace>\n</log>'
    result["two_event_open"] = '<log>\n<trace>\n<event>\n' + first + '\n<event>\n' + second + '\n</event>\n</event>\n</trace>\n</log>'
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--native", type=Path)
    args = parser.parse_args()
    pm4py, importer = _load_public_reference_api()
    _require_version(pm4py)
    errors = []
    rows = []
    for name, xml in fixtures().items():
        for compressed in (False, True):
            try:
                payload = gzip.compress(xml.encode()) if compressed else xml.encode()
                log = importer.deserialize(payload, variant=importer.Variants.LINE_BY_LINE,
                    parameters={"show_progress_bar": False, "decompress_serialization": compressed})
                output = dict(attributes=normalize(log.attributes), traces=[dict(attributes=normalize(trace.attributes),
                    events=[normalize(dict(event)) for event in trace]) for trace in log])
            except Exception as error:
                output = dict(error=type(error).__name__, message=str(error))
            row = dict(name=name, xml=xml, compressed=compressed, result=output)
            if args.native:
                with tempfile.TemporaryDirectory(prefix="minera-xes-lines-") as directory:
                    source = Path(directory) / ("input.xes.gz" if compressed else "input.xes")
                    source.write_bytes(gzip.compress(xml.encode()) if compressed else xml.encode())
                    try:
                        actual = subprocess.run(
                            [str(args.native.resolve()), str(source),
                             "--xes-import-variant", "line_by_line", "--log-convert",
                             "to_event_log"], capture_output=True, text=True,
                            encoding="utf-8", timeout=30,
                            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
                    except Exception as error:
                        native, native_error = _native_failure("native_process", error)
                    else:
                        native, native_error = _decode_native_import(actual)
                if native_error:
                    errors.append(dict(name=name, compressed=compressed, **native_error))
                row["native"] = native
                row["matches"] = output == native or ("error" in output and "error" in native)
            rows.append(row)
    result = dict(
        reference=pm4py.__version__, reference_commit=REFERENCE_COMMIT,
        reference_commit_scope=REFERENCE_COMMIT_SCOPE,
        environment=_environment(), reference_module=_module_metadata(importer),
        rows=rows, errors=errors)
    if args.native:
        try:
            native_metadata = _file_metadata(args.native)
        except Exception as error:
            native_metadata = None
            errors.append(dict(kind="native_metadata", message=str(error)))
        result["native"] = str(args.native.resolve())
        result["native_executable"] = native_metadata
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Captured {len(rows)} LINE_BY_LINE observations")
    if args.native:
        print(f"Native matches: {sum(row['matches'] for row in rows)}/{len(rows)}")
    return 1 if args.native and (errors or any(not row["matches"] for row in rows)) else 0


if __name__ == "__main__":
    raise SystemExit(main())
