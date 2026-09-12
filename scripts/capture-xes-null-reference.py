"""Capture public XES None-value and nested omission behavior."""
import argparse
import gzip
import hashlib
import json
import os
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
    from pm4py.objects.log.obj import Event, EventLog, Trace
    return pm4py, exporter, Event, EventLog, Trace


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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    pm4py, exporter, Event, EventLog, Trace = _load_public_reference_api()
    _require_version(pm4py)
    cases = {
        "none": None,
        "none_empty_mapping": {"value": None, "children": {}},
        "none_empty_list": {"value": None, "children": []},
        "none_mapping": {"value": None, "children": {"child": None, "kept": "value"}},
        "none_list": {"value": None, "children": [("child", None), ("kept", "value")]},
        "scalar_mapping": {"value": True, "children": {"child": None, "kept": "value"}},
        "scalar_list": {"value": True, "children": [("child", None), ("kept", "value")]},
        "nested_none": {"value": "parent", "children": {"child": {"value": None, "children": {"kept": "value"}}}},
    }
    rows = []
    for scope in ("log", "global", "trace", "event"):
        for name, value in cases.items():
            for variant in exporter.Variants:
                for compressed in (False, True):
                    event = Event({"concept:name": "A"})
                    trace = Trace([event], attributes={"concept:name": "c"})
                    log = EventLog([trace])
                    if scope == "global":
                        log.omni_present["event"] = {"probe": value}
                    else:
                        {"log": log.attributes, "trace": trace.attributes, "event": event}[scope]["probe"] = value
                    try:
                        xml = exporter.serialize(log, variant=variant, parameters={"show_progress_bar": False, "compress": compressed})
                        if compressed:
                            xml = gzip.decompress(xml)
                        node = next((node for node in ET.fromstring(xml).iter() if node.get("key") == "probe"), None)
                        result = None if node is None else ET.tostring(node, encoding="unicode")
                    except Exception as error:
                        result = {"error": type(error).__name__}
                    rows.append(dict(scope=scope, shape=name, variant=variant.name, compressed=compressed, result=result))
    result = dict(
        reference=pm4py.__version__, reference_commit=REFERENCE_COMMIT,
        reference_commit_scope=REFERENCE_COMMIT_SCOPE,
        environment=_environment(), reference_module=_module_metadata(exporter),
        rows=rows)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Captured {len(rows)} null export observations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
