"""Capture public RUSTXES scalar and missing-column behavior without source reads."""
import argparse
import datetime
import gzip
import hashlib
import itertools
import json
import math
import os
import subprocess
import tempfile
from importlib.metadata import version
from pathlib import Path
from xml.sax.saxutils import quoteattr
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


def normalize(value):
    if isinstance(value, dict):
        return {str(k): normalize(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [normalize(v) for v in value]
    if isinstance(value, datetime.datetime):
        return {'date': value.isoformat()}
    if isinstance(value, float) and not math.isfinite(value):
        return {'float': 'nan' if math.isnan(value) else 'inf' if value > 0 else '-inf'}
    return value


def comparable(value):
    if isinstance(value, dict):
        if set(value) == {'date'}:
            return {'date_instant': datetime.datetime.fromisoformat(value['date'].replace('Z', '+00:00')).astimezone(datetime.timezone.utc).isoformat()}
        return {key: comparable(item) for key, item in value.items()}
    if isinstance(value, list):
        return [comparable(item) for item in value]
    if isinstance(value, bool) or value is None or isinstance(value, str):
        return value
    return {'type': type(value).__name__, 'value': value}


def native_attribute(attribute):
    if attribute.get('null'):
        return None
    kind, value = attribute['kind'], attribute['value']
    if kind == 'date':
        return {'date': value}
    if kind == 'int':
        return int(attribute.get('typed_value', value))
    if kind == 'float':
        return normalize(float(attribute.get('typed_value') if attribute.get('typed_value') is not None else value))
    if kind == 'boolean':
        return attribute.get('typed_value', value == 'true')
    return value


def native_traces(log):
    result = []
    for trace in log['traces']:
        attributes = {a['key']: native_attribute(a) for a in trace['attributes']}
        if trace.get('case_id_attribute'):
            a = trace['case_id_attribute']
            attributes[a['key']] = native_attribute(a)
            if trace['case_id_source_key'] != a['key'] or trace['case_id'] != a['value']:
                raise RuntimeError('Stale case selector cache')
        events = []
        for event in trace['events']:
            if event['case_id'] != trace['case_id'] or event['case_id_source_key'] != trace['case_id_source_key']:
                raise RuntimeError('Stale event case context')
            row = {a['key']: native_attribute(a) for a in event['attributes']}
            for key in ('activity_attribute', 'timestamp_attribute', 'resource_attribute', 'lifecycle_attribute'):
                if event.get(key):
                    a = event[key]
                    row[a['key']] = native_attribute(a)
                    cache = key.removesuffix('_attribute')
                    if a['kind'] == 'string' and cache != 'timestamp' and not a.get('null'):
                        if event[cache] != a['value']:
                            raise RuntimeError(f'Stale {cache} cache')
            events.append(row)
        result.append(dict(attributes=attributes, events=events))
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--native', type=Path, help='Compare scalar values/types; dates compare instants, not offset spelling')
    args = parser.parse_args()
    pm4py, importer = _load_public_reference_api()
    _require_version(pm4py)
    backend = version('r4pm')
    _require_backend(backend)
    native_before = _native_metadata(args.native) if args.native else None
    values = {
        'string': [None, '', 'text'],
        'int': [None, '', '42', 'bad', '9223372036854775808', ' 42 ', '+42'],
        'float': [None, '', '1.5', 'bad', 'INF', 'NaN', '1e999', ' 1.5 ', '+1e999', '0.' + '0' * 400 + '1', '9' * 400 + 'e-1'],
        'boolean': [None, '', 'true', 'TRUE', '1', 'bad'],
        'date': [None, '', 'bad', '2024-01-01T12:00:00Z', '2024-01-01T12:00:00+05:30', '2024-01-01T12:00:00', '2024-01-01', '2024-01-01T12:00:00.123456789Z'],
        'id': [None, '', 'bad', 'ABCDEFAB-1234-5678-ABCD-ABCDEF123456', 'abcdefab12345678abcdabcdef123456'],
        'unknown': [None, '', 'text'],
    }
    rows = []
    with tempfile.TemporaryDirectory(prefix='minera-rust-scalar-') as directory:
        for scope in ('trace', 'event'):
            for kind, cases in values.items():
                for value in cases:
                    attribute = f'<{kind} key="probe"' + ('' if value is None else ' value=' + quoteattr(value)) + '/>'
                    for extra_row in (False, True):
                        xml = '<log><trace><string key="concept:name" value="c"/>' + (attribute if scope == 'trace' else '') + '<event><string key="concept:name" value="A"/>' + (attribute if scope == 'event' else '') + '</event>' + ('<event><string key="concept:name" value="B"/></event>' if extra_row and scope == 'event' else '') + '</trace>' + ('<trace><string key="concept:name" value="d"/><event><string key="concept:name" value="B"/></event></trace>' if extra_row and scope == 'trace' else '') + '</log>'
                        for compressed in (False, True):
                            path = Path(directory) / ('input.xes.gz' if compressed else 'input.xes')
                            path.write_bytes(gzip.compress(xml.encode()) if compressed else xml.encode())
                            try:
                                log = importer.apply(str(path), variant=importer.Variants.RUSTXES, parameters={'show_progress_bar': False})
                                result = [dict(attributes=normalize(trace.attributes), events=[normalize(dict(event)) for event in trace]) for trace in log]
                            except BaseException as error:
                                result = {'error': type(error).__name__, 'message': str(error)}
                            rows.append(dict(scope=scope, kind=kind, value=value, extra_row=extra_row, compressed=compressed, xml=xml, result=result))
        lexical = {'int': '42', 'float': '1.5', 'boolean': 'true', 'string': 'text', 'date': '2024-01-01T12:34:56Z', 'id': 'abcdefab12345678abcdabcdef123456', 'nan': 'NaN', 'missing': None, 'escaped': '"q"\\é\nline', 'date_fraction': '2024-01-01T12:00:00.123456789+05:30'}
        for first, second in itertools.product(lexical, repeat=2):
            def attribute(kind):
                tag = 'float' if kind == 'nan' else 'string' if kind in ('missing', 'escaped') else 'date' if kind == 'date_fraction' else kind
                return f'<{tag} key="probe"' + ('' if lexical[kind] is None else ' value=' + quoteattr(lexical[kind])) + '/>'
            xml = '<log><trace><string key="concept:name" value="c"/>' + ''.join('<event><string key="concept:name" value="A"/>' + attribute(kind) + '</event>' for kind in (first, second)) + '</trace></log>'
            path = Path(directory) / 'input.xes'
            path.write_text(xml, encoding='utf-8')
            try:
                log = importer.apply(str(path), variant=importer.Variants.RUSTXES, parameters={'show_progress_bar': False})
                result = [dict(attributes=normalize(trace.attributes), events=[normalize(dict(event)) for event in trace]) for trace in log]
            except BaseException as error:
                result = {'error': type(error).__name__, 'message': str(error)}
            rows.append(dict(scope='mixed', kind=first + '+' + second, value=None, extra_row=True, compressed=False, xml=xml, result=result))
        specials = {
            'null_marker': '<event><string key="concept:name" value="A"/><int key="probe" value="42" null="true"/></event>',
            'missing_then_duplicate': '<event><string key="concept:name" value="A"/><int key="probe"/><int key="probe" value="42"/></event>',
            'nan_then_duplicate': '<event><string key="concept:name" value="A"/><float key="probe" value="NaN"/><float key="probe" value="1.5"/></event>',
            'selected_activity': '<event><int key="concept:name" value="42"/></event><event><string key="concept:name" value="A"/></event>',
            'selected_resource': '<event><string key="concept:name" value="A"/><int key="org:resource" value="42"/></event><event><string key="concept:name" value="A"/><string key="org:resource" value="r"/></event>',
            'overlapping_selectors': '<event><string key="concept:name" value="A"/><int key="probe" value="42"/></event><event><string key="concept:name" value="B"/></event>',
        }
        float_display_values = ['1.0', '1.2345678901234567', '1e20', '1e-10', '-0.0', '1e-4', '1e-5', '1234567.89123', '999999.9', '999999.9999999', '1000000.0', '0.0000009999999', '0.00000123456789', '12.123456789', '0.123456789', '1.0000001', '-1.23456789', '1.234565e20']
        float_display_values += [f'1e{exponent}' for exponent in range(-12, 10)]
        float_display_values += ['1234567', '12345678', '123456789', '1234567890', '-12345678', '-123456789', '123456.78', '123456.789', '-1.0000001', '-0.0000001', '1.2345678', '1.23456789', '12.345678', '12.3456789', '0.00000012', '0.00000123', '999999.9999994', '999999.9999995']
        float_display_values += ['100000.123456789', '99999.123456789', '999999.123456789', '999999.4999999', '999999.5000001', '999998.9999999', '999999.00001', '999995.123456']
        for value in dict.fromkeys(float_display_values):
            specials['mixed_float_' + value] = '<event><string key="concept:name" value="A"/><float key="probe" value="' + value + '"/></event><event><string key="concept:name" value="A"/><string key="probe" value="s"/></event>'
        for name, events in specials.items():
            xml = '<log><trace><string key="concept:name" value="c"/>' + events + '</trace></log>'
            path = Path(directory) / 'input.xes'
            path.write_text(xml, encoding='utf-8')
            try:
                log = importer.apply(str(path), variant=importer.Variants.RUSTXES, parameters={'show_progress_bar': False})
                result = [dict(attributes=normalize(trace.attributes), events=[normalize(dict(event)) for event in trace]) for trace in log]
            except BaseException as error:
                result = {'error': type(error).__name__, 'message': str(error)}
            native_options = ['--xes-activity-attribute', 'probe', '--xes-resource-attribute', 'probe'] if name == 'overlapping_selectors' else []
            rows.append(dict(scope='special', kind=name, value=None, extra_row=False, compressed=False, xml=xml, result=result, native_options=native_options))
        if args.native:
            for row in rows:
                path = Path(directory) / ('input.xes.gz' if row['compressed'] else 'input.xes')
                path.write_bytes(gzip.compress(row['xml'].encode()) if row['compressed'] else row['xml'].encode())
                try:
                    native = subprocess.run([str(args.native.resolve()), str(path), '--xes-import-variant', 'rustxes', '--log-convert', 'to_event_log'] + row.get('native_options', []), capture_output=True, text=True, encoding='utf-8')
                except (OSError, subprocess.TimeoutExpired) as error:
                    row['comparison'] = {'native_process_error': type(error).__name__, 'message': str(error)}
                    native = None
                if native is not None and native.returncode:
                    row['comparison'] = {'native_error': native.stderr, 'stdout': native.stdout,
                                         'returncode': native.returncode}
                elif native is not None:
                    try:
                        actual = native_traces(json.loads(native.stdout)['log'])
                        row['comparison'] = 'match' if comparable(actual) == comparable(row['result']) else {'actual': actual}
                    except (KeyError, IndexError, TypeError, ValueError, RuntimeError, json.JSONDecodeError) as error:
                        row['comparison'] = {'native_malformed_output': type(error).__name__,
                                             'message': str(error), 'stdout': native.stdout,
                                             'stderr': native.stderr}
    native_after = _native_metadata(args.native) if args.native else None
    native_metadata = None
    integrity_errors = []
    if args.native:
        native_metadata, integrity_errors = _native_integrity(native_before, native_after)
    comparison_errors = ([{'scope': row.get('scope'), 'kind': row.get('kind'),
                           'comparison': row.get('comparison')}
                          for row in rows if row.get('comparison') != 'match']
                         if args.native else [])
    comparison_errors.extend(integrity_errors)
    artifact = dict(reference=pm4py.__version__, reference_commit=REFERENCE_COMMIT,
                    reference_commit_scope=REFERENCE_COMMIT_SCOPE,
                    environment=_environment(), reference_module=_module_metadata(importer),
                    r4pm=backend, native_executable=native_metadata,
                    date_comparison='absolute instant only', rows=rows,
                    errors=comparison_errors)
    args.output.write_text(json.dumps(artifact, indent=2) + '\n', encoding='utf-8')
    print(f'Captured {len(rows)} scalar observations')
    if args.native:
        matches = sum(row['comparison'] == 'match' for row in rows)
        print(f'{matches}/{len(rows)} native scalar comparisons matched')
        if artifact['errors']:
            return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
