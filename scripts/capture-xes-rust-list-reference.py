"""Black-box RUSTXES list and container value captures; no implementation reads."""
import argparse
import gzip
import hashlib
import json
import math
import os
import random
import struct
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
    random_bits = random.Random(271822)
    floats = [struct.unpack('!d', random_bits.getrandbits(64).to_bytes(8, 'big'))[0] for _ in range(512)]
    shapes = {
        'empty': '',
        'string': '<string key="child" value="text"/>',
        'escaped': '<string key="a&amp;&quot;" value="&quot;quoted&quot;\\slash&#10;line&#9;tab&#13;return 😀"/>',
        'types': '<int key="i" value="-42"/><float key="f" value="1.0"/><boolean key="b" value="true"/><date key="d" value="2024-01-01T12:34:56.123456+05:30"/><id key="id" value="12345678-1234-5678-1234-567812345678"/>',
        'float_edges': '<float key="f" value="-0.0"/><float key="small" value="1e-10"/><float key="big" value="1e20"/><float key="inf" value="INF"/><float key="nan" value="NaN"/>',
        'duplicate': '<string key="child" value="first"/><string key="child" value="second"/>',
        'scalar_children': '<string key="child" value="parent"><int key="nested" value="2"/></string>',
        'scalar_empty_values': '<string key="child" value="parent"><values/></string>',
        'list': '<list key="nested"><values><string key="leaf" value="v"/></values></list>',
        'container': '<container key="nested"><string key="leaf" value="v"/></container>',
        'mixed_values': '<string key="before" value="a"/><values><string key="inside" value="b"/></values><string key="after" value="c"/>',
        'two_values': '<values><string key="first" value="a"/></values><values><string key="second" value="b"/></values>',
        'unknown': '<unknown key="unknown" value="x"/><string key="good" value="y"/>',
        'unicode': '<string key="unicode" value="é 中 😀 &#160; &#8203; &#8232; &#769;"/>',
        'float_thresholds': ''.join(f'<float key="f{i}" value="{v}"/>' for i, v in enumerate(('1e-4', '1e-5', '1e-6', '1e15', '1e16', '1e17', '1.2345678901234567', '5e-324', '1.7976931348623157e308'))),
        'date_precision': ''.join(f'<date key="d{i}" value="{v}"/>' for i, v in enumerate(('2024-01-01T00:00:00Z', '2024-01-01T00:00:00.1Z', '2024-01-01T00:00:00.1234Z', '2024-01-01T00:00:00.123456789-03:30'))),
        'empty_scalar_children': '<string key="s" value="v"></string>',
        'null_marker': '<string key="s" value="v" null="true"/>',
        'ids': '<id key="upper" value="ABCDEFAB-1234-5678-ABCD-ABCDEF123456"/><id key="compact" value="abcdefab12345678abcdabcdef123456"/>',
        'missing_values': '<string key="s"/><int key="i"/><float key="f"/><boolean key="b"/><date key="d"/><id key="id"/>',
        'invalid_values': '<int key="i" value="bad"/><float key="f" value="bad"/><date key="d" value="bad"/><id key="id" value="bad"/>',
        'sampled_floats': ''.join(f'<float key="f{i}" value="{v!r}"/>' for i, v in enumerate(floats) if math.isfinite(v)),
        'uuid_forms': ''.join(f'<id key="id{i}" value="{v}"/>' for i, v in enumerate(('{abcdefab-1234-5678-abcd-abcdef123456}', 'urn:uuid:abcdefab-1234-5678-abcd-abcdef123456', 'abcdefab-12345678-abcd-abcdef123456', 'zzzzzzzz12345678abcdabcdef123456', '00000000-0000-0000-0000-000000000000'))),
        'date_forms': ''.join(f'<date key="d{i}" value="{v}"/>' for i, v in enumerate(('2024-01-01', '2024-01-01T00:00:00', '2024-01-01T00:00:00+0530', '2024-01-01t00:00:00z', '2024-01-01 00:00:00Z', '2024-01-01T23:59:60Z'))),
    }
    rows = []
    with tempfile.TemporaryDirectory(prefix='minera-rust-list-') as directory:
        for scope in ('trace', 'event'):
            for container in ('list', 'container'):
                for wrapped in (False, True):
                    for shape, children in shapes.items():
                        attribute = f'<{container} key="probe">' + ('<values>' if wrapped else '') + children + ('</values>' if wrapped else '') + f'</{container}>'
                        xml = '<log><trace><string key="concept:name" value="c"/>' + (attribute if scope == 'trace' else '') + '<event><string key="concept:name" value="A"/>' + (attribute if scope == 'event' else '') + '</event></trace></log>'
                        for compressed in (False, True):
                            path = Path(directory) / ('input.xes.gz' if compressed else 'input.xes')
                            path.write_bytes(gzip.compress(xml.encode()) if compressed else xml.encode())
                            try:
                                log = importer.apply(str(path), variant=importer.Variants.RUSTXES, parameters={'show_progress_bar': False})
                                value = (log[0].attributes if scope == 'trace' else log[0][0]).get('probe')
                                result = {'value': value, 'type': type(value).__name__}
                            except BaseException as error:
                                result = {'error': type(error).__name__, 'message': str(error)}
                            row = dict(scope=scope, container=container, wrapped=wrapped, shape=shape, compressed=compressed, xml=xml, result=result)
                            if args.native:
                                try:
                                    native = subprocess.run([str(args.native.resolve()), str(path), '--xes-import-variant', 'rustxes', '--log-convert', 'to_event_log'], capture_output=True, text=True, encoding='utf-8')
                                except (OSError, subprocess.TimeoutExpired) as error:
                                    row['comparison'] = {'native_process_error': type(error).__name__, 'message': str(error)}
                                    rows.append(row)
                                    continue
                                if native.returncode:
                                    row['comparison'] = {'native_error': native.stderr,
                                                         'stdout': native.stdout,
                                                         'returncode': native.returncode}
                                else:
                                    try:
                                        trace = json.loads(native.stdout)['log']['traces'][0]
                                        attributes = trace['attributes'] if scope == 'trace' else trace['events'][0]['attributes']
                                        actual = next((a for a in attributes if a['key'] == 'probe'), None)
                                        row['comparison'] = 'match' if actual and result == {'value': actual['value'], 'type': 'str'} and actual['kind'] == 'string' and actual['container'] == 'scalar' else {'actual': actual, 'stdout': native.stdout, 'stderr': native.stderr}
                                    except (KeyError, IndexError, TypeError, ValueError, json.JSONDecodeError) as error:
                                        row['comparison'] = {'native_malformed_output': type(error).__name__, 'message': str(error), 'stdout': native.stdout, 'stderr': native.stderr}
                            rows.append(row)
    native_after = _native_metadata(args.native) if args.native else None
    native_metadata = None
    integrity_errors = []
    if args.native:
        native_metadata, integrity_errors = _native_integrity(native_before, native_after)
    comparison_errors = ([{'scope': row.get('scope'), 'container': row.get('container'),
                           'wrapped': row.get('wrapped'), 'shape': row.get('shape'),
                           'comparison': row.get('comparison')}
                          for row in rows if row.get('comparison') != 'match']
                         if args.native else [])
    comparison_errors.extend(integrity_errors)
    artifact = dict(reference=pm4py.__version__, reference_commit=REFERENCE_COMMIT,
                    reference_commit_scope=REFERENCE_COMMIT_SCOPE,
                    environment=_environment(), reference_module=_module_metadata(importer),
                    r4pm=backend, native_executable=native_metadata,
                    rows=rows,
                    errors=comparison_errors)
    args.output.write_text(json.dumps(artifact, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print(f'Captured {len(rows)} observations')
    if args.native:
        matches = sum(row['comparison'] == 'match' for row in rows)
        print(f'{matches}/{len(rows)} native value matches')
        if artifact['errors']:
            return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
