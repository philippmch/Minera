#!/usr/bin/env python3
"""Replay captured workbench requests against a running Minera server and
refresh the frontend fixtures that changed.

The frontend fixture tests (model-conversion-workbench-fixtures.test.ts,
active-source-workbench-fixtures.test.ts, ...) check captured own-server
responses for internal consistency only, so a backend change leaves them
stale while they keep passing. This script replays every entry of a request
manifest in order (the stored request bytes are sent unchanged), compares
each response with the captured one and, with --write, rewrites the entries
whose response changed: the response file, its byte count and sha256, the
Content-Length header, the status, and every export artifact taken from the
response's "exports" object or, for the input workbenches, its top-level
"<name>"/"<name>_export" field (written as WTF-8, so lone surrogates
survive; a hex "artifact_originals" carrier is written as its bytes). An
export that is now null loses its record, and one the manifest records for
other entries that is now produced gets a record named like theirs. The
expected status and the manifest fields that mirror the response
(operation, executed, error, native_calls, ...) follow the new response.
Manifests keep their line ends.

Start a fresh server first, so session-dependent values (source ids) match
the original capture order, for example:

  build/minera_server --port 8568 --frontend /tmp/empty &
  scripts/capture-workbench-fixtures.py \\
      --manifest src/frontend/tests/fixtures/model-conversion-workbench-request-manifest.json \\
      --origin http://127.0.0.1:8568 --only '^powl_to_net-' --write

Without --write the script only reports which entries differ. --only limits
the rewrite (not the replay) to entry ids that match the regular expression.
"""

import argparse
import hashlib
import json
import os
import re
import sys
import urllib.error
import urllib.request


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


STRUCTURAL = {"id", "method", "path", "fields", "field_bytes", "headers", "request_body", "response",
              "response_headers", "exports", "expected_status", "family", "input", "uploads"}


def parse(data: bytes) -> dict:
    try:
        value = json.loads(data.decode("utf-8", "surrogatepass"))
    except ValueError:
        return {}
    return value if isinstance(value, dict) else {}


def export_bytes(value: dict, name: str, status: int):
    """The bytes of export `name` in a response, None when it is null, or
    KeyError when the response has no such field at all."""
    # Most workbenches answer with an "exports" object; the input workbenches
    # (process-tree alignment, inductive/workflow input, anti-alignment, graph
    # alignment, active source) put each export at the top level, as "<name>"
    # or "<name>_export".
    if isinstance(value.get("exports"), dict):
        text, key = value["exports"].get(name), name
    else:
        key = next((k for k in (name, name + "_export") if k in value), None)
        if key is None and status < 400:
            raise KeyError(name)
        text = value.get(key) if key else None
    # A byte artifact that is not text (embedding workbench) has a null export
    # and its bytes in "artifact_originals" as hex.
    originals = value.get("artifact_originals")
    carrier = originals.get(name) if isinstance(originals, dict) else None
    if isinstance(carrier, dict) and carrier.get("encoding") == "hex" and isinstance(carrier.get("data"), str):
        return bytes.fromhex(carrier["data"])
    if isinstance(text, str):
        return text.encode("utf-8", "surrogatepass")
    return None


def record_like(template_id: str, template: dict, entry_id: str) -> dict:
    """A new export record named like the manifest's other records of that
    export: the template entry's id is replaced by this entry's id."""
    def rename(text: str) -> str:
        directory, _, base = text.rpartition("/")
        if base.startswith(template_id):
            base = entry_id + base[len(template_id):]
        return f"{directory}/{base}" if directory else base
    record = dict(template)
    for key in ("filename", "path"):
        if key in record:
            record[key] = rename(record[key])
    return record


def refresh_exports(entry, exports, value, status, previous, old_status, known, path_of):
    for name, record in list(exports.items()):
        try:
            artifact = export_bytes(value, name, status)
        except KeyError:
            print(f"  {entry['id']}: export {name} not found in the response; record kept")
            continue
        if artifact is None:
            del exports[name]
            continue
        with open(path_of(record), "wb") as handle:
            handle.write(artifact)
        record.update(bytes=len(artifact), sha256=sha256(artifact))
    produced = value.get("exports")
    if isinstance(produced, dict):
        for name, text in produced.items():
            if isinstance(text, str) and name not in exports and name not in known and not name.endswith("_error"):
                print(f"  {entry['id']}: new export {name} has no fixture record; add it by hand")
    # An export the manifest records for other entries, produced here now but
    # not in the stored response (an entry may leave a produced export out).
    for name in sorted(set(known) - set(exports)):
        try:
            artifact = export_bytes(value, name, status)
            before = export_bytes(previous, name, old_status)
        except KeyError:
            continue
        if artifact is None or before is not None:
            continue
        template_id, template = known[name]
        record = record_like(template_id, template, entry["id"])
        if not record.get("path", "").rpartition("/")[2].startswith(entry["id"]):
            print(f"  {entry['id']}: new export {name} has no fixture record; add it by hand")
            continue
        fmt = next((value[k] for k in (name + "_format", name + "_export_format") if isinstance(value.get(k), str)), None)
        if fmt and record.get("format") and fmt != record["format"]:
            old_suffix = "." + record["format"]
            for key in ("filename", "path"):
                if key in record and record[key].endswith(old_suffix):
                    record[key] = record[key][: -len(old_suffix)] + "." + fmt
            record["format"] = fmt
        record.update(bytes=len(artifact), sha256=sha256(artifact))
        with open(path_of(record), "wb") as handle:
            handle.write(artifact)
        exports[name] = record
        print(f"  {entry['id']}: new export record {name}")


def post(origin: str, entry: dict, body: bytes):
    request = urllib.request.Request(
        origin + entry["path"], data=body, method=entry.get("method", "POST"),
        headers=dict(entry.get("headers", {})))
    try:
        with urllib.request.urlopen(request, timeout=600) as response:
            return response.status, response.read()
    except urllib.error.HTTPError as error:
        return error.code, error.read()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--origin", required=True)
    parser.add_argument("--root", default=os.path.join(os.path.dirname(__file__), ".."))
    parser.add_argument("--only", default="")
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    manifest_path = os.path.join(root, args.manifest) if not os.path.isabs(args.manifest) else args.manifest
    with open(manifest_path, "rb") as handle:
        raw_manifest = handle.read()
    manifest = json.loads(raw_manifest.decode("utf-8"))
    # Some manifests are stored with CRLF line ends (byte-identical fixtures,
    # see .gitattributes); a rewrite keeps the file's line ends.
    newline = "\r\n" if b"\r\n" in raw_manifest else "\n"
    only = re.compile(args.only) if args.only else None
    known_exports = {}
    for entry in manifest["entries"]:
        for name, record in (entry.get("exports") or {}).items():
            known_exports.setdefault(name, (entry["id"], record))
    changed, same, rewritten = [], 0, 0

    def path_of(record):
        return os.path.join(root, record["path"])

    # A manifest field mirrors the response (operation, executed, error,
    # native_calls, variant, ...) when it equals the stored response's field
    # of the same name in every entry; input records that sometimes match
    # (source, model, ...) are not mirrors.
    agree, differ = set(), set()
    for entry in manifest["entries"]:
        with open(path_of(entry["response"]), "rb") as handle:
            previous = parse(handle.read())
        for key, field in entry.items():
            if key not in STRUCTURAL and key in previous:
                (agree if previous[key] == field else differ).add(key)
    mirrors = sorted(agree - differ)

    for entry in manifest["entries"]:
        with open(path_of(entry["request_body"]), "rb") as handle:
            body = handle.read()
        status, data = post(args.origin.rstrip("/"), entry, body)
        with open(path_of(entry["response"]), "rb") as handle:
            stored = handle.read()
        if data == stored and status == entry["response"]["status"]:
            same += 1
            continue
        changed.append(entry["id"])
        if not args.write or (only and not only.search(entry["id"])):
            continue
        response = entry["response"]
        with open(path_of(response), "wb") as handle:
            handle.write(data)
        old_status = response.get("status")
        response.update(bytes=len(data), sha256=sha256(data), status=status)
        if entry.get("expected_status") == old_status:
            entry["expected_status"] = status
        if "response_headers" in entry and "Content-Length" in entry["response_headers"]:
            entry["response_headers"]["Content-Length"] = str(len(data))
        value = parse(data)
        # Manifest fields that mirror a response field follow the new response.
        for key in mirrors:
            if key in entry and key in value:
                entry[key] = value[key]
        exports = entry.get("exports")
        if exports is not None:
            refresh_exports(entry, exports, value, status, parse(stored), old_status, known_exports, path_of)
        rewritten += 1

    if rewritten:
        with open(manifest_path, "w", encoding="utf-8", newline=newline) as handle:
            handle.write(json.dumps(manifest, indent=2, ensure_ascii=True) + "\n")
    print(f"{same} unchanged, {len(changed)} changed, {rewritten} rewritten")
    for name in changed:
        print("  changed:", name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
