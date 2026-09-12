"""Capture date lexical behavior through public pinned XES exporters only."""
import argparse
import datetime
import gzip
import hashlib
import json
import os
from pathlib import Path
from xml.etree import ElementTree
import warnings


REFERENCE_VERSION = "2.7.22"
PANDAS_VERSION = "3.0.5"
REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"
REFERENCE_COMMIT_SCOPE = "configured_reference_pin"


def _load_public_reference_api():
    warnings.filterwarnings("ignore")
    import pandas as pd
    import pm4py
    from pm4py.objects.log.exporter.xes import exporter
    from pm4py.objects.log.obj import Event, EventLog, Trace
    return pm4py, pd, exporter, Event, EventLog, Trace


def _require_versions(pm4py, pandas):
    versions = (getattr(pm4py, "__version__", None),
                getattr(pandas, "__version__", None))
    if versions != (REFERENCE_VERSION, PANDAS_VERSION):
        raise RuntimeError(
            f"This oracle requires pm4py=={REFERENCE_VERSION} and "
            f"pandas=={PANDAS_VERSION}; got pm4py=={versions[0]!r}, "
            f"pandas=={versions[1]!r}")


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


def capture():
    pm4py, pd, exporter, Event, EventLog, Trace = _load_public_reference_api()
    _require_versions(pm4py, pd)
    rows = []
    for source in ("2024-01-01T00:00:00", "2024-01-01T00:00:00Z",
                   "2024-01-01T00:00:00.000000Z", "2024-01-01T00:00:00.1Z",
                   "2024-01-01T00:00:00.123400Z", "2024-01-01T00:00:00.123456000Z", "2024-01-01T00:00:00.000000001Z",
                   "2024-01-01T00:00:00.123456789+05:30",
                   "1969-12-31T23:59:59.999999999-04:00",
                   "2500-01-01T00:00:00.000001Z", "2500-01-01T00:00:00.000000001Z", "0001-01-01T00:00:00Z"):
        for kind, factory in (("datetime", datetime.datetime.fromisoformat),
                              ("Timestamp", pd.Timestamp)):
            try:
                value = factory(source)
            except (ValueError, OverflowError) as error:
                rows.append(dict(source=source, kind=kind, construction_error=type(error).__name__))
                continue
            for shape in ("scalar", "nested_wrapper", "empty_wrapper"):
                payload = value if shape == "scalar" else {"value": value, "children":
                    {"child": "metadata"} if shape == "nested_wrapper" else {}}
                log = EventLog([Trace([Event({"probe": payload})])])
                for variant in exporter.Variants:
                    for compressed in (False, True):
                        output = exporter.serialize(log, variant=variant, parameters={
                            "compress": compressed, "show_progress_bar": False})
                        if compressed:
                            output = gzip.decompress(output)
                        element = next(x for x in ElementTree.fromstring(output).iter()
                                       if x.get("key") == "probe")
                        rows.append(dict(source=source, kind=kind, shape=shape,
                                         variant=variant.name, compressed=compressed,
                                         tag=element.tag, value=element.get("value")))
    return dict(
        reference_version=pm4py.__version__, pandas_version=pd.__version__,
        reference_commit=REFERENCE_COMMIT,
        reference_commit_scope=REFERENCE_COMMIT_SCOPE,
        environment=_environment(), reference_module=_module_metadata(exporter),
        rows=rows)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.write_text(json.dumps(capture(), indent=2) + "\n", encoding="utf-8")
