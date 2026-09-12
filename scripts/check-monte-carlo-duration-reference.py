"""Check underlying public draws and conditional nonnegative simulation laws.

Thresholds are fixed before native execution. RNG streams need not match across
runtimes; native map/direct modes with the same seed must produce identical durations.
"""
import argparse
from bisect import bisect_right
from datetime import datetime
import importlib.util
import json
import math
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import urllib.request
import urllib.parse

spec = importlib.util.spec_from_file_location('scheduler', Path(__file__).with_name('check-monte-carlo-scheduler-reference.py'))
scheduler = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scheduler)


def cdf(x, case, conditional):
    location, spread = case['location'], case['spread']
    if spread == 0:
        return float(x >= location)
    if case['distribution'] == 'uniform':
        lower = max(0, location) if conditional else location
        return max(0, min(1, (x - lower) / (location + spread - lower)))
    if conditional and x < 0:
        return 0
    survival = .5 * math.erfc((x - location) / spread / math.sqrt(2))
    if conditional:
        return 1 - survival / (.5 * math.erfc(-location / spread / math.sqrt(2)))
    return 1 - survival


def ks_one(values, case, conditional):
    ordered = sorted(values)
    n = len(ordered)
    return max(max(abs(cdf(x, case, conditional) - i / n),
                   abs((i + 1) / n - cdf(x, case, conditional))) for i, x in enumerate(ordered))


def ks_two(left, right):
    left, right = sorted(left), sorted(right)
    return max(abs(bisect_right(left, x) / len(left) - bisect_right(right, x) / len(right))
               for x in sorted(set(left + right)))


def validate(values, row, sampling, conditional=True, public_compare=False):
    assert len(values) == sampling['count'], f'Expected {sampling["count"]} samples, got {len(values)}'
    assert all(math.isfinite(x) for x in values)
    if conditional:
        assert all(0 <= x < sampling['arrival'] for x in values), 'Service duration outside nonnegative isolated-arrival range'
    case = row['spec']
    if case['spread'] == 0:
        assert all(x == case['location'] for x in values), 'Fixed service law changed'
        return dict(fixed_exact=True)
    statistic = ks_one(values, case, conditional)
    assert statistic <= sampling['one_sample_ks_limit'], f'One-sample KS {statistic}'
    result = dict(one_sample_ks=statistic)
    if public_compare:
        statistic = ks_two(values, row['simulation_durations'])
        assert statistic <= sampling['two_sample_ks_limit'], f'Two-sample KS {statistic}'
        result['two_sample_ks'] = statistic
    return result


def durations(xml, row, sampling):
    traces = scheduler.normalized_events(scheduler.trace_events(xml))
    assert len(traces) == sampling['count']
    values = []
    for i, trace in enumerate(traces):
        assert len(trace) == 1 and trace[0]['activity'] == 'A'
        timestamp = trace[0]['timestamp']
        value = timestamp - (sampling['start'] + i * sampling['arrival'])
        if row['spec']['spread'] == 0:
            assert timestamp == sampling['start'] + i * sampling['arrival'] + row['spec']['location']
        values.append(value)
    return values


def parameters(row, sampling, mode):
    case = row['spec']
    values = dict(monte_carlo_simulations=str(sampling['count']), monte_carlo_max_steps='1',
        monte_carlo_start=str(sampling['start']), monte_carlo_case_arrival_ratio=str(sampling['arrival']),
        monte_carlo_seed=str(sampling['native_seed']), monte_carlo_default_resources_per_place='1')
    if mode == 'direct':
        values.update(monte_carlo_transition_weights='1',
            monte_carlo_transition_durations=f'{case["distribution"]}:{case["location"]};{case["spread"]}')
    else:
        second = case['spread'] if case['distribution'] == 'normal' else case['location'] + case['spread']
        values['monte_carlo_provided_stochastic_map'] = f'0:timed:{case["distribution"]}:{case["location"]}:{second}:0:1:0'
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', type=Path, default=Path(__file__).resolve().parents[1] / 'docs/monte-carlo-duration-reference-fixtures.json')
    parser.add_argument('--exporter', type=Path)
    parser.add_argument('--server', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    document = json.loads(args.fixtures.read_text(encoding='utf-8'))
    sampling, rows = document['sampling'], document['observations']
    checks = []
    def check(name, route, action):
        try:
            result = action()
            checks.append(dict(name=name, route=route, passed=True, **(result or {})))
        except Exception as error:
            checks.append(dict(name=name, route=route, passed=False, error=f'{type(error).__name__}: {error}'))
        print(name, route, 'passed' if checks[-1]['passed'] else 'DIFF', flush=True)
    for row in rows:
        check(row['spec']['name'], 'public-direct', lambda: validate(row['direct_draws'], row, sampling, conditional=False))
        def public_simulation():
            actual = [datetime.fromisoformat(x).timestamp() - (sampling['start'] + i * sampling['arrival'])
                      for i, x in enumerate(row['simulation_timestamps'])]
            assert actual == row['simulation_durations'], 'Stored durations disagree with raw timestamps'
            return validate(actual, row, sampling)
        check(row['spec']['name'], 'public-simulation', public_simulation)
    with tempfile.TemporaryDirectory(prefix='minera-duration-evidence-') as temporary:
        directory = Path(temporary)
        log, model, output = directory / 'log.csv', directory / 'model.pnml', directory / 'generated.xes'
        log.write_text('case_id,activity,timestamp\n1,A,2026-01-01T00:00:00Z\n', encoding='utf-8')
        model.write_text(scheduler.support.pnml(['source', 'sink'], [('a', 'A')],
            [('source', 'a', 1), ('a', 'sink', 1)], {'source': 1}, {'sink': 1}), encoding='utf-8')
        native = {}
        if args.exporter:
            for row in rows:
                for mode in ['direct', 'map']:
                    def cli():
                        command = [str(args.exporter.resolve()), str(log), '--check-pnml', str(model), '--playout-miner', 'imported']
                        for key, value in parameters(row, sampling, mode).items():
                            command.extend(['--' + key.replace('_', '-'), value])
                        command.extend(['--playout', str(output)])
                        result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', timeout=60)
                        assert result.returncode == 0, result.stdout + result.stderr
                        values = durations(output.read_text(encoding='utf-8'), row, sampling)
                        native[row['spec']['name'], 'cli', mode] = values
                        return validate(values, row, sampling, public_compare=True)
                    check(row['spec']['name'], 'cli-' + mode, cli)
        if args.server:
            with socket.socket() as listener:
                listener.bind(('127.0.0.1', 0))
                port = listener.getsockname()[1]
            def request(path, data=None, content_type='application/xml'):
                req = urllib.request.Request(f'http://127.0.0.1:{port}' + path, data=data, headers={'Content-Type': content_type})
                with urllib.request.urlopen(req, timeout=60) as response:
                    return response.read().decode()
            with (directory / 'server.log').open('w', encoding='utf-8') as server_log:
                server = subprocess.Popen([str(args.server.resolve()), '--port', str(port)], stdout=server_log,
                    stderr=server_log, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
                try:
                    for _ in range(200):
                        try:
                            request('/api/version')
                            break
                        except OSError:
                            assert server.poll() is None, 'Server exited before readiness'
                            time.sleep(.05)
                    else:
                        raise RuntimeError('Server readiness timed out')
                    request('/api/mine', log.read_bytes(), 'text/csv')
                    request('/api/check-model', model.read_bytes())
                    for row in rows:
                        for mode in ['direct', 'map']:
                            def http():
                                query = urllib.parse.urlencode(dict(parameters(row, sampling, mode), miner='imported', mode='monte_carlo'))
                                values = durations(request('/api/playout?' + query, b''), row, sampling)
                                native[row['spec']['name'], 'http', mode] = values
                                return validate(values, row, sampling, public_compare=True)
                            check(row['spec']['name'], 'http-' + mode, http)
                finally:
                    server.terminate()
                    try:
                        server.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        server.kill()
                        server.wait(timeout=10)
        for row in rows:
            name = row['spec']['name']
            available = [(route, mode) for route in ['cli', 'http'] for mode in ['direct', 'map']
                         if (name, route, mode) in native]
            if len(available) > 1:
                def equivalent():
                    baseline = native[name, *available[0]]
                    assert all(native[name, *key] == baseline for key in available), 'Same native seed changed across transport/map modes'
                check(name, 'native-seed-equivalence', equivalent)
    args.output.write_text(json.dumps(dict(sampling=sampling, checks=checks), indent=2) + '\n', encoding='utf-8')
    passed = sum(x['passed'] for x in checks)
    print(f'{passed}/{len(checks)} duration-law checks passed')
    return int(passed != len(checks))


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except Exception as error:
        print(json.dumps(dict(error=type(error).__name__, message=str(error))))
        raise SystemExit(1)
