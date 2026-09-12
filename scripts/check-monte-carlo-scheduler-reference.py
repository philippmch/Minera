"""Compare CLI simulation traces and diagnostics with saved public outcomes."""
import argparse
from datetime import datetime
import importlib.util
import json
import math
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET

spec = importlib.util.spec_from_file_location('support',
    Path(__file__).with_name('check-pnml-model-transports.py'))
support = importlib.util.module_from_spec(spec)
spec.loader.exec_module(support)


def compare(actual, expected, path=''):
    if isinstance(expected, dict):
        assert actual.keys() == expected.keys(), (path, actual, expected)
        for key in expected:
            compare(actual[key], expected[key], path + '.' + key)
    elif isinstance(expected, list):
        assert len(actual) == len(expected), (path, actual, expected)
        for i, (a, e) in enumerate(zip(actual, expected)):
            compare(a, e, path + f'[{i}]')
    elif isinstance(expected, (float, int)):
        assert math.isclose(actual, expected, rel_tol=1e-15, abs_tol=1e-10), (path, actual, expected)
    else:
        assert actual == expected, (path, actual, expected)


def trace_events(xml):
    root = ET.fromstring(xml)
    for node in root.iter():
        node.tag = node.tag.rsplit('}', 1)[-1]
    return [[{attr.attrib['key']: attr.attrib['value'] for attr in event}
             for event in trace.findall('event')] for trace in root.findall('trace')]


def normalized_events(traces):
    return [[dict(activity=e['concept:name'], timestamp=datetime.fromisoformat(
                 e['time:timestamp'].replace('Z', '+00:00')).timestamp()) for e in trace]
            for trace in traces]


def public_result(actual, case):
    result = {key: actual['diagnostics'][key] for key in [
        'cases_ex_time', 'median_cases_ex_time', 'case_arrival_ratio', 'total_cases_time']}
    for key, ids, index in [
        ('output_places_interval_trees', case['places'], 'place_index'),
        ('output_transitions_interval_trees', [t[0] for t in case['transitions']], 'transition_index')]:
        result[key] = {ids[tree[index]]: [dict(start=x['start'], end=x['end'], data={})
                                        for x in tree['intervals']] for tree in actual[key]}
    return result


def check_http_rows(executable, directory, log, rows):
    with socket.socket() as listener:
        listener.bind(('127.0.0.1', 0))
        port = listener.getsockname()[1]
    base = f'http://127.0.0.1:{port}'
    def request(path, data=None, content_type='application/xml'):
        req = urllib.request.Request(base + path, data=data, headers={'Content-Type': content_type})
        try:
            with urllib.request.urlopen(req, timeout=30) as response:
                return response.status, response.read().decode()
        except urllib.error.HTTPError as error:
            return error.code, error.read().decode()
    checks = []
    with (directory / 'server.log').open('w', encoding='utf-8') as server_log:
        server = subprocess.Popen([str(executable.resolve()), '--port', str(port)],
            stdout=server_log, stderr=server_log,
            creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        try:
            for _ in range(200):
                try:
                    if request('/api/version')[0] == 200:
                        break
                except (OSError, urllib.error.URLError):
                    if server.poll() is not None:
                        raise RuntimeError('server exited before readiness')
                time.sleep(0.05)
            else:
                raise RuntimeError('server did not become ready')
            status, body = request('/api/mine', log.read_bytes(), 'text/csv')
            assert status == 200, body
            for row in rows:
                case = row['spec']
                model = support.pnml(case['places'], case['transitions'], case['arcs'],
                                     case['initial'], case['final'])
                status, body = request('/api/check-model', model.encode())
                assert status == 200, body
                parameters = dict(miner='imported', monte_carlo_simulations=str(case['simulations']),
                    monte_carlo_case_arrival_ratio=str(case['arrival']), monte_carlo_start='1000000',
                    monte_carlo_transition_durations=','.join('normal:' + str(case['durations'][t[0]]) + ';0'
                                                             for t in case['transitions']),
                    monte_carlo_transition_weights=','.join(str(case.get('weights', {}).get(t[0], 1))
                                                            for t in case['transitions']),
                    monte_carlo_default_resources_per_place=str(case['capacity']),
                    monte_carlo_enable_diagnostics=str(case.get('diagnostics', False)).lower())
                if case.get('resource_key_type') == 'object':
                    parameters['monte_carlo_resources_per_place'] = ','.join(
                        f'{case["places"].index(name)}={value}' for name, value in case['resources'].items())
                query = urllib.parse.urlencode(parameters)
                status, body = request('/api/monte-carlo?' + query, b'')
                check = dict(name=case['name'], route='http')
                if 'error' in row:
                    check.update(passed=status != 200, expected_error=row['error'])
                    if status == 200:
                        check['unexpected_success'] = json.loads(body)
                elif status != 200:
                    check.update(passed=False, native_error=body)
                else:
                    actual = public_result(json.loads(body), case)
                    errors = []
                    for key in row['result']:
                        try:
                            compare(actual[key], row['result'][key], key)
                        except AssertionError as error:
                            errors.append(str(error))
                    status, xml = request('/api/playout?mode=monte_carlo&' + query, b'')
                    if status != 200:
                        errors.append(xml)
                    else:
                        try:
                            compare(normalized_events(trace_events(xml)),
                                    normalized_events([t['events'] for t in row['log']]), 'events')
                        except AssertionError as error:
                            errors.append(str(error))
                    check.update(passed=not errors, errors=errors)
                checks.append(check)
                print(case['name'], 'http', 'passed' if check['passed'] else 'DIFF', flush=True)
        finally:
            server.terminate()
            try:
                server.wait(timeout=10)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait(timeout=10)
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exporter', type=Path, required=True)
    parser.add_argument('--server', type=Path)
    parser.add_argument('--fixtures', type=Path, default=Path(__file__).resolve().parents[1] /
                        'docs/monte-carlo-scheduler-reference-fixtures.json')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    rows = json.loads(args.fixtures.read_text(encoding='utf-8'))['observations']
    checks = []
    with tempfile.TemporaryDirectory(prefix='minera-monte-carlo-scheduler-') as temporary:
        directory = Path(temporary)
        log = directory / 'log.csv'
        log.write_text('case_id,activity,timestamp\n1,A,2026-01-01T00:00:00Z\n'
                       '1,B,2026-01-01T00:00:01Z\n', encoding='utf-8')
        for row in rows:
            case = row['spec']
            model, output = directory / 'model.pnml', directory / 'generated.xes'
            model.write_text(support.pnml(case['places'], case['transitions'], case['arcs'],
                                          case['initial'], case['final']), encoding='utf-8')
            durations = ','.join('normal:' + str(case['durations'][t[0]]) + ';0'
                                 for t in case['transitions'])
            weights = ','.join(str(case.get('weights', {}).get(t[0], 1)) for t in case['transitions'])
            arguments = [str(args.exporter.resolve()), str(log), '--check-pnml', str(model),
                '--playout-miner', 'imported', '--monte-carlo-simulations', str(case['simulations']),
                '--monte-carlo-case-arrival-ratio', str(case['arrival']),
                '--monte-carlo-start', '1000000', '--monte-carlo-transition-durations', durations,
                '--monte-carlo-transition-weights', weights,
                '--monte-carlo-default-resources-per-place', str(case['capacity']),
                '--monte-carlo-enable-diagnostics', str(case.get('diagnostics', False)).lower()]
            if case.get('resource_key_type') == 'object':
                resources = ','.join(f'{case["places"].index(name)}={value}'
                                     for name, value in case['resources'].items())
                arguments += ['--monte-carlo-resources-per-place', resources]
            completed = subprocess.run([*arguments, '--monte-carlo-json'], capture_output=True,
                                       text=True, encoding='utf-8', timeout=30)
            check = dict(name=case['name'], route='cli')
            if 'error' in row:
                check['expected_error'] = row['error']
                check['passed'] = completed.returncode != 0
                if not check['passed']:
                    check['unexpected_success'] = json.loads(completed.stdout)
            elif completed.returncode:
                check.update(passed=False, native_error=completed.stderr + completed.stdout)
            else:
                actual = public_result(json.loads(completed.stdout), case)
                errors = []
                for key in row['result']:
                    try:
                        compare(actual[key], row['result'][key], key)
                    except AssertionError as error:
                        errors.append(str(error))
                generated = subprocess.run([*arguments, '--playout', str(output)], capture_output=True,
                                           text=True, encoding='utf-8', timeout=30)
                if generated.returncode:
                    errors.append(generated.stderr + generated.stdout)
                else:
                    try:
                        compare(normalized_events(trace_events(output.read_text(encoding='utf-8'))),
                                normalized_events([t['events'] for t in row['log']]), 'events')
                    except AssertionError as error:
                        errors.append(str(error))
                check.update(passed=not errors, errors=errors)
            checks.append(check)
            print(case['name'], 'passed' if check['passed'] else 'DIFF', flush=True)
        if args.server:
            checks.extend(check_http_rows(args.server, directory, log, rows))
    args.output.write_text(json.dumps(dict(checks=checks), indent=2) + '\n', encoding='utf-8')
    passed = sum(c['passed'] for c in checks)
    print(f'{passed}/{len(checks)} scheduler comparisons passed')
    return 0 if passed == len(checks) else 1


if __name__ == '__main__':
    raise SystemExit(main())
