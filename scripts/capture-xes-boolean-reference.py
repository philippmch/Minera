"""Capture public 2.7.22 XES boolean behavior without inspecting its source.

Run with the pinned reference and its dependencies on PYTHONPATH. The output
records XML attribute values, so formatting differences cannot hide casing.
"""

import argparse
import gzip
import hashlib
import json
from importlib.metadata import version
import os
from pathlib import Path
from tempfile import TemporaryDirectory
from xml.etree import ElementTree
import warnings


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


def capture(include_rust=False):
    pm4py, exporter, importer, Event, EventLog, Trace = _load_public_reference_api()
    _require_version(pm4py)
    rows = []
    for scope in ("log", "global", "trace", "event"):
        for value in (False, True):
            for shape in ("scalar", "empty_wrapper", "nested_wrapper"):
                payload = value if shape == "scalar" else {
                    "value": value,
                    "children": {} if shape == "empty_wrapper" else {"child": value},
                }
                event = Event({"concept:name": "A"})
                trace = Trace([event], attributes={"concept:name": "case"})
                log = EventLog([trace])
                target = {"log": log.attributes, "trace": trace.attributes,
                          "event": event}.get(scope)
                if scope == "global":
                    log.omni_present["event"] = {"probe": payload}
                else:
                    target["probe"] = payload
                for variant in exporter.Variants:
                    for compressed in (False, True):
                        output = exporter.serialize(log, variant=variant, parameters={
                            "compress": compressed, "show_progress_bar": False,
                        })
                        if compressed:
                            output = gzip.decompress(output)
                        root = ElementTree.fromstring(output)
                        parent = next(element for element in root.iter()
                                      if element.get("key") == "probe")
                        rows.append({
                            "scope": scope, "input": value, "shape": shape,
                            "variant": variant.name, "compressed": compressed,
                            "parent": parent.attrib,
                            "children": [child.attrib for child in parent],
                        })
    imports = []
    for variant in importer.Variants:
        # RUSTXES has no public string-deserialization backend.
        if variant.name == "RUSTXES":
            continue
        for lexical in ("true", "false", "True", "False", "TRUE", "FALSE",
                        "tRuE", "1", "0", "yes", "", " true "):
            xml = ('<log>\n<trace>\n<event>\n<boolean key="flag" value="' +
                   lexical + '"/>\n</event>\n</trace>\n</log>').encode()
            for compressed in (False, True):
                content = gzip.compress(xml) if compressed else xml
                log = importer.deserialize(content, variant=variant, parameters={
                    "show_progress_bar": False,
                    "decompress_serialization": compressed,
                })
                imports.append({"variant": variant.name, "lexical": lexical,
                                "compressed": compressed, "value": log[0][0]["flag"]})
    result = {
        "reference_version": pm4py.__version__,
        "reference_commit": REFERENCE_COMMIT,
        "reference_commit_scope": REFERENCE_COMMIT_SCOPE,
        "environment": _environment(),
        "reference_module": _module_metadata(exporter),
        "rows": rows, "imports": imports,
    }
    if include_rust:
        result["rust_backend"] = {"r4pm": version("r4pm"), "polars": version("polars")}
        result["rust_imports"] = []
        with TemporaryDirectory(prefix="minera-boolean-oracle-") as directory:
            for scope in ("event", "trace"):
                for lexical in ("true", "false", "True", "False", "TRUE", "FALSE",
                                "tRuE", "1", "0", "yes", "", " true "):
                    attribute = '<boolean key="flag" value="' + lexical + '"/>'
                    xml = ('<log><trace><string key="concept:name" value="c"/>' +
                           (attribute if scope == "trace" else "") +
                           '<event><string key="concept:name" value="A"/>' +
                           (attribute if scope == "event" else "") +
                           '</event></trace></log>').encode()
                    for compressed in (False, True):
                        path = Path(directory) / ("input.xes.gz" if compressed else "input.xes")
                        path.write_bytes(gzip.compress(xml) if compressed else xml)
                        log = importer.apply(str(path), variant=importer.Variants.RUSTXES,
                                             parameters={"show_progress_bar": False})
                        attributes = log[0][0] if scope == "event" else log[0].attributes
                        result["rust_imports"].append({"scope": scope, "lexical": lexical,
                            "compressed": compressed, "value": attributes["flag"]})
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--include-rust", action="store_true",
                        help="Also probe the optional r4pm file importer")
    arguments = parser.parse_args()
    result = capture(arguments.include_rust)
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Captured {len(result['rows'])} export, {len(result['imports'])} import, "
          f"and {len(result.get('rust_imports', []))} Rust file-import cases")
    raise SystemExit(0)
