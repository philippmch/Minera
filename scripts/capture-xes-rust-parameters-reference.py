"""Capture public RUSTXES parameters and return modes without source reads."""
import argparse
import hashlib
import inspect
import json
import os
import runpy
import tempfile
from importlib.metadata import version
from pathlib import Path
import warnings


REFERENCE_VERSION = '2.7.22'
BACKEND_VERSION = '0.6.2'
REFERENCE_COMMIT = '0715c31f3f1d85924107d6aa5329806db69df899'
REFERENCE_COMMIT_SCOPE = 'configured_reference_pin'
pm4py = pd = pl = importer = normalize = None


def _load_public_reference_api():
    global pm4py, pd, pl, importer, normalize
    warnings.filterwarnings('ignore')
    import pandas as pd_module
    import polars as pl_module
    import pm4py as pm4py_module
    from pm4py.objects.log.importer.xes import importer as importer_module
    normalize = runpy.run_path(str(Path(__file__).with_name('capture-xes-rust-scalar-reference.py')))['normalize']
    pm4py, pd, pl, importer = pm4py_module, pd_module, pl_module, importer_module
    return pm4py, pd, pl, importer


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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    pm4py, pd, pl, importer = _load_public_reference_api()
    _require_version(pm4py)
    backend = version('r4pm')
    _require_backend(backend)
    rust = importer.Variants.RUSTXES.value
    cases = [('default', {})]
    ignored = {
        'max_traces': [0, 1, 'invalid'],
        'timestamp_sort': [True, 'invalid'],
        'reverse_sort': [True, 'invalid'],
        'activity_key': ['other', None],
        'pm4py:param:activity_key': ['other', None],
        'case_id_key': ['other', None],
        'pm4py:param:case_id_key': ['other', None, 'concept:name'],
        'timestamp_key': ['other', None],
        'pm4py:param:timestamp_key': ['other', None],
        'attributes_to_read': [['concept:name'], [], 'invalid'],
        'set_attributes_to_read': [['concept:name'], [], 'invalid'],
        'pm4py:param:attributes_to_read': [['concept:name'], [], 'invalid'],
    }
    for key, values in ignored.items():
        cases.extend((key, {key: value}) for value in values)
    for legacy in (False, True):
        for lazy in (False, True):
            cases.append(('return_modes', dict(return_legacy_log_object=legacy, return_pl_lazyframe=lazy)))
    for lazy in (False, True):
        cases.append(('raw_mode_case_key', {'return_legacy_log_object': False, 'return_pl_lazyframe': lazy, 'pm4py:param:case_id_key': 'other'}))
    xml = '<log><trace><string key="concept:name" value="z"/><event><string key="concept:name" value="B"/><date key="time:timestamp" value="2024-01-02T00:00:00Z"/><string key="extra" value="b"/></event><event><string key="concept:name" value="A"/><date key="time:timestamp" value="2024-01-01T00:00:00Z"/></event></trace><trace><string key="concept:name" value="a"/><event><string key="concept:name" value="C"/><date key="time:timestamp" value="2023-01-01T00:00:00Z"/></event></trace></log>'
    rows = []
    with tempfile.TemporaryDirectory(prefix='minera-rust-params-') as directory:
        path = Path(directory) / 'input.xes'
        path.write_text(xml, encoding='utf-8')
        for name, parameters in cases:
            try:
                value = importer.apply(str(path), variant=importer.Variants.RUSTXES, parameters=parameters)
                result = dict(type=type(value).__name__, module=type(value).__module__)
                if result['type'] == 'EventLog':
                    result['traces'] = [dict(attributes=normalize(t.attributes), events=[normalize(dict(e)) for e in t]) for t in value]
                elif result['type'] == 'DataFrame':
                    result['rows'] = normalize(value.to_dict(orient='records'))
                    result['dtypes'] = {key: str(dtype) for key, dtype in value.dtypes.items()}
                elif result['type'] == 'LazyFrame':
                    materialized = value.collect()
                    result['rows'] = normalize(materialized.to_dicts())
                    result['dtypes'] = {key: str(dtype) for key, dtype in materialized.schema.items()}
                else:
                    raise RuntimeError(f'Unexpected return type {result["type"]}')
            except BaseException as error:
                result = dict(error=type(error).__name__, message=str(error))
            rows.append(dict(name=name, parameters=parameters, result=result))
    baseline = rows[0]['result']
    for row in rows:
        if row['name'] in ignored:
            row['unchanged'] = row['result'] == baseline
    validation_errors = []
    if not all(row.get('unchanged', True) for row in rows if row['name'] != 'pm4py:param:case_id_key'):
        validation_errors.append('generic parameters changed the baseline result')
    if any(row.get('unchanged', False) for row in rows if row['name'] == 'pm4py:param:case_id_key'):
        validation_errors.append('pm4py:param:case_id_key did not change the baseline result')
    if [row.get('result', {}).get('type') for row in rows if row['name'] == 'return_modes'] != ['DataFrame', 'LazyFrame', 'EventLog', 'EventLog']:
        validation_errors.append('return_modes types differ from the public reference')
    if [row.get('result', {}).get('type') for row in rows if row['name'] == 'raw_mode_case_key'] != ['DataFrame', 'LazyFrame']:
        validation_errors.append('raw_mode_case_key types differ from the public reference')
    artifact = dict(reference=pm4py.__version__, reference_commit=REFERENCE_COMMIT,
                    reference_commit_scope=REFERENCE_COMMIT_SCOPE,
                    environment=_environment(), reference_module=_module_metadata(importer),
                    r4pm=backend, pandas=pd.__version__, polars=pl.__version__,
                    public_signature=str(inspect.signature(rust.apply)),
                    exposed_parameters={p.name: p.value for p in rust.Parameters},
                    xml=xml, rows=rows, validation_errors=validation_errors)
    args.output.write_text(json.dumps(artifact, indent=2) + '\n', encoding='utf-8')
    unchanged = sum(row.get('unchanged', False) for row in rows)
    print(f'Captured {len(rows)} parameter observations; {unchanged}/{sum("unchanged" in row for row in rows)} generic parameter calls unchanged')
    if validation_errors:
        for error in validation_errors:
            print(f'Validation failure: {error}')
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
