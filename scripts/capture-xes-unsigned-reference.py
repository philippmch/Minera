"""Capture public XES integer boundaries without reading implementation text."""

import warnings

warnings.filterwarnings("ignore")

import argparse
import hashlib
from importlib.metadata import version
import json
from pathlib import Path
import subprocess
import tempfile

import pm4py
from pm4py.objects.log.importer.xes import importer

PIN = "0715c31f3f1d85924107d6aa5329806db69df899"
VARIANTS = (
    "ITERPARSE",
    "LINE_BY_LINE",
    "ITERPARSE_MEM_COMPRESSED",
    "ITERPARSE_20",
    "CHUNK_REGEX",
    "RUSTXES",
)
BOUNDARIES = (2**63 - 1, 2**63, 2**64 - 1, 2**64)


def capture(checkout):
    if pm4py.__version__ != "2.7.22" or version("r4pm") != "0.6.2":
        raise RuntimeError("Expected PM4Py 2.7.22 and r4pm 0.6.2")
    if tuple(variant.name for variant in importer.Variants) != VARIANTS:
        raise RuntimeError("Public XES variant manifest differs")
    modules = []
    for module in [importer] + [variant.value for variant in importer.Variants]:
        relative = module.__name__.replace(".", "/") + ".py"
        expected = subprocess.check_output(
            [
                "git",
                "-C",
                str(checkout),
                "show",
                PIN + ":" + relative,
            ]
        ).replace(b"\r\n", b"\n")
        actual = Path(module.__file__).read_bytes().replace(b"\r\n", b"\n")
        if expected != actual:
            raise RuntimeError("Public module pin mismatch: " + module.__name__)
        modules.append(
            {
                "module": module.__name__,
                "installed_sha256": hashlib.sha256(actual).hexdigest(),
                "pinned_sha256": hashlib.sha256(expected).hexdigest(),
            }
        )
    rows = []
    with tempfile.TemporaryDirectory(prefix="minera-unsigned-xes-") as directory:
        path = Path(directory) / "input.xes"
        for value in BOUNDARIES:
            xml = (
                '<log>\n<trace>\n<string key="concept:name" value="C"/>\n'
                '<event>\n<string key="concept:name" value="A"/>\n'
                '<int key="probe" value="' + str(value) + '"/>\n'
                "</event>\n</trace>\n</log>\n"
            )
            path.write_text(xml, encoding="utf-8")
            for variant in importer.Variants:
                row = {"variant": variant.name, "input": value, "xml": xml}
                try:
                    output = importer.apply(
                        str(path),
                        variant=variant,
                        parameters={
                            "show_progress_bar": False,
                            "return_legacy_log_object": True,
                        },
                    )
                    row["value"] = output[0][0].get("probe")
                    row["type"] = type(output[0][0].get("probe")).__name__
                except BaseException as error:
                    row["error"] = {"type": type(error).__name__, "message": str(error)}
                rows.append(row)
    return {
        "scope": "Public integer boundary evidence; native integers beyond uint64 remain unsupported.",
        "reference_version": pm4py.__version__,
        "backend_version": version("r4pm"),
        "reference_commit": PIN,
        "reference_method": "opaque module bytes with CRLF normalized to LF; no source decoding",
        "reference_modules": modules,
        "rows": rows,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--reference-checkout", type=Path, required=True)
    args = parser.parse_args()
    report = capture(args.reference_checkout)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(
        json.dumps(
            {
                "cases": len(report["rows"]),
                "errors": sum("error" in row for row in report["rows"]),
            }
        )
    )
