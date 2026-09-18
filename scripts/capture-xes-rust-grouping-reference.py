"""Public RUSTXES trace reconstruction and source-column collision probes."""
import argparse
import gzip
import hashlib
import json
import os
import re
import runpy
import subprocess
import tempfile
from importlib.metadata import version
from pathlib import Path
import warnings
from xml.sax.saxutils import quoteattr


REFERENCE_VERSION = "2.7.22"
BACKEND_VERSION = "0.6.2"
REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"
REFERENCE_COMMIT_SCOPE = "configured_reference_pin"
pm4py = importer = None
normalize = comparable = native_traces = None


def _load_public_reference_api():
    global pm4py, importer, normalize, comparable, native_traces
    warnings.filterwarnings("ignore")
    import pm4py as pm4py_module
    from pm4py.objects.log.importer.xes import importer as importer_module
    helpers = runpy.run_path(str(Path(__file__).with_name('capture-xes-rust-scalar-reference.py')))
    pm4py, importer = pm4py_module, importer_module
    normalize, comparable, native_traces = (helpers[name] for name in ('normalize', 'comparable', 'native_traces'))
    return pm4py, importer


def _require_version(pm4py_module):
    actual = getattr(pm4py_module, "__version__", None)
    if actual != REFERENCE_VERSION:
        raise RuntimeError(
            f"This oracle requires pm4py=={REFERENCE_VERSION}; got {actual!r}")


def _require_backend(actual):
    if actual != BACKEND_VERSION:
        raise RuntimeError(f"This oracle requires r4pm=={BACKEND_VERSION}; got {actual!r}")


def _environment():
    return {"PYTHONHASHSEED": os.environ.get("PYTHONHASHSEED"),
            "TZ": os.environ.get("TZ")}


def _file_metadata(path):
    resolved = Path(path).resolve()
    digest = hashlib.sha256()
    try:
        with resolved.open("rb") as source:
            for chunk in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(chunk)
    except OSError as error:
        raise RuntimeError(f"Unable to hash {resolved}: {error}") from error
    return {"path": str(resolved), "sha256": digest.hexdigest()}


def _module_metadata(module):
    filename = getattr(module, "__file__", None)
    return _file_metadata(filename) if filename else None


def _native_metadata(path):
    try:
        return _file_metadata(path)
    except (OSError, RuntimeError) as error:
        return {"path": str(Path(path).resolve()), "sha256": None, "error": str(error)}


def _native_integrity(before, after):
    metadata = {
        "path": before.get("path", after.get("path")),
        "sha256_before": before.get("sha256"),
        "sha256_after": after.get("sha256"),
    }
    errors = []
    if after.get("sha256") is not None:
        metadata["sha256"] = after["sha256"]
    if before.get("error"):
        metadata["error"] = before["error"]
        metadata["error_before"] = before["error"]
        errors.append({"kind": "native_hash_error", "phase": "before",
                       "error": before["error"]})
    if after.get("error"):
        metadata["error"] = after["error"]
        metadata["error_after"] = after["error"]
        errors.append({"kind": "native_hash_error", "phase": "after",
                       "error": after["error"]})
    if (before.get("sha256") is not None and after.get("sha256") is not None
            and before["sha256"] != after["sha256"]):
        metadata["changed"] = True
        errors.append({"kind": "native_executable_changed",
                       "sha256_before": before["sha256"],
                       "sha256_after": after["sha256"]})
    return metadata, errors


def attr(key, value, kind='string'):
    return f'<{kind} key={quoteattr(key)}' + ('' if value is None else ' value=' + quoteattr(str(value))) + '/>'


def event(name='A', attributes=''):
    return '<event>' + attr('concept:name', name) + attributes + '</event>'


def trace(name, events=None, attributes='', kind='string'):
    return '<trace>' + ('' if name is None else attr('concept:name', name, kind)) + attributes + (event() if events is None else events) + '</trace>'


def matching_failure(expected, completed):
    if not isinstance(expected, dict) or not completed.returncode:
        return False
    if expected.get('error') == 'KeyError':
        return 'case:concept:name' in expected['message'] and 'case:concept:name' in completed.stderr
    if expected.get('error') == 'DuplicateError':
        key = re.search(r"name '([^']*)'", expected['message'])
        return bool(key and key[1] in completed.stderr and 'duplicate' in completed.stderr.lower())
    return False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--native', type=Path)
    parser.add_argument('--reference-samples', type=int, default=1)
    parser.add_argument('--native-samples', type=int, default=1)
    args = parser.parse_args()
    pm4py, importer = _load_public_reference_api()
    _require_version(pm4py)
    backend = version('r4pm')
    _require_backend(backend)
    native_before = _native_metadata(args.native) if args.native else None
    cases = {
        'duplicate': trace('c', event('A')) + trace('c', event('B')),
        'interleaved': trace('b', event('A')) + trace('a', event('B')) + trace('b', event('C')),
        'trace_attr_first': trace('c', event('A'), attr('probe', 'first')) + trace('c', event('B'), attr('probe', 'second')),
        'trace_attr_first_missing': trace('c', event('A')) + trace('c', event('B'), attr('probe', 'second')),
        'all_missing_id': trace(None),
        'some_missing_id': trace('c') + trace(None, event('B')),
        'first_missing_id': trace(None) + trace('c', event('B')),
        'empty_id': trace('') + trace('c'),
        'missing_value_id': '<trace>' + attr('concept:name', None) + event() + '</trace>',
        'numeric_ids': trace('1', event('A'), kind='int') + trace('1.0', event('B'), kind='float'),
        'mixed_ids': trace('1', event('A'), kind='int') + trace('1', event('B')),
        'nan_id': trace('NaN', kind='float') + trace('1', kind='float'),
        'event_case_attr': trace('c', event('A', attr('case:probe', 'first')) + event('B', attr('case:probe', 'second'))),
        'event_case_attr_first_missing': trace('c', event('A') + event('B', attr('case:probe', 'second'))),
        'colliding_trace_event': trace('c', event('A', attr('case:probe', 'event')), attr('probe', 'trace')),
        'colliding_trace_event_missing': trace('c', event('A') + event('B', attr('case:probe', 'event')), attr('probe', 'trace')),
        'event_case_id': trace(None, event('A', attr('case:concept:name', 'event-case'))),
        'colliding_case_id': trace('trace-case', event('A', attr('case:concept:name', 'event-case'))),
        'event_case_id_splits': trace(None, event('A', attr('case:concept:name', 'x')) + event('B', attr('case:concept:name', 'y'))),
        'double_prefix': trace('c', event('A', attr('case:case:probe', 'value'))),
        'trace_case_alias': trace(None, attributes=attr('case:concept:name', 'alias')),
        'case_empty_key': trace('c', event('A', attr('case:', 'value'))),
        'empty_log': '',
        'empty_trace': trace('c', ''),
        'empty_missing_id': trace(None, '') + trace('c'),
        'empty_first_attributes': trace('c', '', attr('probe', 'empty')) + trace('c', event('A'), attr('probe', 'live')),
        'alias_alongside_id': trace('c', attributes=attr('case:concept:name', 'alias')),
        'alias_before_id': '<trace>' + attr('case:concept:name', 'alias') + attr('concept:name', 'c') + event() + '</trace>',
        'case_prefix_exact': trace('c', event('A', attr('case', 'plain') + attr('Case:probe', 'capital') + attr('case:probe', 'drop'))),
        'trace_prefixes': trace('c', attributes=attr('case:probe', 'nested') + attr('', 'empty')),
        'date_ids': trace('2024-01-01T12:00:00Z', event('A'), kind='date') + trace('2024-01-01T13:00:00+01:00', event('B'), kind='date'),
        'trace_internal_prefix': trace('c', attributes=attr('showcase:probe', 'internal') + attr('case:case:other', 'repeated')),
        'alias_same_distinct_groups': trace('c', event('A'), attr('case:concept:name', 'alias')) + trace('d', event('B'), attr('case:concept:name', 'alias')),
        'alias_different_same_group': trace('c', event('A'), attr('case:concept:name', 'first')) + trace('c', event('B'), attr('case:concept:name', 'second')),
        'alias_collision': trace('c', attributes=attr('probe', 'first') + attr('case:probe', 'second')),
        'alias_collision_reverse': trace('c', attributes=attr('case:probe', 'first') + attr('probe', 'second')),
        'prefixed_standard_trace': trace('c', attributes=attr('case:org:resource', 'r')),
        'custom_prefixed_selector': trace('c', attributes=attr('case:custom', 'value')),
        'custom_selector_collision': trace('c', attributes=attr('probe', 'first') + attr('case:probe', 'second')),
        'missing_custom_selector': trace('c', attributes=attr('', 'unrelated')),
        'empty_custom_selector': trace('c', attributes=attr('', 'selected')),
        'empty_activity_selector': trace('c', event('A', attr('', 'selected'))),
        'empty_resource_selector': trace('c', event('A', attr('', 'selected'))),
        'empty_lifecycle_selector': trace('c', event('A', attr('', 'selected'))),
        'empty_timestamp_selector': trace('c', event('A', attr('', '2024-01-01T12:00:00Z', 'date'))),
        'empty_event_extra_trace_column': trace('c', event('A', attr('', 'value')), attr('extra', 'trace')),
        'empty_event_case_column': trace('c', event('A', attr('', 'value') + attr('case:extra', 'ignored'))),
        'empty_event_null_column': trace('c', event('A', attr('', 'value') + attr('hidden', None, 'int'))),
        'empty_event_unknown_column': trace('c', event('A', attr('', 'value') + attr('hidden', 'ignored', 'unknown'))),
        'empty_event_empty_trace_column': trace('c', event('A', attr('', 'value'))) + trace('e', '', attr('ghost', 'empty')),
        'empty_trace_conflicting_type': trace('c', event('A'), attr('probe', 'live')) + trace('e', '', attr('probe', '42', 'int')),
        'empty_generated_collision': trace('c', event('A', attr('', 'value') + attr('column_0', 'named'))),
        'missing_event_key': trace('c', event('A', '<string value="value"/>')),
        'missing_trace_key': trace('c', attributes='<string value="value"/>'),
    }
    rows = []
    with tempfile.TemporaryDirectory(prefix='minera-rust-group-') as directory:
        for name, body in cases.items():
            xml = '<log>' + body + '</log>'
            for compressed in (False, True):
                path = Path(directory) / ('input.xes.gz' if compressed else 'input.xes')
                path.write_bytes(gzip.compress(xml.encode()) if compressed else xml.encode())
                support = []
                extensions = None
                for _ in range(max(1, args.reference_samples)):
                    try:
                        log = importer.apply(str(path), variant=importer.Variants.RUSTXES, parameters={'show_progress_bar': False})
                        result = [dict(attributes=normalize(t.attributes), events=[normalize(dict(e)) for e in t]) for t in log]
                        extensions = normalize(log.extensions)
                    except BaseException as error:
                        result = {'error': type(error).__name__, 'message': str(error)}
                    if result not in support:
                        support.append(result)
                result = support[0]
                row = dict(name=name, compressed=compressed, xml=xml, result=result, reference_support=support, extensions=extensions)
                selectors = {'custom_prefixed_selector': 'case:custom', 'custom_selector_collision': 'case:probe', 'missing_custom_selector': 'absent', 'empty_custom_selector': ''}
                native_options = ['--xes-case-attribute', selectors[name]] if name in selectors else []
                for selected in ('activity', 'resource', 'lifecycle', 'timestamp'):
                    if name == f'empty_{selected}_selector':
                        native_options = [f'--xes-{selected}-attribute', '']
                row['native_options'] = native_options
                if args.native:
                    native_support = []
                    native_failures = []
                    row['comparison'] = 'match'
                    for _ in range(max(1, args.native_samples)):
                        try:
                            native = subprocess.run([str(args.native.resolve()), str(path), '--xes-import-variant', 'rustxes', '--log-convert', 'to_event_log'] + native_options, capture_output=True, text=True, encoding='utf-8')
                        except (OSError, subprocess.TimeoutExpired) as error:
                            row['comparison'] = {'native_process_error': type(error).__name__, 'message': str(error)}
                            native_failures.append(str(error))
                            continue
                        if native.returncode:
                            if native.stderr not in native_failures:
                                native_failures.append(native.stderr)
                            expected_error = next((expected for expected in support if matching_failure(expected, native)), None)
                            if expected_error is None:
                                row['comparison'] = {'native_error': native.stderr,
                                                     'stdout': native.stdout,
                                                     'returncode': native.returncode}
                            elif expected_error not in native_support:
                                native_support.append(expected_error)
                            continue
                        try:
                            native_log = json.loads(native.stdout)['log']
                            actual = native_traces(native_log)
                            native_extensions = {e['name']: dict(prefix=e['prefix'], uri=e['uri']) for e in native_log['metadata']['extensions']}
                        except (KeyError, TypeError, ValueError, RuntimeError, json.JSONDecodeError) as error:
                            row['comparison'] = {'native_malformed_output': type(error).__name__, 'message': str(error),
                                                 'stdout': native.stdout, 'stderr': native.stderr}
                            native_failures.append(native.stderr)
                            continue
                        if name == 'missing_custom_selector' and any(t.get('case_id_attribute') is not None for t in native_log['traces']):
                            row['comparison'] = {'fabricated_case_projection': True}
                        if name == 'empty_custom_selector' and any(t.get('case_id_attribute', {}).get('key') != '' for t in native_log['traces']):
                            row['comparison'] = {'missing_empty_key_projection': True}
                        if native_extensions != extensions:
                            row['comparison'] = {'native_extensions': native_extensions}
                        if actual not in native_support:
                            native_support.append(actual)
                        if not any(comparable(actual) == comparable(expected) for expected in support):
                            row['comparison'] = {'actual': actual}
                    row['native_support'] = native_support
                    row['native_failures'] = native_failures
                    row['covered_reference_outcomes'] = sum(any(comparable(actual) == comparable(expected) for actual in native_support) for expected in support)
                rows.append(row)
    native_after = _native_metadata(args.native) if args.native else None
    native_metadata = None
    integrity_errors = []
    if args.native:
        native_metadata, integrity_errors = _native_integrity(native_before, native_after)
    comparison_errors = ([{'name': row.get('name'), 'comparison': row.get('comparison')}
                          for row in rows
                          if row.get('comparison') not in ('match', 'error_match')]
                         if args.native else [])
    comparison_errors.extend(integrity_errors)
    artifact = dict(reference=pm4py.__version__,
                    reference_commit=REFERENCE_COMMIT,
                    reference_commit_scope=REFERENCE_COMMIT_SCOPE,
                    environment=_environment(),
                    reference_module=_module_metadata(importer),
                    r4pm=backend,
                    reference_samples=max(1, args.reference_samples),
                    native_samples=max(1, args.native_samples) if args.native else 0,
                    native_executable=native_metadata,
                    rows=rows,
                    errors=comparison_errors)
    args.output.write_text(json.dumps(artifact, indent=2) + '\n', encoding='utf-8')
    print(f'Captured {len(rows)} grouping observations')
    if args.native:
        matches = sum(row['comparison'] in ('match', 'error_match') for row in rows)
        print(f'{matches}/{len(rows)} grouping comparisons matched (errors compare missing/duplicate-key failures only)')
        variable = [row for row in rows if len(row['reference_support']) > 1]
        print(f'Variable-case observed support: {sum(row["covered_reference_outcomes"] for row in variable)}/{sum(len(row["reference_support"]) for row in variable)} outcomes reached')
        if artifact['errors']:
            return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
