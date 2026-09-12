"""Check raw Alpha+ graph/marking transports through the native CLI and HTTP API.

The cases are deliberately small and deterministic.  This gate checks the
public JSON shape and live/detached marking identity; it does not import the
Python reference runtime or make probabilistic claims about graph discovery.
"""
import argparse
import json
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from xml.sax.saxutils import quoteattr


ROOT = Path(__file__).resolve().parents[1]
PROCESS_TIMEOUT = 30
HTTP_TIMEOUT = 15
MINE_TIMEOUT = 60
READY_TIMEOUT = 20


def xes(traces, activity_attribute="concept:name", selected_values=None):
    """Serialize simple traces as XES without writing outside the temp dir."""
    selected_values = selected_values or {}
    output = ["<log>"]
    for trace in traces:
        output.append("<trace>")
        for label in trace:
            output.append("<event>")
            output.append("<string key=" + quoteattr("concept:name") +
                          " value=" + quoteattr(selected_values.get(label, label)) + "/>" )
            if activity_attribute != "concept:name":
                output.append("<string key=" + quoteattr(activity_attribute) +
                              " value=" + quoteattr(label) + "/>" )
            output.append("</event>")
        output.append("</trace>")
    output.append("</log>")
    return "".join(output)


def _integer(value, field):
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise ValueError(f"{field} must be a nonnegative integer")


def _place_name(value):
    if not isinstance(value, dict):
        raise ValueError("Alpha+ place name must be an object")
    if set(value) == {"boundary"}:
        if not isinstance(value["boundary"], str) or not value["boundary"]:
            raise ValueError("Alpha+ boundary name must be non-empty text")
        return
    if set(value) != {"before", "after"}:
        raise ValueError("Alpha+ place name has an unexpected shape")
    for side in ("before", "after"):
        if not isinstance(value[side], list) or not all(isinstance(item, str) for item in value[side]):
            raise ValueError("Alpha+ place pairs must contain text arrays")


def validate_result(value):
    """Validate the raw result contract before comparing transports."""
    if not isinstance(value, dict) or value.get("family") != "ALPHA_PLUS":
        raise ValueError("response is not an ALPHA_PLUS result")
    fields = {"family", "places", "transitions", "detached_initial_marking", "detached_final_marking"}
    if not fields.issubset(value):
        raise ValueError("response is missing Alpha+ result fields")
    places = value["places"]
    transitions = value["transitions"]
    detached_initial = value["detached_initial_marking"]
    detached_final = value["detached_final_marking"]
    if not all(isinstance(items, list) for items in (places, transitions, detached_initial, detached_final)):
        raise ValueError("Alpha+ result fields must be arrays")

    for index, place in enumerate(places):
        if not isinstance(place, dict) or set(place) != {"id", "name", "initial", "final"}:
            raise ValueError("Alpha+ place has an unexpected shape")
        if place["id"] != index:
            raise ValueError("Alpha+ place ids must be contiguous")
        _place_name(place["name"])
        _integer(place["initial"], "initial marking")
        _integer(place["final"], "final marking")

    for index, transition in enumerate(transitions):
        if not isinstance(transition, dict) or set(transition) != {"id", "label", "inputs", "outputs"}:
            raise ValueError("Alpha+ transition has an unexpected shape")
        if transition["id"] != index:
            raise ValueError("Alpha+ transition ids must be contiguous")
        if transition["label"] is not None and not isinstance(transition["label"], str):
            raise ValueError("Alpha+ transition labels must be text or null")
        for direction in ("inputs", "outputs"):
            arcs = transition[direction]
            if not isinstance(arcs, list) or any(
                    isinstance(place_id, bool) or not isinstance(place_id, int) or
                    place_id < 0 or place_id >= len(places) for place_id in arcs):
                raise ValueError("Alpha+ transition arc references an invalid place")

    def detached(markings):
        for marking in markings:
            if not isinstance(marking, dict) or set(marking) != {"name", "tokens"}:
                raise ValueError("Alpha+ detached marking has an unexpected shape")
            _place_name(marking["name"])
            if isinstance(marking["tokens"], bool) or not isinstance(marking["tokens"], int) or marking["tokens"] <= 0:
                raise ValueError("Alpha+ detached marking tokens must be positive integers")

    detached(detached_initial)
    detached(detached_final)
    if sum(place["initial"] for place in places) + sum(item["tokens"] for item in detached_initial) != 1:
        raise ValueError("Alpha+ initial marking does not contain one token")
    if sum(place["final"] for place in places) + sum(item["tokens"] for item in detached_final) != 1:
        raise ValueError("Alpha+ final marking does not contain one token")
    return value


def assert_case(value, name):
    validate_result(value)
    if name == "empty-trace":
        if len(value["places"]) != 1 or value["transitions"] or len(value["detached_initial_marking"]) != 1:
            raise ValueError("empty trace did not preserve its detached initial boundary")
    elif name == "singleton":
        if not any(transition["label"] == "a" for transition in value["transitions"]):
            raise ValueError("singleton result is missing its visible transition")
        if value["detached_initial_marking"] or value["detached_final_marking"]:
            raise ValueError("singleton result unexpectedly detached a marking")
    elif name == "one-loop":
        loops = [transition for transition in value["transitions"] if transition["label"] == "a"]
        if len(loops) != 1 or not loops[0]["inputs"] or loops[0]["inputs"] != loops[0]["outputs"]:
            raise ValueError("one-loop result did not retain a self-loop")
        if len(value["detached_initial_marking"]) != 1:
            raise ValueError("one-loop result lost its detached initial boundary")
        place = value["places"][loops[0]["inputs"][0]]
        if place["name"] != value["detached_initial_marking"][0]["name"]:
            raise ValueError("one-loop detached marking changed place identity")
    return value


def _run_flags():
    return {"creationflags": getattr(subprocess, "CREATE_NO_WINDOW", 0)}


def invoke_exporter(exporter, source, *, activity_attribute=None, cleanup=None,
                    max_rows=None, output=None):
    command = [str(exporter.resolve()), str(source), "--alpha-plus-result"]
    if activity_attribute is not None:
        command.extend(["--alpha-activity-attribute", activity_attribute])
    if cleanup is not None:
        command.extend(["--alpha-remove-unconnected", str(cleanup).lower()])
    if max_rows is not None:
        command.extend(["--alpha-max-rows-to-materialize", str(max_rows)])
    if output is not None:
        command.extend(["-o", str(output)])
    try:
        process = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                                encoding="utf-8", timeout=PROCESS_TIMEOUT, **_run_flags())
    except subprocess.TimeoutExpired as error:
        raise RuntimeError("exporter timed out") from error
    result = {"status": process.returncode, "stdout": process.stdout,
              "stderr_present": bool(process.stderr.strip())}
    if process.returncode == 0 and output is None:
        try:
            result["json"] = json.loads(process.stdout)
        except (TypeError, ValueError):
            raise ValueError("exporter emitted malformed JSON") from None
    return result


def _multipart(fields, files):
    boundary = "minera-alpha-plus-model-transports"
    chunks = []
    for name, value in fields.items():
        chunks.extend([f"--{boundary}\r\n".encode(),
                       f'Content-Disposition: form-data; name="{name}"\r\n\r\n'.encode(),
                       str(value).encode(), b"\r\n"])
    for name, filename, content, content_type in files:
        chunks.extend([f"--{boundary}\r\n".encode(),
                       f'Content-Disposition: form-data; name="{name}"; filename="{filename}"\r\n'.encode(),
                       f"Content-Type: {content_type}\r\n\r\n".encode(),
                       content, b"\r\n"])
    chunks.append(f"--{boundary}--\r\n".encode())
    return b"".join(chunks), {"Content-Type": f"multipart/form-data; boundary={boundary}"}


def http_request(base, path, *, body=None, headers=None, method="POST", timeout=HTTP_TIMEOUT):
    if isinstance(body, str):
        body = body.encode("utf-8")
    request = urllib.request.Request(base + path, data=body, headers=headers or {}, method=method)
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            return response.status, response.read().decode("utf-8")
    except urllib.error.HTTPError as error:
        return error.code, error.read().decode("utf-8")


def json_response(status, body, *, expected_status=200, case=None):
    if status != expected_status:
        raise ValueError(f"HTTP status {status}, expected {expected_status}")
    try:
        value = json.loads(body)
    except (TypeError, ValueError):
        raise ValueError("HTTP endpoint emitted malformed JSON") from None
    if case is not None:
        assert_case(value, case)
    else:
        validate_result(value)
    return value


def _choose_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


def start_server(server, frontend=None):
    port = _choose_port()
    command = [str(server.resolve()), "--port", str(port)]
    if frontend is not None:
        command.extend(["--frontend", str(frontend.resolve())])
    temporary = tempfile.NamedTemporaryFile(prefix="minera-alpha-server-", suffix=".log",
                                             delete=False)
    log_path = Path(temporary.name)
    try:
        process = subprocess.Popen(command, cwd=ROOT, stdout=temporary, stderr=subprocess.STDOUT,
                                   **_run_flags())
    except Exception:
        temporary.close()
        log_path.unlink(missing_ok=True)
        raise
    temporary.close()
    base = f"http://127.0.0.1:{port}"
    deadline = time.monotonic() + READY_TIMEOUT
    try:
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError("server exited before readiness")
            try:
                status, _ = http_request(base, "/api/version", method="GET", timeout=1)
                if status == 200:
                    return process, base, log_path
            except (OSError, urllib.error.URLError):
                pass
            time.sleep(0.1)
        raise RuntimeError("server did not become ready")
    except Exception:
        stop_server(process)
        log_path.unlink(missing_ok=True)
        raise


def stop_server(process):
    if process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)


def _check(checks, name, action):
    try:
        details = action()
    except Exception as error:
        checks.append({"name": name, "match": False, "error": str(error)})
        return None
    checks.append({"name": name, "match": True, **({} if details is None else {"details": details})})
    return details


def _cli_success(checks, exporter, source, name, traces, **options):
    def run():
        process = invoke_exporter(exporter, source, **options)
        if process["status"] != 0:
            raise ValueError(f"exporter returned {process['status']}")
        value = process.get("json")
        if value is None:
            raise ValueError("exporter stdout did not contain a result")
        assert_case(value, name)
        return {"status": process["status"], "result": value}
    return _check(checks, "cli-" + name, run)


def _cli_failure(checks, exporter, source, name, **options):
    def run():
        process = invoke_exporter(exporter, source, **options)
        if process["status"] == 0:
            raise ValueError("exporter unexpectedly accepted invalid input")
        return {"status": process["status"], "stderr_present": process["stderr_present"]}
    return _check(checks, "cli-" + name, run)


def _http_success(checks, base, name, traces, body, *, path="/api/discover-alpha-plus",
                  headers=None, expected=None, expected_status=200, check_name=None):
    def run():
        status, response = http_request(base, path, body=body, headers=headers)
        value = json_response(status, response, expected_status=expected_status, case=name)
        if expected is not None and value != expected:
            raise ValueError("HTTP result differs from CLI raw graph")
        return {"status": status, "result": value}
    return _check(checks, check_name or "http-" + name, run)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exporter", type=Path, required=True,
                        help="Path to minera_export (or minera_export.exe)")
    parser.add_argument("--server", type=Path, required=True,
                        help="Path to minera_server (or minera_server.exe)")
    parser.add_argument("--output", type=Path, required=True,
                        help="JSON report path")
    parser.add_argument("--frontend", type=Path,
                        help="Optional frontend directory passed to the server")
    args = parser.parse_args()

    checks = []
    cli_results = {}
    server_process = None
    server_log = None
    try:
        with tempfile.TemporaryDirectory(prefix="minera-alpha-model-transports-") as directory:
            directory = Path(directory)
            empty_trace = directory / "empty-trace.xes"
            singleton = directory / "singleton.xes"
            one_loop = directory / "one-loop.xes"
            selector = directory / "selector.xes"
            empty_log = directory / "empty-log.xes"
            empty_trace.write_text(xes([[]]), encoding="utf-8")
            singleton.write_text(xes([["a"]]), encoding="utf-8")
            one_loop.write_text(xes([["a", "a"]]), encoding="utf-8")
            selector.write_text(xes([["a"]], "phase", {"a": "ignored"}), encoding="utf-8")
            empty_log.write_text(xes([]), encoding="utf-8")

            for name, source in (("empty-trace", empty_trace), ("singleton", singleton), ("one-loop", one_loop)):
                details = _cli_success(checks, args.exporter, source, name,
                                       [[]] if name == "empty-trace" else [["a"]] if name == "singleton" else [["a", "a"]])
                if details is not None:
                    cli_results[name] = details["result"]
            cleanup_details = _check(checks, "cli-one-loop-cleanup",
                                     lambda: _cli_cleanup(args.exporter, one_loop))
            if cleanup_details is not None:
                cli_results["one-loop-cleanup"] = cleanup_details["result"]
            selector_details = _check(checks, "cli-selector", lambda: _cli_selector(args.exporter, selector))
            if selector_details is not None:
                cli_results["selector"] = selector_details["result"]
            row_cap_details = _check(checks, "cli-row-cap", lambda: _cli_row_cap(args.exporter, singleton))
            if row_cap_details is not None:
                cli_results["row-cap"] = row_cap_details["result"]
            output_details = _check(checks, "cli-output-file", lambda: _cli_output_file(
                args.exporter, singleton, directory, cli_results.get("singleton")))
            if output_details is not None:
                cli_results["output-file"] = output_details["result"]
            _cli_failure(checks, args.exporter, empty_log, "empty-log")
            _check(checks, "cli-output-unwritable", lambda: _cli_output_unwritable(
                args.exporter, singleton, directory))

            server_process, base, server_log = start_server(args.server, args.frontend)
            status, _ = http_request(base, "/api/discover-alpha-plus", method="POST")
            _check(checks, "http-no-cached-log", lambda: _expect_status(status, 409))
            _http_success(checks, base, "empty-trace", [[]], xes([[]]).encode(),
                          headers={"Content-Type": "application/xml"}, expected=cli_results.get("empty-trace"))
            _http_success(checks, base, "singleton", [["a"]], xes([["a"]]).encode(),
                          headers={"Content-Type": "application/xml"}, expected=cli_results.get("singleton"))
            _http_success(checks, base, "one-loop", [["a", "a"]], xes([["a", "a"]]).encode(),
                          headers={"Content-Type": "application/xml"}, expected=cli_results.get("one-loop"))
            selector_query = "?" + urllib.parse.urlencode({"alpha_activity_attribute": "phase"})
            _http_success(checks, base, "selector", [["a"]], xes([["a"]], "phase", {"a": "ignored"}).encode(),
                          path="/api/discover-alpha-plus" + selector_query,
                          headers={"Content-Type": "application/xml"}, expected=cli_results.get("selector"))
            _http_success(checks, base, "row-cap", [[]], xes([[]]).encode(),
                          path="/api/discover-alpha-plus?alpha_max_rows_to_materialize=0",
                          headers={"Content-Type": "application/xml"}, expected=cli_results.get("empty-trace"))
            _check(checks, "http-row-cap-rejects-events", lambda: _expect_status(
                http_request(base, "/api/discover-alpha-plus?alpha_max_rows_to_materialize=0",
                             body=xes([["a"]]), headers={"Content-Type": "application/xml"})[0], 400))
            _http_success(checks, base, "row-cap-positive", [["a"]], xes([["a"]]).encode(),
                          path="/api/discover-alpha-plus?alpha_max_rows_to_materialize=1",
                          headers={"Content-Type": "application/xml"}, expected=cli_results.get("row-cap"))
            _check(checks, "http-unknown-parameter", lambda: _expect_status(
                http_request(base, "/api/discover-alpha-plus?bogus=1", body=xes([["a"]]),
                             headers={"Content-Type": "application/xml"})[0], 400))
            _check(checks, "http-empty-log", lambda: _expect_status(
                http_request(base, "/api/discover-alpha-plus", body=xes([]),
                             headers={"Content-Type": "application/xml"})[0], 400))
            multipart_body, multipart_headers = _multipart(
                {"alpha_remove_unconnected": "true"},
                [("log", "input.xes", xes([["a", "a"]]).encode(), "application/xml")])
            _http_success(checks, base, "one-loop", [["a", "a"]], multipart_body,
                          headers=multipart_headers, expected=cli_results.get("one-loop-cleanup"),
                          check_name="http-multipart-one-loop")
            _cached_session(checks, base, cli_results.get("singleton"), xes([["a"]]).encode())
    except Exception as error:
        checks.append({"name": "harness", "match": False, "error": str(error)})
    finally:
        if server_process is not None:
            stop_server(server_process)
        if server_log is not None:
            server_log.unlink(missing_ok=True)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    failures = [check for check in checks if not check["match"]]
    args.output.write_text(json.dumps({"checker": "alpha-plus-model-transports",
                                       "checks": checks,
                                       "passed": len(checks) - len(failures),
                                       "failed": len(failures)}, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks) - len(failures)}/{len(checks)} Alpha+ CLI/HTTP transport checks passed")
    for failure in failures[:12]:
        print(failure["name"] + ": " + failure.get("error", "failed"))
    return 1 if failures else 0


def _expect_status(actual, expected):
    if actual != expected:
        raise ValueError(f"HTTP status {actual}, expected {expected}")
    return {"status": actual}


def _cli_selector(exporter, source):
    process = invoke_exporter(exporter, source, activity_attribute="phase")
    if process["status"] != 0:
        raise ValueError(f"exporter returned {process['status']}")
    value = process["json"]
    assert_case(value, "singleton")
    labels = [transition["label"] for transition in value["transitions"]]
    if "a" not in labels or "ignored" in labels:
        raise ValueError("CLI selector did not select the phase field")
    return {"status": process["status"], "result": value}


def _cli_cleanup(exporter, source):
    process = invoke_exporter(exporter, source, cleanup=True)
    if process["status"] != 0:
        raise ValueError(f"exporter returned {process['status']} for cleanup")
    value = process["json"]
    assert_case(value, "one-loop")
    return {"status": process["status"], "result": value}


def _cli_row_cap(exporter, source):
    output = invoke_exporter(exporter, source, max_rows=1)
    if output["status"] != 0:
        raise ValueError(f"exporter returned {output['status']} for a fitting row cap")
    value = output["json"]
    assert_case(value, "singleton")
    return {"status": output["status"], "result": value}


def _cli_output_file(exporter, source, directory, expected):
    output_path = directory / "alpha-result.json"
    process = invoke_exporter(exporter, source, output=output_path)
    if process["status"] != 0:
        raise ValueError(f"exporter returned {process['status']} for -o")
    if process["stdout"].strip():
        raise ValueError("exporter wrote JSON to stdout despite -o")
    try:
        value = json.loads(output_path.read_text(encoding="utf-8"))
    except (OSError, TypeError, ValueError):
        raise ValueError("exporter -o output is missing or malformed") from None
    assert_case(value, "singleton")
    if expected is not None and value != expected:
        raise ValueError("exporter -o result differs from stdout result")
    return {"status": process["status"], "result": value}


def _cli_output_unwritable(exporter, source, directory):
    output_path = directory / "missing-parent" / "alpha-result.json"
    process = invoke_exporter(exporter, source, output=output_path)
    if process["status"] == 0:
        raise ValueError("exporter unexpectedly wrote through a missing output parent")
    return {"status": process["status"], "stderr_present": process["stderr_present"]}


def _cached_session(checks, base, expected, source):
    body, headers = _multipart(
        {"alpha_variant": "plus", "alpha_activity_attribute": "concept:name"},
        [("log", "cached.xes", source, "application/xml")])
    status, _ = http_request(base, "/api/mine", body=body, headers=headers, timeout=MINE_TIMEOUT)
    if status in (404, 405):
        checks.append({"name": "http-cached-session-log", "match": True, "skipped": True,
                       "reason": "server does not expose /api/mine"})
        return

    def run():
        if status != 200:
            raise ValueError(f"HTTP /api/mine status {status}, expected 200")
        cached_status, cached_body = http_request(base, "/api/discover-alpha-plus", method="POST")
        cached = json_response(cached_status, cached_body, case="singleton")
        if expected is not None and cached != expected:
            raise ValueError("cached-session Alpha+ result differs from uploaded CLI result")
        return {"status": cached_status, "result": cached}
    _check(checks, "http-cached-session-log", run)


if __name__ == "__main__":
    raise SystemExit(main())
