"""Observe whether public XES parsers retain empty scalar wrapper identity."""
import argparse
import hashlib
import json
import os
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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    pm4py, importer = _load_public_reference_api()
    _require_version(pm4py)
    rows = []
    for variant in importer.Variants:
        if variant.name == "RUSTXES":
            continue
        for tag, value in (("boolean", "True"), ("date", "2024-01-01T00:00:00Z"), ("string", "text")):
            for shape, inner in (("self_closing", None), ("empty", ""), ("empty_values", "<values/>"),
                                 ("child", '<string key="child" value="x"/>')):
                attribute = f'<{tag} key="probe" value="{value}"' + ('/>' if inner is None else f'>{inner}</{tag}>')
                xml = f'<log>\n<trace>\n<event>\n{attribute}\n</event>\n</trace>\n</log>'
                try:
                    log = importer.deserialize(xml.encode(), variant=variant, parameters={"show_progress_bar": False})
                    result = log[0][0]["probe"]
                    observed = dict(type=type(result).__name__, value=repr(result), wrapped=isinstance(result, dict))
                except Exception as error:
                    observed = dict(error=type(error).__name__)
                rows.append(dict(variant=variant.name, tag=tag, shape=shape, result=observed))
    result = dict(
        reference=pm4py.__version__, reference_commit=REFERENCE_COMMIT,
        reference_commit_scope=REFERENCE_COMMIT_SCOPE,
        environment=_environment(), reference_module=_module_metadata(importer),
        rows=rows)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Captured {len(rows)} public parser observations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
