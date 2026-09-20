"""Exercise case-relation table output through the CLI and local HTTP route.

Run only after all native compilation/linking has finished. This checks adapter
behavior and one exact merged index, not complete PM4Py equivalence.
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
        merged_index = cli["table"]["index"]
        require((merged_index["class"], merged_index["dtype"], merged_index["names"]) ==
                ("Index", "int64", [None]), "merged index metadata differs")
        require(all(label["kind"] == "scalar" for label in merged_index["labels"]),
                "merged integer index contains nonscalar labels")
        require([label["value"]["typed_value"] for label in merged_index["labels"]] ==
                [1, 0, 1, 2, 2, 0], "merged index row provenance differs")
        checks.append("CLI preserves base row labels and expanded incoming row labels through sorting")
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
            require("table" in legacy, "mixed XES/CSV merge omitted typed table")
            require("lhs" in legacy["table"]["columns"] and "rhs" in legacy["table"]["columns"],
                    "mixed merge dropped relation selector columns")
            incoming_xes = root / "incoming.xes"
            incoming_xes.write_text('<log xes.version="1.0"><trace><string key="concept:name" value="R1"/>'
                                    '<event><string key="concept:name" value="incoming"/>'
                                    '<date key="time:timestamp" value="2024-01-02T00:00:00Z"/>'
                                    '<int key="score" value="7"/></event></trace></log>', encoding="utf-8")
            base.write_text("case_id,activity,timestamp\nL1,base,2024-01-01T00:00:00Z\n", encoding="utf-8")
            relations.write_text("case_id_LEFT,case_id_RIGHT,relation_note\nL1,R1,first\nL1,R1,second\n", encoding="utf-8")
            for label, base_path, incoming_path in (
                    ("XES/CSV", xes, incoming),
                    ("CSV/XES", base, incoming_xes),
                    ("XES/XES", xes, incoming_xes)):
                mixed_command = [str(export), str(base_path), "--merge-case-relations", str(incoming_path),
                                 "--case-relations", str(relations)]
                result = subprocess.run(mixed_command, capture_output=True, text=True, timeout=30)
                require(result.returncode == 0, label + " CLI merge failed: " + result.stderr)
                mixed = json.loads(result.stdout)
                table = mixed["table"]
                require(len(table["rows"]) == 3, label + " lost duplicate relation expansion")
                note_index = table["columns"].index("relation_note")
                notes = [row[note_index]["value"] for row in table["rows"] if row[note_index] is not None]
                require(notes == ["first", "second"], label + " relation metadata/order differs")
                require([event["activity"] for trace in mixed["log"]["traces"] for event in trace["events"]]
                        == ["base", "incoming", "incoming"], label + " compatibility activity/order differs")
                dates = [cell for row in table["rows"] for cell in row
                         if cell is not None and cell.get("kind") == "date" and isinstance(cell.get("typed_value"), str)]
                require(dates, label + " lost typed XES dates")
                if incoming_path == incoming_xes:
                    score_index = table["columns"].index("score")
                    scores = [row[score_index] for row in table["rows"]
                              if row[score_index] is not None and not row[score_index].get("null", False)]
                    require(len(scores) == 2 and all(cell["kind"] == "float" and cell["typed_value"] == 7 for cell in scores),
                            label + " lost numeric XES values or missing-value float promotion")
                    missing_score = table["rows"][0][score_index]
                    require(missing_score is None or missing_score.get("null", False), label + " filled missing XES attribute")
                payload, content_type = multipart({"base_log": base_path, "incoming_log": incoming_path,
                                                   "case_relations": relations})
                with urllib.request.urlopen(urllib.request.Request(url, payload, {"Content-Type": content_type}), timeout=30) as response:
                    require(json.load(response) == mixed, label + " HTTP/CLI responses differ")
                checks.append(label + " CLI/HTTP typed tables retain duplicate relations and extra metadata")
                rejected = subprocess.run(mixed_command + ["--merge-timestamp-key", "absent"],
                                          capture_output=True, text=True, timeout=30)
                require(rejected.returncode != 0, label + " CLI accepted missing explicit selector")
                try:
                    urllib.request.urlopen(urllib.request.Request(url + "?merge_timestamp_key=absent", payload,
                                             {"Content-Type": content_type}), timeout=30).close()
                except urllib.error.HTTPError as error:
                    require(error.code == 400, label + " missing selector HTTP status differs")
                else:
                    raise RuntimeError(label + " HTTP accepted missing explicit selector")
                checks.append(label + " CLI/HTTP reject missing explicit timestamp selector")
            for selector, trace_key, left_value, right_value, collision_key in (
                    ("concept:name", "concept:name", "L1", "R1", "concept:name"),
                    ("foo", "foo", "LX", "RX", "foo"),
                    (None, "concept:name", "L1", "R1", "timestamp")):
                for path, case, selected, activity, day in ((xes, "L1", left_value, "base", "01"),
                                                           (incoming_xes, "R1", right_value, "incoming", "02")):
                    trace_extra = '' if trace_key == "concept:name" else '<string key="foo" value="' + selected + '"/>'
                    event_extra = '' if collision_key == "concept:name" else '<string key="' + collision_key + '" value="event-' + activity + '"/>'
                    path.write_text('<log><trace><string key="concept:name" value="' + case + '"/>' + trace_extra
                                    + '<event><string key="concept:name" value="' + activity + '"/>' + event_extra
                                    + '<date key="time:timestamp" value="2024-01-' + day + 'T00:00:00Z"/></event></trace></log>', encoding="utf-8")
                relation_key = selector or "case_id"
                relations.write_text(relation_key + "_LEFT," + relation_key + "_RIGHT\n" + left_value + "," + right_value + "\n", encoding="utf-8")
                other = incoming if selector is None else incoming_xes
                args = [str(export), str(xes), "--merge-case-relations", str(other), "--case-relations", str(relations)]
                if selector is not None: args += ["--merge-case-id-key", selector]
                result = subprocess.run(args, capture_output=True, text=True, timeout=30)
                require(result.returncode == 0, "XES collision merge failed: " + result.stderr)
                collided = json.loads(result.stdout)
                require([event["activity"] for event in collided["log"]["traces"][0]["events"]] == ["base", "incoming"],
                        "XES collision lost activity or timestamp ordering")
                case_column = collided["table"]["columns"].index(selector or "case_id")
                require(all(row[case_column]["value"] == left_value for row in collided["table"]["rows"]), "XES collision selected wrong case")
                column = collided["table"]["columns"].index("event:" + collision_key)
                require(collided["table"]["rows"][0][column]["value"] == ("base" if collision_key == "concept:name" else "event-base"),
                        "XES collision discarded original field")
                payload, content_type = multipart({"base_log": xes, "incoming_log": other, "case_relations": relations})
                query = "?" + urllib.parse.urlencode({"merge_case_id_key": selector}) if selector else ""
                with urllib.request.urlopen(urllib.request.Request(url + query, payload, {"Content-Type": content_type}), timeout=30) as response:
                    require(json.load(response) == collided, "XES collision HTTP/CLI differs")
                checks.append("CLI/HTTP retain XES role and colliding event column " + collision_key)
            xes.write_text('<log><trace><string key="concept:name" value="L1"/><event>'
                           '<string key="concept:name" value="base"/><int key="n" value="0"/>'
                           '<date key="time:timestamp" value="2024-01-01T00:00:00Z"/></event></trace></log>', encoding="utf-8")
            incoming_xes.write_text('<log><trace><string key="concept:name" value="R1"/>'
                                    '<event><string key="concept:name" value="incoming"/><int key="n" value="1"/>'
                                    '<date key="time:timestamp" value="2024-01-02T00:00:00Z"/></event>'
                                    '<event><string key="concept:name" value="incoming"/><float key="n" value="2.5"/>'
                                    '<date key="time:timestamp" value="2024-01-03T00:00:00Z"/></event></trace></log>', encoding="utf-8")
            relations.write_text("case_id_LEFT,case_id_RIGHT\nL1,R1\n", encoding="utf-8")
            for empty_base in (False, True):
                if empty_base:
                    base.write_text("case_id,activity,timestamp\n", encoding="utf-8")
                source = base if empty_base else xes
                result = subprocess.run([str(export), str(source), "--merge-case-relations", str(incoming_xes),
                                         "--case-relations", str(relations)], capture_output=True, text=True, timeout=30)
                require(result.returncode == 0, "XES numeric mixture/empty alias failed: " + result.stderr)
                body = json.loads(result.stdout)
                column = body["table"]["columns"].index("n")
                cells = [row[column] for row in body["table"]["rows"]]
                require(all(cell["kind"] == "float" for cell in cells), "XES mixed numeric column did not promote")
                require([cell["typed_value"] for cell in cells] == ([1, 2.5] if empty_base else [0, 1, 2.5]),
                        "XES numeric values/order changed")
                payload, content_type = multipart({"base_log": source, "incoming_log": incoming_xes, "case_relations": relations})
                with urllib.request.urlopen(urllib.request.Request(url, payload, {"Content-Type": content_type}), timeout=30) as response:
                    require(json.load(response) == body, "XES numeric/empty alias HTTP/CLI differs")
                checks.append("CLI/HTTP XES numeric promotion with " + ("empty CSV headers" if empty_base else "XES base"))
            base.write_text("case_id,activity,timestamp\nL1,base,2024-01-01T00:00:00Z\n", encoding="utf-8")
            incoming_xes.write_text('<log xes.version="1.0"><trace><int key="concept:name" value="1"/>'
                                    '<event><string key="concept:name" value="incoming"/>'
                                    '<date key="time:timestamp" value="2024-01-02T00:00:00Z"/>'
                                    '</event></trace></log>', encoding="utf-8")
            relations.write_text("case_id_LEFT,case_id_RIGHT\nL1,1\n", encoding="utf-8")
            result = subprocess.run([str(export), str(base), "--merge-case-relations", str(incoming_xes),
                                     "--case-relations", str(relations)], capture_output=True, text=True, timeout=30)
            require(result.returncode != 0, "numeric XES case joined string relation key")
            payload, content_type = multipart({"base_log": base, "incoming_log": incoming_xes,
                                               "case_relations": relations})
            try:
                urllib.request.urlopen(urllib.request.Request(url, payload, {"Content-Type": content_type}), timeout=30).close()
            except urllib.error.HTTPError as error:
                require(error.code == 400, "numeric XES/string relation error status differs")
            else:
                raise RuntimeError("HTTP numeric XES case joined string relation key")
            checks.append("CLI/HTTP preserve numeric XES case types and reject incompatible string relation keys")
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
