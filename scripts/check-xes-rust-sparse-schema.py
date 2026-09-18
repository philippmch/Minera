"""Validate sparse RustXES trace reconstruction without building a dense table."""
import argparse
import hashlib
import json
import os
import subprocess
import tempfile
import time
from pathlib import Path


REFERENCE_VERSION = '2.7.22'
REFERENCE_COMMIT = '0715c31f3f1d85924107d6aa5329806db69df899'
REFERENCE_COMMIT_SCOPE = 'configured_reference_pin'


def _environment():
    return {'PYTHONHASHSEED': os.environ.get('PYTHONHASHSEED'), 'TZ': os.environ.get('TZ')}


def _file_metadata(path):
    resolved = Path(path).resolve()
    digest = hashlib.sha256()
    try:
        with resolved.open('rb') as source:
            for chunk in iter(lambda: source.read(1024 * 1024), b''):
                digest.update(chunk)
    except OSError as error:
        return {'path': str(resolved), 'sha256': None, 'error': str(error)}
    return {'path': str(resolved), 'sha256': digest.hexdigest()}


def _write_report(path, report):
    if path is not None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


def _finish_report(path, report, executable, before):
    after = _file_metadata(executable)
    record = {
        'path': before.get('path', after.get('path')),
        'sha256_before': before.get('sha256'),
        'sha256_after': after.get('sha256'),
    }
    if after.get('sha256') is not None:
        record['sha256'] = after['sha256']
    if before.get('error'):
        record['error'] = before['error']
        record['error_before'] = before['error']
        report['errors'].append({'kind': 'native_hash_error', 'phase': 'before',
                                 'error': before['error']})
    if after.get('error'):
        record['error'] = after['error']
        record['error_after'] = after['error']
        report['errors'].append({'kind': 'native_hash_error', 'phase': 'after',
                                 'error': after['error']})
    if (before.get('sha256') is not None and after.get('sha256') is not None
            and before['sha256'] != after['sha256']):
        record['changed'] = True
        report['errors'].append({
            'kind': 'native_executable_changed',
            'sha256_before': before['sha256'],
            'sha256_after': after['sha256'],
        })
    report['native_executable'] = record
    _write_report(path, report)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--native', type=Path, required=True)
    parser.add_argument('--traces', type=int, default=3000)
    parser.add_argument('--output', type=Path, default=Path('build/xes-rust-sparse-schema-report.json'))
    args = parser.parse_args()
    native_before = _file_metadata(args.native)
    report = {
        'reference': REFERENCE_VERSION,
        'reference_commit': REFERENCE_COMMIT,
        'reference_commit_scope': REFERENCE_COMMIT_SCOPE,
        'environment': _environment(),
        'native_executable': None,
        'traces_requested': args.traces,
        'errors': [],
    }
    if args.traces <= 0:
        report['errors'].append({'kind': 'invalid_trace_count', 'traces': args.traces})
        _finish_report(args.output, report, args.native, native_before)
        print('traces must be positive')
        return 1
    with tempfile.TemporaryDirectory(prefix='minera-rust-sparse-') as directory:
        path = Path(directory) / 'input.xes'
        path.write_text('<log>' + ''.join(f'<trace><string key="concept:name" value="c{i}"/><string key="p{i}" value="v{i}"/><event><string key="concept:name" value="A"/></event></trace>' for i in range(args.traces)) + '</log>', encoding='utf-8')
        started = time.perf_counter()
        try:
            native = subprocess.run([str(args.native.resolve()), str(path), '--xes-import-variant', 'rustxes', '--log-convert', 'to_event_log'], capture_output=True, text=True, encoding='utf-8')
        except (OSError, subprocess.TimeoutExpired) as error:
            report['errors'].append({'kind': 'native_process_error', 'error': type(error).__name__, 'message': str(error)})
            _finish_report(args.output, report, args.native, native_before)
            print(f'Native process error: {error}')
            return 1
        elapsed = time.perf_counter() - started
        report['elapsed_seconds'] = elapsed
        report['native_returncode'] = native.returncode
        if native.returncode:
            report['errors'].append({'kind': 'native_nonzero_exit', 'returncode': native.returncode,
                                     'stdout': native.stdout, 'stderr': native.stderr})
            _finish_report(args.output, report, args.native, native_before)
            print(f'Native process exited {native.returncode}: {native.stderr}')
            return 1
        try:
            traces = json.loads(native.stdout)['log']['traces']
            if len(traces) != args.traces:
                raise ValueError(f'Expected {args.traces} traces, got {len(traces)}')
            for i, trace in enumerate(traces):
                if trace.get('case_id') != f'c{i}':
                    raise ValueError(f'Trace {i} has case_id {trace.get("case_id")!r}')
                if {a['key']: a['value'] for a in trace.get('attributes', [])} != {f'p{i}': f'v{i}'}:
                    raise ValueError(f'Trace {i} has unexpected sparse attributes')
                if len(trace.get('events', [])) != 1 or trace['events'][0].get('activity') != 'A':
                    raise ValueError(f'Trace {i} has unexpected event shape')
        except (KeyError, IndexError, TypeError, ValueError, json.JSONDecodeError) as error:
            report['errors'].append({'kind': 'malformed_native_output', 'error': type(error).__name__,
                                     'message': str(error), 'stdout': native.stdout,
                                     'stderr': native.stderr})
            _finish_report(args.output, report, args.native, native_before)
            print(f'Malformed native output: {error}')
            return 1
        report['traces_validated'] = len(traces)
        _finish_report(args.output, report, args.native, native_before)
        print(f'Validated {len(traces)} traces with distinct sparse columns in {elapsed:.3f}s')
    return 1 if report['errors'] else 0


if __name__ == '__main__':
    raise SystemExit(main())
