"""Compare native fitting/statistics and model transports with saved public calls.

Run after builds finish. The reference document contains public observations,
never implementation contents. CTMC fallback records must not enter fitted JSON.
"""
import argparse
from datetime import datetime, timedelta, timezone
import importlib.util
import json
import math
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.request
import xml.etree.ElementTree as ET

support_path = Path(__file__).with_name('check-pnml-model-transports.py')
support_spec = importlib.util.spec_from_file_location('pnml_transports', support_path)
support = importlib.util.module_from_spec(support_spec)
support_spec.loader.exec_module(support)


def xes(spec):
    root = ET.Element('log', xmlns='http://www.xes-standard.org/')
    for index, events in enumerate(spec['traces']):
        trace = ET.SubElement(root, 'trace')
        ET.SubElement(trace, 'string', key='concept:name', value=str(index))
        for label, second in events:
            event = ET.SubElement(trace, 'event')
            ET.SubElement(event, 'string', key='concept:name', value=label)
            instant = datetime(2026, 1, 1, tzinfo=timezone.utc) + timedelta(seconds=second)
            ET.SubElement(event, 'date', key='time:timestamp', value=instant.isoformat())
    return ET.tostring(root, encoding='unicode')


def compare(actual, expected, path=''):
    if isinstance(expected, dict):
        assert actual.keys() == expected.keys(), (path, actual, expected)
        for key, value in expected.items():
            compare(actual[key], value, path + '.' + key)
    elif isinstance(expected, list):
        assert len(actual) == len(expected), (path, actual, expected)
        for index, (left, right) in enumerate(zip(actual, expected)):
            compare(left, right, path + f'[{index}]')
    elif isinstance(expected, (float, int)):
        assert math.isclose(actual, expected, rel_tol=1e-12, abs_tol=1e-12), (path, actual, expected)
    else:
        assert actual == expected, (path, actual, expected)


def records(result, ids):
    normalized = {}
    for record in result['map']['transitions']:
        name = ids[record['transition']]
        assert name not in normalized, name
        distribution = record['distribution'].upper()
        assert record['tertiary_parameter'] == 0
        assert record['timing'] == ('immediate' if distribution == 'IMMEDIATE' else 'timed')
        parameters = [record['parameter']]
        if distribution == 'IMMEDIATE':
            assert record['parameter'] == record['secondary_parameter'] == 0
            parameters = []
        elif distribution == 'NORMAL':
            parameters.append(record['secondary_parameter'])
        elif distribution == 'UNIFORM':
            parameters.append(record['secondary_parameter'] - record['parameter'])
        else:
            assert record['secondary_parameter'] == 0
        normalized[name] = dict(distribution=distribution, parameters=parameters,
            weight=record['activation_weight'], priority=record['priority'])
    return normalized


def expected_record(value):
    return dict(distribution=value['distribution'],
        parameters=[] if value['parameters'] == 'UNDEFINED' else
            [float(part) for part in value['parameters'].split(';')],
        weight=value['weight'], priority=value['priority'])


def check_analysis(result, expected):
    negative_mean = any(value['distribution'] == 'NORMAL' and value['parameters'][0] < 0 or
        value['distribution'] == 'UNIFORM' and
        value['parameters'][0] + value['parameters'][1] / 2 < 0 for value in expected.values())
    if negative_mean:
        assert result['reachability']['exhausted'] is True
        assert result['reachability']['states'] == []
        assert result['reachability']['diagnostic'].startswith('CTMC unavailable:')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--exporter', type=Path)
    parser.add_argument('--server', type=Path)
    parser.add_argument('--fixtures', type=Path,
        default=Path(__file__).resolve().parents[1] / 'docs/stochastic-fit-reference-fixtures.json')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    observations = json.loads(args.fixtures.read_text(encoding='utf-8'))['observations']
    checks = []
    server = None
    with tempfile.TemporaryDirectory(prefix='minera-stochastic-fit-') as temporary:
        directory = Path(temporary)
        model, log, output = [directory / name for name in ('model.pnml', 'log.xes', 'fitted.json')]
        server_log = (directory / 'server.log').open('w', encoding='utf-8')

        def run(*arguments, expect_error=False):
            result = subprocess.run(list(map(str, arguments)), capture_output=True,
                text=True, encoding='utf-8', timeout=120)
            if expect_error:
                assert result.returncode != 0 and 'negative' in result.stderr, (arguments, result)
                return
            assert result.returncode == 0, (arguments, result.stderr, result.stdout[:2000])
            return json.loads(result.stdout)

        try:
            if args.server:
                with socket.socket() as listener:
                    listener.bind(('127.0.0.1', 0))
                    port = listener.getsockname()[1]
                server = subprocess.Popen([str(args.server.resolve()), '--port', str(port)],
                    stdout=server_log, stderr=server_log,
                    creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
                base = f'http://127.0.0.1:{port}'

                def get(path, data=None, content_type='application/xml', expect_error=False):
                    request = urllib.request.Request(base + path, data=data,
                        headers={'Content-Type': content_type})
                    try:
                        with urllib.request.urlopen(request, timeout=120) as response:
                            assert not expect_error, path
                            return json.loads(response.read())
                    except urllib.error.HTTPError as error:
                        if expect_error:
                            body = error.read().decode()
                            assert error.code == 400 and 'negative' in body, (path, error.code, body)
                            return
                        raise AssertionError((path, error.code, error.read().decode())) from error

                for _ in range(200):
                    try:
                        get('/api/version')
                        break
                    except (OSError, urllib.error.URLError):
                        assert server.poll() is None, 'server exited before readiness'
                        time.sleep(0.05)
                else:
                    raise AssertionError('server not ready')

            for row in observations:
                spec = row['spec']
                name = spec['name']
                model.write_text(support.pnml(spec['places'], spec['transitions'],
                    spec['arcs'], spec['initial'], spec['final']), encoding='utf-8')
                log.write_text(xes(spec), encoding='utf-8')
                force = spec.get('force_distribution', 'EXPONENTIAL')
                cli_distribution = [] if force is None else ['--stochastic-map-distribution', force.lower()]
                query = '/api/stochastic-map?miner=imported'
                if force is not None:
                    query += '&stochastic_map_distribution=' + force.lower()
                cli_arguments = [log, '--check-pnml', model, '--stochastic-map-miner', 'imported',
                    '--stochastic-map', output, '--no-alignments', *cli_distribution]
                if 'error' in row:
                    run(args.probe.resolve(), model, log, force or 'automatic', expect_error=True)
                    checks.append(dict(name=name, route='native_expected_error', passed=True))
                    if args.exporter:
                        run(args.exporter.resolve(), *cli_arguments, expect_error=True)
                        checks.append(dict(name=name, route='cli_expected_error', passed=True))
                    if args.server:
                        body, headers = support.multipart('log', log.name, log.read_text(encoding='utf-8'))
                        get('/api/mine', body, headers['Content-Type'])
                        get('/api/check-model', model.read_bytes())
                        get(query, expect_error=True)
                        checks.append(dict(name=name, route='http_expected_error', passed=True))
                    continue
                native = run(args.probe.resolve(), model, log, force or 'automatic')
                ids = native['transition_ids']
                # Public Petri transitions form a set. The named duplicate
                # fixture records both observed insertion-order outcomes for
                # identical labels/arcs; require one whole observed outcome.
                failures = []
                for outcome in [row, *row.get('alternatives', [])]:
                    expected = {key: expected_record(value)
                        for key, value in outcome['records'].items()}
                    try:
                        compare(records(native, ids), expected, name + '.native.map')
                        compare(native['statistics'], outcome['statistics'], name + '.native.statistics')
                        break
                    except AssertionError as error:
                        failures.append(str(error))
                else:
                    raise AssertionError(failures)
                checks.append(dict(name=name, route='native', passed=True))
                if 'monte_carlo_map' in native:
                    compare(records({'map': native['monte_carlo_map']}, ids), expected, name + '.monte_carlo')
                    checks.append(dict(name=name, route='monte_carlo_fit', passed=True))
                # Empty event logs are valid fitting inputs. Whole-log discovery
                # adapters have a separate input contract, exercised elsewhere.
                if not any(spec['traces']):
                    continue
                if args.exporter:
                    run(args.exporter.resolve(), *cli_arguments)
                    cli = json.loads(output.read_text(encoding='utf-8'))
                    compare(records(cli, ids), expected, name + '.cli.map')
                    check_analysis(cli, expected)
                    checks.append(dict(name=name, route='cli', passed=True))
                if args.server:
                    body, headers = support.multipart('log', log.name, log.read_text(encoding='utf-8'))
                    get('/api/mine', body, headers['Content-Type'])
                    get('/api/check-model', model.read_bytes())
                    http = get(query)
                    compare(records(http, ids), expected, name + '.http.map')
                    check_analysis(http, expected)
                    if args.exporter:
                        compare(http, cli, name + '.transport')
                    checks.append(dict(name=name, route='http', passed=True))
                if name == 'zero_keys' and (args.exporter or args.server):
                    # A one-sided initial override must clear inherited zero
                    # membership and affect fitting, while final context stays.
                    marking = ','.join('1' if place in ('source', 'middle') else '0'
                                       for place in native['place_ids'])
                    overridden_expected = dict(expected)
                    overridden_expected['b'] = dict(expected['b'], weight=0.5)
                    if args.exporter:
                        run(args.exporter.resolve(), *cli_arguments, '--stochastic-map-initial-marking', marking)
                        overridden = json.loads(output.read_text(encoding='utf-8'))
                        compare(records(overridden, ids), overridden_expected, name + '.cli.override')
                        checks.append(dict(name=name, route='cli_initial_override', passed=True))
                    if args.server:
                        overridden = get(query + '&stochastic_map_initial_marking=' + marking)
                        compare(records(overridden, ids), overridden_expected, name + '.http.override')
                        checks.append(dict(name=name, route='http_initial_override', passed=True))
                print(name + ': passed', flush=True)
        finally:
            if server:
                server.terminate()
                try:
                    server.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    server.kill()
                    server.wait(timeout=10)
            server_log.close()
    args.output.write_text(json.dumps(dict(passed=len(checks), checks=checks), indent=2) + '\n')
    print(f'{len(checks)} checks passed')


if __name__ == '__main__':
    main()
