"""Capture scalar wrapper child containers through public pinned XES APIs."""
import argparse
import datetime
import hashlib
import json
import os
import subprocess
import tempfile
import warnings
from pathlib import Path
from xml.etree import ElementTree as ET


REFERENCE_VERSION = "2.7.22"
REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"
REFERENCE_COMMIT_SCOPE = "configured_reference_pin"


def _load_public_reference_api():
    warnings.filterwarnings("ignore")
    import pm4py
    from pm4py.objects.log.exporter.xes import exporter
    from pm4py.objects.log.importer.xes import importer
    from pm4py.objects.log.obj import Event, EventLog, Trace
    return pm4py, exporter, importer, Event, EventLog, Trace


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


def _native_attribute_value(attribute):
    value = attribute["typed_value"] if "typed_value" in attribute else attribute["value"]
    children = attribute.get("children", [])
    if attribute.get("scalar_wrapper") or attribute.get("scalar_children_list") or children:
        nested = [[child["key"], _native_attribute_value(child)] for child in children]
        value = dict(value=value, children=nested if attribute.get("scalar_children_list")
                     else dict(nested))
    return value


def _decode_native_import(process):
    stderr = process.stderr or ""
    if process.returncode not in (0, 1) or "Usage:" in stderr:
        return _native_failure(
            "native_process", f"Native CLI failed before import: {stderr[:400]}", process)
    if process.returncode == 1:
        return {"error": True, "message": stderr}, None
    try:
        event = json.loads(process.stdout)["log"]["traces"][0]["events"][0]
        attributes = event["attributes"]
        if not isinstance(attributes, list):
            raise ValueError("native event attributes must be an array")
        return ({item["key"]: _native_attribute_value(item) for item in attributes}, None)
    except Exception as error:
        return _native_failure("native_malformed_output", error, process)


def normalize(value):
    if isinstance(value, dict):
        return {str(key): normalize(item) for key, item in value.items()}
    if isinstance(value, (tuple, list)):
        return [normalize(item) for item in value]
    if isinstance(value, datetime.datetime):
        return {"date": value.isoformat()}
    return value


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--native", type=Path, help="Compare imports through the native log-conversion CLI")
    args = parser.parse_args()
    pm4py, exporter, importer, Event, EventLog, Trace = _load_public_reference_api()
    _require_version(pm4py)
    child = '<string key="same" value="first"/><int key="same" value="2"/>'
    shapes = {"empty": "", "empty_values": "<values/>", "direct": child,
              "values": "<values>" + child + "</values>",
              "before_values": child + "<values/>", "after_values": "<values/>" + child,
              "two_values": "<values>" + child + "</values><values>" + child + "</values>",
              "distinct_values": '<values><string key="first" value="1"/></values><values><string key="second" value="2"/></values>',
              "mapping_then_values": '<string key="direct" value="1"/><values><string key="nested" value="2"/></values>'}
    imports, exports, errors = [], [], []
    for variant in importer.Variants:
        if variant.name == "RUSTXES":
            continue
        for shape, inner in shapes.items():
            xml = ('<log>\n<trace>\n<event>\n<boolean key="probe" value="true">' + inner +
                   '</boolean>\n</event>\n</trace>\n</log>').encode()
            try:
                result = importer.deserialize(xml, variant=variant, parameters={"show_progress_bar": False})
                output = normalize(dict(result[0][0]))
            except Exception as error:
                output = {"error": type(error).__name__}
            row = dict(variant=variant.name, shape=shape, result=output)
            if args.native:
                with tempfile.TemporaryDirectory(prefix="minera-xes-children-") as directory:
                    source = Path(directory) / "input.xes"
                    source.write_bytes(xml)
                    try:
                        native = subprocess.run(
                            [str(args.native.resolve()), str(source), "--log-convert",
                             "to_event_log", "--xes-import-variant", variant.name.lower()],
                            capture_output=True, text=True, encoding="utf-8", timeout=30,
                            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
                    except Exception as error:
                        actual, native_error = _native_failure("native_process", error)
                    else:
                        actual, native_error = _decode_native_import(native)
                if native_error:
                    errors.append(dict(variant=variant.name, shape=shape, **native_error))
                row["native"] = actual
                row["matches"] = actual == output or ("error" in actual and "error" in output)
            imports.append(row)
    for variant in exporter.Variants:
        for children in ({}, [], {"same": 2}, [("same", "first"), ("same", 2)]):
            for value in (True, "parent", datetime.datetime(2024, 1, 1)):
                log = EventLog([Trace([Event({"probe": {"value": value, "children": children}})])])
                try:
                    xml = exporter.serialize(log, variant=variant, parameters={"show_progress_bar": False})
                    parent = next(item for item in ET.fromstring(xml).iter() if item.get("key") == "probe")
                    output = ET.tostring(parent, encoding="unicode")
                except Exception as error:
                    output = {"error": type(error).__name__}
                exports.append(dict(variant=variant.name, children=normalize(children), value=normalize(value), result=output))
    result = dict(
        reference=pm4py.__version__, reference_commit=REFERENCE_COMMIT,
        reference_commit_scope=REFERENCE_COMMIT_SCOPE,
        environment=_environment(), reference_module=_module_metadata(importer),
        imports=imports, exports=exports, errors=errors)
    if args.native:
        try:
            native_metadata = _file_metadata(args.native)
        except Exception as error:
            native_metadata = None
            errors.append(dict(kind="native_metadata", message=str(error)))
        result["native"] = str(args.native.resolve())
        result["native_executable"] = native_metadata
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Captured {len(imports)} imports and {len(exports)} exports")
    if args.native:
        print(f"Native imports matching reference: {sum(row['matches'] for row in imports)}/{len(imports)}")
    return 1 if args.native and (errors or any(not row["matches"] for row in imports)) else 0


if __name__ == "__main__":
    raise SystemExit(main())
