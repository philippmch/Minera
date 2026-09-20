"""Check complete public formatter results plus explicit date resolution."""

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import runpy
import subprocess

ROOT = Path(__file__).resolve().parents[1]
TRANSPORT = runpy.run_path(str(ROOT / "scripts/check-format-dataframe-transports.py"))
DIGEST = "a32771143ac9c0b8ccb8eda5035e4fc4b9fbc4c576d812b5513e920a4cb5a898"
NAMES = tuple(
    zone + "_" + spec
    for zone in ("naive", "UTC", "Europe_Berlin")
    for spec in (
        "object_us",
        "object_ns",
        "inferred_us",
        "inferred_ns",
        "mixed",
        "start_ns",
        "explicit_ns",
    )
)


def load(raw):
    if hashlib.sha256(raw).hexdigest() != DIGEST:
        raise ValueError("Public date resolution artifact hash differs")
    report = json.loads(raw)
    if tuple(row["name"] for row in report["rows"]) != NAMES:
        raise ValueError("Incomplete date resolution manifest")
    return report


def wire(frame):
    table = TRANSPORT["wire_table"](frame)
    for row, source in zip(table["rows"], frame["records"], strict=True):
        for column, cell in zip(table["columns"], row, strict=True):
            tag = source["cell_tags"][column]
            if tag["kind"] == "date" and not tag["missing"]:
                cell["date_resolution"] = tag["native_resolution"]
                if "native_timezone" in tag:
                    cell["date_timezone"] = tag["native_timezone"]
    return table


def compare(row, actual):
    TRANSPORT["compare"](row, actual)
    for key, expected in (
        ("input_after", row["input_after"]),
        ("table", row["result"]),
    ):
        table = actual[key]
        for cells, source in zip(table["rows"], expected["records"], strict=True):
            for column, cell in zip(table["columns"], cells, strict=True):
                tag = source["cell_tags"][column]
                if tag["kind"] == "date" and not tag["missing"]:
                    if cell.get("date_resolution", "us") != tag["native_resolution"]:
                        raise ValueError(
                            "Date resolution differs: " + key + "/" + column
                        )
                    # Input metadata must survive. Formatter output timezone is
                    # represented by its UTC dtype and resolved lexical offset.
                    if key == "input_after" and cell.get("date_timezone") != tag.get(
                        "native_timezone"
                    ):
                        raise ValueError("Input timezone metadata differs")


def check(executable, reference):
    report = load(gzip.decompress(reference.read_bytes()))
    before = hashlib.sha256(executable.read_bytes()).hexdigest()
    for row in report["rows"]:
        request = {"table": wire(row["input"]), "options": row["options"]}
        result = subprocess.run(
            [str(executable), "-"],
            input=json.dumps(request),
            text=True,
            capture_output=True,
            check=True,
        )
        try:
            compare(row, json.loads(result.stdout))
        except ValueError as error:
            raise ValueError(row["name"] + ": " + str(error)) from None
    if before != hashlib.sha256(executable.read_bytes()).hexdigest():
        raise ValueError("Native executable changed during validation")
    return {
        "reference_sha256": DIGEST,
        "reference_provenance": report["reference_provenance"],
        "case_names": list(NAMES),
        "cli_comparisons": len(NAMES),
        "native_sha256_before_and_after": before,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument(
        "--reference",
        type=Path,
        default=ROOT / "docs/formatter-date-resolution-reference.json.gz",
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = check(args.executable.resolve(), args.reference)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(str(result["cli_comparisons"]) + " date resolution CLI comparisons passed")
