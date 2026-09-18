"""Capture metadata from the public RUSTXES file importer without source reads."""
import argparse
import datetime
import gzip
import hashlib
import json
import os
import subprocess
import tempfile
from importlib.metadata import version
from pathlib import Path
import warnings


REFERENCE_VERSION = "2.7.22"
BACKEND_VERSION = "0.6.2"
REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"
REFERENCE_COMMIT_SCOPE = "configured_reference_pin"
pm4py = importer = None


def _load_public_reference_api():
    global pm4py, importer
    warnings.filterwarnings("ignore")
    import pm4py as pm4py_module
    from pm4py.objects.log.importer.xes import importer as importer_module
    pm4py, importer = pm4py_module, importer_module
    return pm4py, importer


def _require_version(pm4py_module):
    actual = getattr(pm4py_module, "__version__", None)
    if actual != REFERENCE_VERSION:
        raise RuntimeError(
            f"This oracle requires pm4py=={REFERENCE_VERSION}; got {actual!r}")


def _require_backend(actual):
    if actual != BACKEND_VERSION:
        raise RuntimeError(f"This oracle requires r4pm=={BACKEND_VERSION}; got {actual!r}")


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


def _native_metadata(path):
    try:
        return _file_metadata(path)
    except (OSError, RuntimeError) as error:
        return {"path": str(Path(path).resolve()), "sha256": None, "error": str(error)}


def _native_integrity(before, after):
    metadata = {
        "path": before.get("path", after.get("path")),
        "sha256_before": before.get("sha256"),
        "sha256_after": after.get("sha256"),
    }
    errors = []
    if after.get("sha256") is not None:
        metadata["sha256"] = after["sha256"]
    if before.get("error"):
        metadata["error"] = before["error"]
        metadata["error_before"] = before["error"]
        errors.append({"kind": "native_hash_error", "phase": "before",
                       "error": before["error"]})
    if after.get("error"):
        metadata["error"] = after["error"]
        metadata["error_after"] = after["error"]
        errors.append({"kind": "native_hash_error", "phase": "after",
                       "error": after["error"]})
    if (before.get("sha256") is not None and after.get("sha256") is not None
            and before["sha256"] != after["sha256"]):
        metadata["changed"] = True
        errors.append({"kind": "native_executable_changed",
                       "sha256_before": before["sha256"],
                       "sha256_after": after["sha256"]})
    return metadata, errors


def normalize(value):
    if isinstance(value, dict):
        return {str(k): normalize(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [normalize(v) for v in value]
    if isinstance(value, datetime.datetime):
        return {"date": value.isoformat()}
    return value


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--native", type=Path, help="Compare successful reference metadata projections with this CLI")
    args = parser.parse_args()
    pm4py, importer = _load_public_reference_api()
    _require_version(pm4py)
    backend_version = version("r4pm")
    _require_backend(backend_version)
    native_before = _native_metadata(args.native) if args.native else None
    shapes = {
        "string": '<string key="probe" value="text"/>',
        "int": '<int key="probe" value="42"/>',
        "float": '<float key="probe" value="1.5"/>',
        "boolean": '<boolean key="probe" value="true"/>',
        "date": '<date key="probe" value="2024-01-01T12:00:00+05:30"/>',
        "duplicate": '<string key="probe" value="first"/><string key="probe" value="second"/>',
        "scalar_children": '<string key="probe" value="parent"><string key="child" value="value"/></string>',
        "list": '<list key="probe"><values><string key="child" value="value"/></values></list>',
        "container": '<container key="probe"><string key="child" value="value"/></container>',
        "standards": '<date key="time:timestamp" value="2024-01-01T12:00:00Z"/><string key="org:resource" value="r"/><string key="lifecycle:transition" value="complete"/><float key="cost:total" value="1.5"/>',
        "unknown_namespace": '<string key="custom:probe" value="value"/>',
        "all_prefixes": ''.join(f'<string key="{prefix}:probe" value="value"/>' for prefix in ('concept', 'time', 'org', 'lifecycle', 'cost', 'identity', 'artifactlifecycle', 'micro', 'semantic', 'swcomm', 'swevent', 'swtelemetry')),
        "nested_prefix": '<string key="probe" value="parent"><string key="org:resource" value="r"/></string>',
    }
    rows = []
    with tempfile.TemporaryDirectory(prefix="minera-rust-metadata-") as directory:
        for scope in ("log", "trace", "event", "global", "event_no_name"):
            for shape, attribute in shapes.items():
                xml = ('<log>' + (attribute if scope == "log" else '') +
                       ('<global scope="event">' + attribute + '</global>' if scope == "global" else '') +
                       '<trace><string key="concept:name" value="c"/>' + (attribute if scope == "trace" else '') +
                       '<event>' + ('' if scope == "event_no_name" else '<string key="concept:name" value="A"/>') +
                       (attribute if scope in ("event", "event_no_name") else '') +
                       '</event></trace></log>')
                for compressed in (False, True):
                    path = Path(directory) / ('input.xes.gz' if compressed else 'input.xes')
                    path.write_bytes(gzip.compress(xml.encode()) if compressed else xml.encode())
                    try:
                        log = importer.apply(str(path), variant=importer.Variants.RUSTXES, parameters={"show_progress_bar": False})
                        result = dict(attributes=normalize(log.attributes), extensions=normalize(log.extensions),
                            classifiers=normalize(log.classifiers), globals=normalize(log.omni_present),
                            traces=[dict(attributes=normalize(trace.attributes), events=[normalize(dict(event)) for event in trace]) for trace in log])
                    except BaseException as error:
                        result = {"error": type(error).__name__, "message": str(error)}
                    rows.append(dict(scope=scope, shape=shape, compressed=compressed, xml=xml, result=result))
        for shape, xml in {
            "empty_log": '<log/>',
            "empty_trace": '<log><trace><string key="concept:name" value="c"/></trace></log>',
            "later_schema": '<log><trace><event><string key="concept:name" value="A"/></event><event><string key="org:probe" value="r"/></event></trace><trace><string key="cost:probe" value="c"/></trace></log>',
            "later_schema_with_case": '<log><trace><string key="concept:name" value="c"/><event><string key="concept:name" value="A"/></event><event><string key="org:probe" value="r"/></event></trace><trace><string key="concept:name" value="d"/><string key="cost:probe" value="c"/></trace></log>',
            "declarations": '<log><extension name="Custom" prefix="custom" uri="urn:custom"/><classifier name="Class" keys="concept:name"/><trace><string key="concept:name" value="c"/><event><string key="concept:name" value="A"/><string key="custom:probe" value="value"/></event></trace></log>',
        }.items():
            for compressed in (False, True):
                path = Path(directory) / ('input.xes.gz' if compressed else 'input.xes')
                path.write_bytes(gzip.compress(xml.encode()) if compressed else xml.encode())
                try:
                    log = importer.apply(str(path), variant=importer.Variants.RUSTXES, parameters={"show_progress_bar": False})
                    result = dict(attributes=normalize(log.attributes), extensions=normalize(log.extensions),
                        classifiers=normalize(log.classifiers), globals=normalize(log.omni_present),
                        traces=[dict(attributes=normalize(trace.attributes), events=[normalize(dict(event)) for event in trace]) for trace in log])
                except Exception as error:
                    result = {"error": type(error).__name__, "message": str(error)}
                rows.append(dict(scope="special", shape=shape, compressed=compressed, xml=xml, result=result))
        if args.native:
            for row in rows:
                path = Path(directory) / ('input.xes.gz' if row["compressed"] else 'input.xes')
                path.write_bytes(gzip.compress(row["xml"].encode()) if row["compressed"] else row["xml"].encode())
                try:
                    completed = subprocess.run([str(args.native.resolve()), str(path), "--xes-import-variant", "rustxes", "--log-convert", "to_event_log"], capture_output=True, text=True, encoding='utf-8')
                except (OSError, subprocess.TimeoutExpired) as error:
                    row["metadata_comparison"] = {"native_process_error": type(error).__name__, "message": str(error)}
                    continue
                if "error" in row["result"]:
                    row["metadata_comparison"] = "error_match" if row["result"]["error"] == "KeyError" and "case:concept:name" in row["result"]["message"] and completed.returncode and "case:concept:name" in completed.stderr else {"native_status": completed.returncode, "stdout": completed.stdout, "stderr": completed.stderr}
                    continue
                if completed.returncode:
                    row["metadata_comparison"] = {"native_error": completed.stderr,
                                                   "stdout": completed.stdout,
                                                   "returncode": completed.returncode}
                    continue
                try:
                    log = json.loads(completed.stdout)["log"]
                    metadata = log["metadata"]
                    actual = dict(attributes={a["key"]: a["value"] for a in log["attributes"]},
                        extensions={e["name"]: dict(prefix=e["prefix"], uri=e["uri"]) for e in metadata["extensions"]},
                        classifiers={c["name"]: c["keys"] for c in metadata["classifiers"]},
                        globals={g["scope"]: g["attributes"] for g in metadata["globals"]})
                    expected = {key: row["result"][key] for key in actual}
                    row["metadata_comparison"] = "match" if actual == expected else {"expected": expected, "actual": actual}
                except (KeyError, IndexError, TypeError, ValueError, json.JSONDecodeError) as error:
                    row["metadata_comparison"] = {"native_malformed_output": type(error).__name__, "message": str(error),
                                                    "stdout": completed.stdout, "stderr": completed.stderr}
    native_after = _native_metadata(args.native) if args.native else None
    native_metadata = None
    integrity_errors = []
    if args.native:
        native_metadata, integrity_errors = _native_integrity(native_before, native_after)
    comparison_errors = ([{'scope': row.get('scope'), 'shape': row.get('shape'),
                           'comparison': row.get('metadata_comparison')}
                          for row in rows
                          if row.get('metadata_comparison') not in ('match', 'error_match')]
                         if args.native else [])
    comparison_errors.extend(integrity_errors)
    artifact = dict(reference=pm4py.__version__, reference_commit=REFERENCE_COMMIT,
                    reference_commit_scope=REFERENCE_COMMIT_SCOPE,
                    environment=_environment(), reference_module=_module_metadata(importer),
                    r4pm=backend_version,
                    native_executable=native_metadata,
                    rows=rows,
                    errors=comparison_errors)
    args.output.write_text(json.dumps(artifact, indent=2) + "\n", encoding="utf-8")
    print(f"Captured {len(rows)} RUSTXES metadata observations")
    if args.native:
        matches = sum(row["metadata_comparison"] == "match" for row in rows)
        errors = sum(row["metadata_comparison"] == "error_match" for row in rows)
        print(f"Metadata comparisons: {matches} matched; {errors} missing-key failures matched")
        if artifact['errors']:
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
