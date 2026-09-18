"""Compare typed CLI/HTTP formatting against the pinned public corpus.

No reference package is imported. The checked-in generator validates the exact
manifest/provenance before replay; native executables are hashed before/after.
"""

import argparse
import copy
import gzip
import hashlib
import json
import math
import os
from pathlib import Path
import runpy
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parents[1]


def cell(kind, value, key=""):
    result = {"key": key, "value": "", "kind": "string"}
    if kind == "null":
        result["null"] = True
    elif kind == "tuple":
        result["container"] = "tuple"
        result["children"] = [property_cell("", item) for item in value]
    elif kind in ("int", "float", "bool", "date"):
        result["kind"] = "boolean" if kind == "bool" else kind
        result["value"] = str(value)
        result["typed_value"] = value
        if kind == "float" and value in ("Infinity", "-Infinity"):
            result["typed_value"] = "INF" if value == "Infinity" else "-INF"
            result["value"] = "inf" if value == "Infinity" else "-inf"
    elif kind == "string":
        result["value"] = value
    else:
        raise ValueError("unmapped public scalar kind: " + kind)
    return result


def property_cell(key, value):
    if isinstance(value, (dict, list)):
        children = (
            value.items() if isinstance(value, dict) else (("", item) for item in value)
        )
        return {
            "key": key,
            "value": "",
            "kind": "string",
            "container": "container" if isinstance(value, dict) else "list",
            "children": [property_cell(k, v) for k, v in children],
        }
    kind = {
        str: "string",
        int: "int",
        float: "float",
        bool: "bool",
        type(None): "null",
    }[type(value)]
    return cell(kind, value, key)


def label(value):
    if value["kind"] == "tuple":
        return {"kind": "tuple", "items": [label(item) for item in value["value"]]}
    return {"kind": "scalar", "value": cell(value["kind"], value["value"])}


def wire_table(frame):
    columns = frame["column_metadata"]
    index = frame["index_metadata"]
    categories = []
    for column in columns:
        public = frame["categorical_dtypes"].get(column["key"])
        categories.append(
            None
            if public is None
            else {
                "categories_dtype": public["categories_dtype"],
                "ordered": public["ordered"],
                "categories": [
                    cell(item["kind"], item["value"]) for item in public["categories"]
                ],
            }
        )
    return {
        "columns": frame["columns"],
        "dtypes": [column["dtype"] for column in columns],
        "categorical_dtypes": categories,
        "index": {
            "class": index["class"],
            "dtype": index["dtype"],
            "names": index["names"],
            "labels": [label(item) for item in index["labels"]],
        },
        "rows": [
            [
                cell(
                    row["kinds"][column["key"]],
                    row["values"][column["key"]],
                    column["label"],
                )
                for column in columns
            ]
            for row in frame["records"]
        ],
        "attributes": [],
        "metadata": {
            "extensions": [],
            "classifiers": [],
            "trace_globals": [],
            "event_globals": [],
            "globals": [],
            "properties": [
                property_cell(key, value) for key, value in frame["attrs"].items()
            ],
        },
    }


def canonical_cell(value):
    if value is None or value.get("null", False):
        return (None if value is None else value.get("key", ""), "null", None)
    kind = value["kind"]
    container = value.get("container", "scalar")
    if container != "scalar":
        return (
            value.get("key", ""),
            container,
            tuple(canonical_cell(item) for item in value.get("children", [])),
        )
    scalar = value.get("typed_value", value["value"])
    if kind == "float":
        scalar = float(scalar)
        if math.isnan(scalar):
            return (value.get("key", ""), "null", None)
        if float(value["value"]) != scalar:
            raise ValueError("float lexical projection differs")
    elif kind == "int":
        if type(scalar) is not int or int(value["value"]) != scalar:
            raise ValueError("integer lexical projection differs")
    elif kind == "boolean":
        if type(scalar) is not bool or value["value"].lower() not in (
            ("true", "1") if scalar else ("false", "0")
        ):
            raise ValueError("boolean lexical projection differs")
    elif kind == "date" and value["value"] != scalar:
        raise ValueError("date lexical projection differs")
    return (value.get("key", ""), kind, scalar)


def canonical_table(table):
    result = copy.deepcopy(table)
    columns = table["columns"]

    def normalized_cell(value, key=None):
        parsed = canonical_cell(value)
        if key is not None:
            if value is not None and value.get("key") != key:
                raise ValueError("cell key does not match column")
            return (key, *parsed[1:])
        return parsed

    result["rows"] = [
        [normalized_cell(value, key) for key, value in zip(columns, row, strict=True)]
        for row in table["rows"]
    ]
    result["attributes"] = [
        canonical_cell(value) for value in table.get("attributes", [])
    ]
    metadata = result.setdefault("metadata", {})
    for name in ("properties", "trace_globals", "event_globals"):
        metadata[name] = [canonical_cell(value) for value in metadata.get(name, [])]
    for entry in metadata.get("globals", []):
        entry["attributes"] = [canonical_cell(value) for value in entry["attributes"]]
    result.setdefault("categorical_dtypes", [None] * len(columns))
    for descriptor in result["categorical_dtypes"]:
        if descriptor:
            descriptor["categories"] = [
                canonical_cell(value) for value in descriptor["categories"]
            ]

    def normalized_label(value):
        return (
            ("tuple", tuple(normalized_label(item) for item in value["items"]))
            if value["kind"] == "tuple"
            else ("scalar", canonical_cell(value["value"]))
        )

    if result.get("index"):
        result["index"]["labels"] = [
            normalized_label(value) for value in result["index"]["labels"]
        ]
    return result


def compare(row, actual):
    if actual.get("family") != "FORMAT_DATAFRAME":
        raise ValueError("wrong response family")
    failed = "error" in row
    if failed != ("error" in actual) or failed == ("table" in actual):
        raise ValueError("public success/rejection differs")
    if canonical_table(actual["input_after"]) != canonical_table(
        wire_table(row["input_after"])
    ):
        raise ValueError("post-call input differs")
    if not failed and canonical_table(actual["table"]) != canonical_table(
        wire_table(row["result"])
    ):
        raise ValueError("formatted table differs")


def post(url, data, method="POST"):
    request = urllib.request.Request(
        url, data=data, headers={"Content-Type": "application/json"}, method=method
    )
    try:
        with urllib.request.urlopen(request, timeout=15) as response:
            return response.status, response.read()
    except urllib.error.HTTPError as error:
        return error.code, error.read()


def check(executable, server, reference, output):
    generator = runpy.run_path(str(ROOT / "scripts/generate-format-dataframe-tests.py"))
    raw = gzip.decompress(reference.read_bytes())
    generator["generate"](raw)
    report = json.loads(raw)
    excluded = generator["EXCLUDED_CASES"]
    cases = [row for row in report["rows"] if row["name"] not in excluded]
    if len(cases) != 126:
        raise ValueError("expected all 126 representable cases")
    hashes = {
        str(path): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in (executable, server)
    }
    completed = []
    creationflags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    with tempfile.TemporaryDirectory(prefix="minera-format-transport-") as temporary:
        directory = Path(temporary)
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            port = reservation.getsockname()[1]
        with (directory / "server.log").open("wb") as log:
            process = subprocess.Popen(
                [str(server), "--port", str(port)],
                cwd=directory,
                stdout=log,
                stderr=log,
                creationflags=creationflags,
            )
            try:
                endpoint = f"http://127.0.0.1:{port}/api/format-dataframe"
                for attempt in range(100):
                    if process.poll() is not None:
                        raise RuntimeError(
                            "server exited: "
                            + (directory / "server.log").read_text(errors="replace")
                        )
                    try:
                        post(endpoint, b"{}")
                        break
                    except urllib.error.URLError:
                        time.sleep(0.05)
                else:
                    raise RuntimeError("server did not become ready")
                for row in cases:
                    request = json.dumps(
                        {
                            "table": wire_table(row["input"]),
                            "options": row["parameters"],
                        },
                        ensure_ascii=True,
                    ).encode()
                    expected_status = 1 if "error" in row else 0
                    cli = subprocess.run(
                        [str(executable), "-"],
                        input=request,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE,
                        timeout=15,
                        creationflags=creationflags,
                    )
                    if cli.returncode != expected_status:
                        raise ValueError(
                            row["name"]
                            + ": CLI status differs: "
                            + cli.stderr.decode(errors="replace")
                        )
                    try:
                        compare(row, json.loads(cli.stdout))
                        status, body = post(endpoint, request)
                        if status != (400 if expected_status else 200):
                            raise ValueError("HTTP status differs")
                        compare(row, json.loads(body))
                        if json.loads(body) != json.loads(cli.stdout):
                            raise ValueError("CLI and HTTP payloads differ")
                    except Exception as error:
                        raise ValueError(row["name"] + ": " + str(error)) from error
                    completed.append(row["name"])
                # Equivalent integer spellings must not change numeric IDs.
                numeric = next(
                    row
                    for row in cases
                    if row["name"] == "numeric_case_and_activity_ids"
                )
                numeric_table = wire_table(numeric["input"])
                for event in numeric_table["rows"]:
                    for scalar in event:
                        if scalar["kind"] == "int" and scalar["typed_value"] >= 0:
                            scalar["value"] = "00" + scalar["value"]
                request = json.dumps({"table": numeric_table}).encode()
                numeric_cli = subprocess.run(
                    [str(executable), "-"],
                    input=request,
                    capture_output=True,
                    timeout=15,
                    creationflags=creationflags,
                )
                status, numeric_body = post(endpoint, request)
                if numeric_cli.returncode or status != 200:
                    raise ValueError("equivalent integer spelling was rejected")
                compare(numeric, json.loads(numeric_cli.stdout))
                compare(numeric, json.loads(numeric_body))

                # Native metadata not expressible as DataFrame.attrs also
                # survives the transport, including duplicate global scopes.
                metadata_table = wire_table(cases[0]["input"])
                metadata_table["metadata"].update(
                    {
                        "extensions": [
                            {"name": "custom", "prefix": "p", "uri": "urn:test"}
                        ],
                        "classifiers": [{"name": "activity", "keys": ["concept:name"]}],
                        "trace_globals": [property_cell("wide", 18446744073709551615)],
                        "event_globals": [property_cell("enabled", True)],
                        "globals": [
                            {
                                "scope": "trace",
                                "attributes": [property_cell("sequence", i)],
                            }
                            for i in (2, 1)
                        ],
                        "properties": [
                            property_cell(
                                "nested",
                                {
                                    "values": [1, True, None],
                                    "text": "line\nbreak\U0001f600",
                                },
                            )
                        ],
                    }
                )
                metadata_table["attributes"] = [
                    {
                        "key": "payload",
                        "value": "00ff",
                        "kind": "bytes",
                        "scalar_wrapper": True,
                        "scalar_children_list": True,
                        "children": [property_cell("tag", "value")],
                    }
                ]
                metadata_table["rows"][0][0].update(
                    first_class_role="case_id",
                    first_class_state="present",
                    first_class_source_key="external",
                )
                request = json.dumps(
                    {"table": metadata_table}, ensure_ascii=False
                ).encode()
                metadata_cli = subprocess.run(
                    [str(executable), "-"],
                    input=request,
                    capture_output=True,
                    timeout=15,
                    creationflags=creationflags,
                )
                status, metadata_body = post(endpoint, request)
                if metadata_cli.returncode or status != 200:
                    raise ValueError("native metadata transport failed")
                for response in (
                    json.loads(metadata_cli.stdout),
                    json.loads(metadata_body),
                ):
                    if canonical_table(response["input_after"]) != canonical_table(
                        metadata_table
                    ):
                        raise ValueError("native input metadata changed")
                    for key in (
                        "extensions",
                        "classifiers",
                        "trace_globals",
                        "event_globals",
                        "globals",
                    ):
                        if (
                            canonical_table(response["table"])["metadata"][key]
                            != canonical_table(metadata_table)["metadata"][key]
                        ):
                            raise ValueError("formatted native metadata changed")
                    if not response["input_after"]["attributes"][0].get(
                        "scalar_wrapper"
                    ) or not response["input_after"]["attributes"][0].get(
                        "scalar_children_list"
                    ):
                        raise ValueError("scalar wrapper flags lost")
                    for key in (
                        "first_class_role",
                        "first_class_state",
                        "first_class_source_key",
                    ):
                        if (
                            response["input_after"]["rows"][0][0].get(key)
                            != metadata_table["rows"][0][0][key]
                        ):
                            raise ValueError("first-class metadata lost")
                # File input/output, errors, unknown query options and CORS.
                request_file = directory / "request.json"
                request_file.write_text(
                    json.dumps({"table": wire_table(cases[0]["input"])}),
                    encoding="utf-8",
                )
                result_file = directory / "result.json"
                cli = subprocess.run(
                    [str(executable), str(request_file), "--output", str(result_file)],
                    capture_output=True,
                    timeout=15,
                    creationflags=creationflags,
                )
                if cli.returncode or cli.stdout:
                    raise ValueError("CLI file output failed")
                compare(cases[0], json.loads(result_file.read_text(encoding="utf-8")))
                invalid_output = subprocess.run(
                    [
                        str(executable),
                        str(request_file),
                        "--output",
                        str(directory / "missing" / "result.json"),
                    ],
                    capture_output=True,
                    timeout=15,
                    creationflags=creationflags,
                )
                if invalid_output.returncode == 0:
                    raise ValueError("invalid output destination accepted")
                for malformed in (
                    b"{}",
                    b"{",
                    b'{"table":{},"table":{}}',
                    b'{"table":{"columns":[],"rows":[]},"options":{"typo":true}}',
                ):
                    status, body = post(endpoint, malformed)
                    if status != 400 or not json.loads(body).get("error"):
                        raise ValueError("malformed request accepted")
                status, body = post(endpoint + "?unknown=1", request_file.read_bytes())
                if status != 400 or not json.loads(body).get("error"):
                    raise ValueError("unknown query accepted")
                status, _ = post(endpoint, None, "OPTIONS")
                if status not in (200, 204):
                    raise ValueError("preflight failed")
            finally:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
    after = {
        str(path): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in (executable, server)
    }
    if hashes != after:
        raise ValueError("native executable changed during checks")
    result = {
        "reference_sha256": hashlib.sha256(raw).hexdigest(),
        "reference_provenance": report["reference_provenance"],
        "public_cases": len(report["rows"]),
        "case_names": completed,
        "excluded_cases": excluded,
        "cli_comparisons": len(completed),
        "http_comparisons": len(completed),
        "transport_boundary_checks": 12,
        "native_sha256_before_and_after": hashes,
    }
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"{len(completed)} CLI + {len(completed)} HTTP public comparisons passed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--server", type=Path, required=True)
    parser.add_argument(
        "--reference",
        type=Path,
        default=ROOT / "docs/format-dataframe-reference.json.gz",
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    check(
        args.executable.resolve(),
        args.server.resolve(),
        args.reference.resolve(),
        args.output,
    )
