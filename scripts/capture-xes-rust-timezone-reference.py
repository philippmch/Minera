"""Capture public RUSTXES date offsets and supported date range."""
import argparse
import datetime
import gzip
import hashlib
import json
import os
import subprocess
import tempfile
from importlib.metadata import version
from pathlib import Path
import warnings


REFERENCE_VERSION = '2.7.22'
BACKEND_VERSION = '0.6.2'
REFERENCE_COMMIT = '0715c31f3f1d85924107d6aa5329806db69df899'
REFERENCE_COMMIT_SCOPE = 'configured_reference_pin'
pm4py = importer = None


def _load_public_reference_api():
    global pm4py, importer
    warnings.filterwarnings('ignore')
    import pm4py as pm4py_module
    from pm4py.objects.log.importer.xes import importer as importer_module
    pm4py, importer = pm4py_module, importer_module
    return pm4py, importer


def _require_version(pm4py_module):
    actual = getattr(pm4py_module, '__version__', None)
    if actual != REFERENCE_VERSION:
        raise RuntimeError(f'This oracle requires pm4py=={REFERENCE_VERSION}; got {actual!r}')


def _require_backend(actual):
    if actual != BACKEND_VERSION:
        raise RuntimeError(f'This oracle requires r4pm=={BACKEND_VERSION}; got {actual!r}')


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
        raise RuntimeError(f'Unable to hash {resolved}: {error}') from error
    return {'path': str(resolved), 'sha256': digest.hexdigest()}


def _module_metadata(module):
    filename = getattr(module, '__file__', None)
    return _file_metadata(filename) if filename else None


def _native_metadata(path):
    try:
        return _file_metadata(path)
    except (OSError, RuntimeError) as error:
        return {'path': str(Path(path).resolve()), 'sha256': None, 'error': str(error)}


def _native_integrity(before, after):
    metadata = {
        'path': before.get('path', after.get('path')),
        'sha256_before': before.get('sha256'),
        'sha256_after': after.get('sha256'),
    }
    errors = []
    if after.get('sha256') is not None:
        metadata['sha256'] = after['sha256']
    if before.get('error'):
        metadata['error'] = before['error']
        metadata['error_before'] = before['error']
        errors.append({'kind': 'native_hash_error', 'phase': 'before',
                       'error': before['error']})
    if after.get('error'):
        metadata['error'] = after['error']
        metadata['error_after'] = after['error']
        errors.append({'kind': 'native_hash_error', 'phase': 'after',
                       'error': after['error']})
    if (before.get('sha256') is not None and after.get('sha256') is not None
            and before['sha256'] != after['sha256']):
        metadata['changed'] = True
        errors.append({'kind': 'native_executable_changed',
                       'sha256_before': before['sha256'],
                       'sha256_after': after['sha256']})
    return metadata, errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--native', type=Path)
    args = parser.parse_args()
    pm4py, importer = _load_public_reference_api()
    _require_version(pm4py)
    backend = version('r4pm')
    _require_backend(backend)
    native_before = _native_metadata(args.native) if args.native else None
    dates = ['0001-01-01T00:00:00Z', '1600-01-01T00:00:00Z', '1678-01-01T00:00:00Z', '1800-01-01T00:00:00Z', '1893-03-31T23:00:00Z', '1893-04-01T00:00:00Z', '1900-01-01T00:00:00Z', '1916-06-01T00:00:00Z', '1945-05-24T00:00:00Z', '1970-01-01T00:00:00Z', '2024-01-01T12:00:00Z', '2024-07-01T12:00:00Z', '2024-03-31T00:59:59.123456789Z', '2024-03-31T01:00:00Z', '2024-10-27T00:59:59Z', '2024-10-27T01:00:00Z', '2038-07-01T12:00:00Z', '2100-07-01T12:00:00Z', '2263-01-01T00:00:00Z', '9999-01-01T00:00:00Z']
    dates += ['2024-01-01T12:00:00+05:30', '2024-07-01T12:00:00-07:00', '2024-01-01T12:00:00', '2024-01-01T12:00:00+14:00', '2024-01-01T12:00:00.123456789-12:00']
    dates += ['2016-12-31T23:59:60Z', '2024-01-01T23:59:60.123456Z']
    dates += ['2024-01-01T12:34:60Z', '2024-01-01T12:59:60.999999999+05:30', '2024-01-01T23:59:60-12:00', '2024-01-01T23:59:61Z']
    rows = []
    with tempfile.TemporaryDirectory(prefix='minera-rust-zone-') as directory:
        for date in dates:
            for compressed in (False, True):
                path = Path(directory) / ('input.xes.gz' if compressed else 'input.xes')
                xml = f'<log><trace><string key="concept:name" value="c"/><date key="trace_date" value="{date}"/><event><string key="concept:name" value="A"/><date key="time:timestamp" value="{date}"/></event></trace></log>'
                path.write_bytes(gzip.compress(xml.encode()) if compressed else xml.encode())
                try:
                    log = importer.apply(str(path), variant=importer.Variants.RUSTXES, parameters={'show_progress_bar': False})
                    trace_value, event_value = log[0].attributes.get('trace_date'), log[0][0].get('time:timestamp')
                    result = dict(trace=trace_value.isoformat() if trace_value is not None else None, event=event_value.isoformat() if event_value is not None else None, type=type(event_value).__name__, timezone=str(event_value.tzinfo) if event_value is not None else None, timezone_type=type(event_value.tzinfo).__module__ if event_value is not None else None)
                except BaseException as error:
                    result = {'error': type(error).__name__, 'message': str(error)}
                row = dict(date=date, compressed=compressed, xml=xml, result=result)
                if args.native:
                    try:
                        native = subprocess.run([str(args.native.resolve()), str(path), '--xes-import-variant', 'rustxes', '--log-convert', 'to_event_log'], capture_output=True, text=True, encoding='utf-8')
                    except (OSError, subprocess.TimeoutExpired) as error:
                        row['comparison'] = {'native_process_error': type(error).__name__, 'message': str(error)}
                        native = None
                    if native is None:
                        pass
                    elif 'error' in result:
                        row['comparison'] = {'uncompared_reference_error': result, 'native_status': native.returncode}
                    elif native.returncode:
                        row['comparison'] = {'native_error': native.stderr,
                                             'stdout': native.stdout,
                                             'returncode': native.returncode}
                    else:
                        try:
                            trace = json.loads(native.stdout)['log']['traces'][0]
                            native_trace = next((a['value'] for a in trace['attributes'] if a['key'] == 'trace_date'), None)
                            native_event = trace['events'][0].get('timestamp_attribute', {}).get('value')
                            actual = dict(trace=native_trace, event=native_event)
                            def same_utc(left, right):
                                if left is None or right is None:
                                    return left == right
                                left, right = (datetime.datetime.fromisoformat(v.replace('Z', '+00:00')) for v in (left, right))
                                return left == right and left.utcoffset() == right.utcoffset() == datetime.timedelta(0)
                            row['comparison'] = 'match' if all(same_utc(actual[key], result.get(key)) for key in actual) else {'actual': actual}
                        except (KeyError, IndexError, TypeError, ValueError, json.JSONDecodeError) as error:
                            row['comparison'] = {'native_malformed_output': type(error).__name__, 'message': str(error),
                                                 'stdout': native.stdout, 'stderr': native.stderr}
                rows.append(row)
    native_after = _native_metadata(args.native) if args.native else None
    native_metadata = None
    integrity_errors = []
    if args.native:
        native_metadata, integrity_errors = _native_integrity(native_before, native_after)
    comparison_errors = ([{'date': row.get('date'), 'compressed': row.get('compressed'),
                           'comparison': row.get('comparison')}
                          for row in rows if row.get('comparison') != 'match']
                         if args.native else [])
    comparison_errors.extend(integrity_errors)
    artifact = dict(reference=pm4py.__version__, reference_commit=REFERENCE_COMMIT,
                    reference_commit_scope=REFERENCE_COMMIT_SCOPE,
                    environment=_environment(), reference_module=_module_metadata(importer),
                    r4pm=backend,
                    native_executable=native_metadata,
                    rows=rows,
                    errors=comparison_errors)
    args.output.write_text(json.dumps(artifact, indent=2) + '\n', encoding='utf-8')
    print(f'Captured {len(rows)} timezone observations')
    if args.native:
        matches = sum(row['comparison'] == 'match' for row in rows)
        print(f'{matches}/{len(rows)} UTC date comparisons matched')
        if artifact['errors']:
            return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
