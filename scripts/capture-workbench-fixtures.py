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
response's "exports" object (written as WTF-8, so lone surrogates survive).

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
    with open(manifest_path, encoding="utf-8") as handle:
        manifest = json.load(handle)
    only = re.compile(args.only) if args.only else None
    changed, same, rewritten = [], 0, 0

    def path_of(record):
        return os.path.join(root, record["path"])

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
        response.update(bytes=len(data), sha256=sha256(data), status=status)
        if "response_headers" in entry and "Content-Length" in entry["response_headers"]:
            entry["response_headers"]["Content-Length"] = str(len(data))
        exports = entry.get("exports")
        if exports:
            value = json.loads(data.decode("utf-8", "surrogatepass"))
            produced = value.get("exports") or {}
            for name, record in list(exports.items()):
                text = produced.get(name)
                if text is None:
                    del exports[name]
                    continue
                artifact = text.encode("utf-8", "surrogatepass")
                with open(path_of(record), "wb") as handle:
                    handle.write(artifact)
                record.update(bytes=len(artifact), sha256=sha256(artifact))
            for name, text in produced.items():
                if text is not None and name not in exports:
                    print(f"  {entry['id']}: new export {name} has no fixture record; add it by hand")
        rewritten += 1

    if rewritten:
        with open(manifest_path, "w", encoding="utf-8") as handle:
            handle.write(json.dumps(manifest, indent=2, ensure_ascii=True) + "\n")
    print(f"{same} unchanged, {len(changed)} changed, {rewritten} rewritten")
    for name in changed:
        print("  changed:", name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
