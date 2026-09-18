"""Exercise case-relation table output through the CLI and local HTTP route.

Run only after all native compilation/linking has finished. This checks adapter
behavior, not dataframe index/dtype or complete PM4Py equivalence.
"""

import argparse
import hashlib
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check_output(body, activities, case_key="case_id"):
    require(body["family"] == "MERGE_CASE_RELATIONS", "wrong response family")
    table = body["table"]
    index = table["columns"].index("activity")
    require([row[index]["value"] for row in table["rows"]] == activities,
            "table row order differs")
    require(case_key + "_LEFT" in table["columns"] and case_key + "_RIGHT" in table["columns"],
            "relation columns missing")
    require(body["log"] is not None, "compatibility log projection failed")
    return body


def multipart(files):
    boundary = "minera-case-relations-fixture"
    chunks = []
    for key, path in files.items():
        chunks.append((f'--{boundary}\r\nContent-Disposition: form-data; name="{key}"; '
                       f'filename="{path.name}"\r\nContent-Type: text/csv\r\n\r\n').encode())
        chunks.extend((path.read_bytes(), b"\r\n"))
    chunks.append(f"--{boundary}--\r\n".encode())
    return b"".join(chunks), "multipart/form-data; boundary=" + boundary


def run(export, server):
    hashes = {"export": digest(export), "server": digest(server)}
    checks = []
    with tempfile.TemporaryDirectory(prefix="minera-case-relations-") as directory:
        root = Path(directory)
        base, incoming, relations = (root / name for name in ("base.csv", "incoming.csv", "relations.csv"))
        base.write_text("case_id,activity,timestamp\nL2,base-z,z\nL1,base-a,a\nL1,base-empty,\n")
        incoming.write_text("case_id,activity,timestamp\nR1,incoming-b,b\nR2,incoming-y,y\n")
        relations.write_text("case_id_LEFT,case_id_RIGHT\nL2,R2\nL1,R1\nL1,R1\n")
        expected = ["base-a", "incoming-b", "incoming-b", "base-empty", "incoming-y", "base-z"]
        command = [str(export), str(base), "--merge-case-relations", str(incoming),
                   "--case-relations", str(relations)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=30)
        require(result.returncode == 0, "CLI merge failed: " + result.stderr)
        cli = check_output(json.loads(result.stdout), expected)
        checks.append("CLI lexical timestamps, missing configured timestamps, duplicate relations and case order")
        output = root / "result.json"
        result = subprocess.run(command + ["-o", str(output)], capture_output=True, text=True, timeout=30)
        require(result.returncode == 0 and not result.stdout.strip(), "CLI output file routing failed")
        require(json.loads(output.read_text()) == cli, "CLI file and stdout results differ")
        checks.append("CLI output file matches stdout")
        result = subprocess.run(command + ["-o", str(root / "missing" / "result.json")],
                                capture_output=True, text=True, timeout=30)
        require(result.returncode != 0, "CLI invalid output destination succeeded")
        checks.append("CLI invalid output destination rejected")
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            port = listener.getsockname()[1]
        flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
        process = subprocess.Popen([str(server), "--port", str(port)],
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                   creationflags=flags)
        try:
            deadline = time.monotonic() + 15
            while True:
                require(process.poll() is None, "server exited before readiness")
                try:
                    with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                        break
                except OSError:
                    require(time.monotonic() < deadline, "server readiness timed out")
                    time.sleep(0.1)
            payload, content_type = multipart({"base_log": base, "incoming_log": incoming, "case_relations": relations})
            url = f"http://127.0.0.1:{port}/api/merge-case-relations"
            request = urllib.request.Request(url, payload, {"Content-Type": content_type})
            with urllib.request.urlopen(request, timeout=30) as response:
                http = check_output(json.load(response), expected)
            require(http == cli, "HTTP and CLI table responses differ")
            checks.append("HTTP merge matches CLI including compatibility log")
            # A selected column absent from both inputs is a validation error.
            request = urllib.request.Request(url + "?merge_timestamp_key=absent", payload,
                                             {"Content-Type": content_type})
            try:
                urllib.request.urlopen(request, timeout=30).close()
            except urllib.error.HTTPError as error:
                require(error.code == 400, "wrong missing-column HTTP status")
            else:
                raise RuntimeError("HTTP missing timestamp selector succeeded")
            checks.append("HTTP missing timestamp column rejected")
            request = urllib.request.Request(url + "?merge_timestamp_key=time%3Atimestamp", payload,
                                             {"Content-Type": content_type})
            try:
                urllib.request.urlopen(request, timeout=30).close()
            except urllib.error.HTTPError as error:
                require(error.code == 400, "wrong explicit timestamp selector HTTP status")
            else:
                raise RuntimeError("HTTP silently aliased explicit timestamp selector")
            result = subprocess.run(command + ["--merge-timestamp-key", "time:timestamp"],
                                    capture_output=True, text=True, timeout=30)
            require(result.returncode != 0, "CLI silently aliased explicit timestamp selector")
            checks.append("CLI and HTTP preserve explicit missing canonical selector")
            incoming.write_text("case_id,activity\nR1,incoming-b\nR2,incoming-y\n")
            expected_missing = ["base-a", "base-empty", "incoming-b", "incoming-b", "base-z", "incoming-y"]
            result = subprocess.run(command, capture_output=True, text=True, timeout=30)
            require(result.returncode == 0, "CLI rejected one-sided timestamp column: " + result.stderr)
            one_side = check_output(json.loads(result.stdout), expected_missing)
            payload, content_type = multipart({"base_log": base, "incoming_log": incoming, "case_relations": relations})
            request = urllib.request.Request(url, payload, {"Content-Type": content_type})
            with urllib.request.urlopen(request, timeout=30) as response:
                require(check_output(json.load(response), expected_missing) == one_side,
                        "HTTP and CLI one-sided timestamp results differ")
            checks.append("CLI and HTTP support one-sided timestamp columns")
            base.write_text("case_id,activity,timestamp,time:timestamp\nL1,base,a,z\n")
            incoming.write_text("case_id,activity,timestamp,time:timestamp\nR1,incoming,z,a\n")
            relations.write_text("case_id_LEFT,case_id_RIGHT\nL1,R1\n")
            result = subprocess.run(command + ["--merge-timestamp-key", "time:timestamp"],
                                    capture_output=True, text=True, timeout=30)
            require(result.returncode == 0, "CLI explicit canonical timestamp failed: " + result.stderr)
            exact = check_output(json.loads(result.stdout), ["incoming", "base"])
            payload, content_type = multipart({"base_log": base, "incoming_log": incoming, "case_relations": relations})
            request = urllib.request.Request(url + "?merge_timestamp_key=time%3Atimestamp", payload,
                                             {"Content-Type": content_type})
            with urllib.request.urlopen(request, timeout=30) as response:
                require(check_output(json.load(response), ["incoming", "base"]) == exact,
                        "HTTP and CLI exact timestamp results differ")
            checks.append("CLI and HTTP honor explicit canonical column when both aliases exist")
            base.write_text("case_id,activity,timestamp,order_id,when\nOLD-L,base,z,L1,b\nOLD-L,base-empty,z,L1,\n")
            incoming.write_text("case_id,activity,timestamp,order_id,when\nOLD-R,incoming,z,R1,a\n")
            relations.write_text("order_id_LEFT,order_id_RIGHT\nL1,R1\n")
            result = subprocess.run(command + ["--merge-case-id-key", "order_id", "--merge-timestamp-key", "when"],
                                    capture_output=True, text=True, timeout=30)
            require(result.returncode == 0, "CLI custom selectors failed: " + result.stderr)
            custom = check_output(json.loads(result.stdout), ["base-empty", "incoming", "base"], "order_id")
            traces = custom["log"]["traces"]
            require(len(traces) == 1 and traces[0]["case_id"] == "L1", "compatibility log ignored selected case key")
            require([event["timestamp"] for event in traces[0]["events"]] == ["", "a", "b"],
                    "compatibility log ignored selected timestamp key")
            payload, content_type = multipart({"base_log": base, "incoming_log": incoming, "case_relations": relations})
            request = urllib.request.Request(url + "?merge_case_id_key=order_id&merge_timestamp_key=when", payload,
                                             {"Content-Type": content_type})
            with urllib.request.urlopen(request, timeout=30) as response:
                require(json.load(response) == custom, "HTTP and CLI custom-selector projections differ")
            checks.append("CLI and HTTP project selected case/timestamp columns into compatibility log")
            base.write_text("case_id,activity,timestamp,link\nL1,base,a,base-link\n")
            incoming.write_text("case_id,activity,timestamp,link\nR1,incoming,b,incoming-link\n")
            relations.write_text('target,source,link,extra\nL1,R1,relation-link,"quoted, extra"\n')
            result = subprocess.run(command + ["--merge-left-column", "target", "--merge-right-column", "source"],
                                    capture_output=True, text=True, timeout=30)
            require(result.returncode == 0, "CLI relation fields failed: " + result.stderr)
            extras = json.loads(result.stdout)
            table = extras["table"]
            require(table["columns"] == ["case_id", "activity", "timestamp", "link", "link_x", "target", "source", "link_y", "extra"],
                    "relation collision columns/order differ")
            row = dict(zip(table["columns"], table["rows"][1], strict=True))
            require(row["link_x"]["value"] == "incoming-link" and row["link_y"]["value"] == "relation-link"
                    and row["extra"]["value"] == "quoted, extra", "relation fields lost")
            payload, content_type = multipart({"base_log": base, "incoming_log": incoming, "case_relations": relations})
            request = urllib.request.Request(url + "?merge_left_column=target&merge_right_column=source", payload,
                                             {"Content-Type": content_type})
            with urllib.request.urlopen(request, timeout=30) as response:
                require(json.load(response) == extras, "HTTP and CLI relation metadata differ")
            checks.append("CLI and HTTP preserve quoted relation metadata, collision suffixes and explicit relation selectors")
            base.write_text("order_id,step,clock\nL1,base,b\n")
            incoming.write_text("order_id,step,clock\nR1,incoming,a\n")
            relations.write_text("case_id_LEFT,case_id_RIGHT\nL1,R1\n")
            result = subprocess.run(command + ["--case-column", "order_id", "--activity-column", "step", "--timestamp-column", "clock"],
                                    capture_output=True, text=True, timeout=30)
            require(result.returncode == 0, "CLI custom import columns failed: " + result.stderr)
            imported = json.loads(result.stdout)
            traces = imported["log"]["traces"]
            require(len(traces) == 1 and traces[0]["case_id"] == "L1", "custom import case selector was lost")
            require([(event["activity"], event["timestamp"]) for event in traces[0]["events"]] == [("incoming", "a"), ("base", "b")],
                    "custom import activity/timestamp columns were lost")
            checks.append("CLI custom import columns retain default relation selectors and log roles")
            xes = root / "base.xes"
            xes.write_text('<log xes.version="1.0"><trace><string key="concept:name" value="L1"/>'
                           '<event><string key="concept:name" value="base"/>'
                           '<date key="time:timestamp" value="2024-01-01T00:00:00Z"/>'
                           '</event></trace></log>')
            incoming.write_text("case_id,activity,timestamp\nR1,incoming,2024-01-02T00:00:00Z\n")
            relations.write_text("lhs,rhs\nL1,R1\n")
            result = subprocess.run([str(export), str(xes), "--merge-case-relations", str(incoming),
                                     "--case-relations", str(relations), "--merge-left-column", "lhs",
                                     "--merge-right-column", "rhs", "--merge-left-suffix", "_same",
                                     "--merge-right-suffix", "_same"], capture_output=True, text=True, timeout=30)
            require(result.returncode == 0, "mixed XES explicit relation selectors failed: " + result.stderr)
            legacy = json.loads(result.stdout)
            require([event["activity"] for event in legacy["log"]["traces"][0]["events"]] == ["base", "incoming"],
                    "mixed XES legacy merge changed")
            payload, content_type = multipart({"base_log": xes, "incoming_log": incoming, "case_relations": relations})
            query = "?merge_left_column=lhs&merge_right_column=rhs&merge_left_suffix=_same&merge_right_suffix=_same"
            with urllib.request.urlopen(urllib.request.Request(url + query, payload, {"Content-Type": content_type}), timeout=30) as response:
                require(json.load(response) == legacy, "HTTP and CLI mixed XES merges differ")
            checks.append("Mixed XES/CSV CLI and HTTP honor explicit relation selectors over unused suffixes")
        finally:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=10)
    require(hashes == {"export": digest(export), "server": digest(server)},
            "native executable changed during verification")
    return {"checks": checks, "native_sha256_before_and_after": hashes}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--export", type=Path, required=True)
    parser.add_argument("--server", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        report = run(args.export.resolve(), args.server.resolve())
    except urllib.error.HTTPError as error:
        raise RuntimeError("HTTP validation failed: " + error.read().decode("utf-8", errors="replace")) from error
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))
