"""Compare native XES result JSON with the public raw-table capture."""
import argparse
import datetime
import gzip
import hashlib
import json
import math
import os
import re
import subprocess
import tempfile
from pathlib import Path


REFERENCE_VERSION = "2.7.22"
BACKEND_VERSION = "0.6.2"
REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"
REFERENCE_COMMIT_SCOPE = "configured_reference_pin"
_RAW_EXPECTED_OBSERVATION_IDENTITIES = (
    "default",
    "interleaved_cases_legacy", "interleaved_cases_dataframe", "interleaved_cases_lazy",
    "missing_case_all_legacy", "missing_case_all_dataframe", "missing_case_all_lazy",
    "missing_case_mixed_legacy", "missing_case_mixed_dataframe", "missing_case_mixed_lazy",
    "empty_trace_legacy", "empty_trace_dataframe", "empty_trace_lazy",
    "empty_named_extra_before_legacy", "empty_named_extra_before_dataframe", "empty_named_extra_before_lazy",
    "empty_only_attrs_legacy", "empty_only_attrs_dataframe", "empty_only_attrs_lazy",
    "empty_trace_missing_case_legacy", "empty_trace_missing_case_dataframe", "empty_trace_missing_case_lazy",
    "empty_log_legacy", "empty_log_dataframe", "empty_log_lazy",
    "trace_event_plain_collision_legacy", "trace_event_plain_collision_dataframe", "trace_event_plain_collision_lazy",
    "trace_event_case_prefix_collision_legacy", "trace_event_case_prefix_collision_dataframe", "trace_event_case_prefix_collision_lazy",
    "trace_event_case_identity_collision_legacy", "trace_event_case_identity_collision_dataframe", "trace_event_case_identity_collision_lazy",
    "null_only_columns_legacy", "null_only_columns_dataframe", "null_only_columns_lazy",
    "nullable_int_legacy", "nullable_int_dataframe", "nullable_int_lazy",
    "mixed_int_float_legacy", "mixed_int_float_dataframe", "mixed_int_float_lazy",
    "mixed_int_string_legacy", "mixed_int_string_dataframe", "mixed_int_string_lazy",
    "nan_float_legacy", "nan_float_dataframe", "nan_float_lazy",
    "nullable_date_legacy", "nullable_date_dataframe", "nullable_date_lazy",
    "null_only_date_legacy", "null_only_date_dataframe", "null_only_date_lazy",
    "nullable_bool_legacy", "nullable_bool_dataframe", "nullable_bool_lazy",
    "null_only_bool_legacy", "null_only_bool_dataframe", "null_only_bool_lazy",
    "mixed_bool_string_legacy", "mixed_bool_string_dataframe", "mixed_bool_string_lazy",
    "empty_event_key_legacy", "empty_event_key_dataframe", "empty_event_key_lazy",
    "empty_trace_key_legacy", "empty_trace_key_dataframe", "empty_trace_key_lazy",
    "empty_trace_and_event_keys_legacy", "empty_trace_and_event_keys_dataframe", "empty_trace_and_event_keys_lazy",
    "legacy_false_lazy_false", "legacy_false_lazy_true", "legacy_true_lazy_false", "legacy_true_lazy_true",
)

_CONVERSION_EXPECTED_OBSERVATION_IDENTITIES = (
    "selector_default", "selector_event_concept_name", "selector_event_customgroup",
    "selector_event_group", "selector_trace_case_group", "selector_trace_case_group_actual",
    "selector_missing_key", "selector_none_key", "selector_empty_key",
    "firstrow_gid", "numeric_gid", "boolean_gid", "mixed_numeric_gid", "null_gid", "nan_gid",
    "prefix_default", "prefix_case", "prefix_org", "prefix_custom", "prefix_empty", "prefix_short", "prefix_none",
    "flag_stream_postprocessing_true", "flag_stream_postprocessing_false",
    "flag_deepcopy_true", "flag_deepcopy_false",
    "flag_include_case_attributes_true", "flag_include_case_attributes_false",
    "flag_compress_true", "flag_compress_false", "flag_extensions_custom", "flag_all_conversion_flags",
    "raw_dataframe_conversion_options", "raw_lazy_conversion_options",
    "raw_dataframe_null_conversion_options", "raw_lazy_null_conversion_options",
    "empty_log_default", "empty_log_none_case", "empty_log_none_prefix",
    "valid_row_both_none", "malformed_xml_default",
)

_SCHEMA_EXPECTED_OBSERVATION_IDENTITIES = (
    "date_glue_equal_instants", "discarded_reserved_extension", "null_schema_extension",
    "discarded_reserved_extension_default", "null_schema_extension_default",
    "missing_glue_null_prefix", "null_glue_null_prefix",
)

_EXTENSIONS_EXPECTED_OBSERVATION_IDENTITIES = (
    "nonempty_omitted", "nonempty_none", "nonempty_list_empty", "nonempty_tuple_empty",
    "nonempty_dict_empty", "nonempty_concept", "nonempty_organizational",
    "nonempty_concept_organizational", "nonempty_duplicate_concept", "nonempty_concept_tuple_value",
    "nonempty_concept_string", "nonempty_bad_dict", "nonempty_custom_object",
    "nonempty_duplicate_custom_object", "nonempty_duplicate_name_forward", "nonempty_duplicate_name_reverse",
    "nonempty_same_prefix_distinct_names", "nonempty_missing_name_object", "nonempty_missing_prefix_object",
    "empty_omitted", "empty_none", "empty_list_empty", "empty_concept", "empty_custom_object",
    "raw_dataframe_bad_dict", "raw_lazy_concept",
)

_EMPTY_BOUNDARY_EXPECTED_OBSERVATION_IDENTITIES = (
    "single_selfclosing_trace:legacy", "single_selfclosing_trace:dataframe", "single_selfclosing_trace:lazy",
    "single_explicit_empty_trace:legacy", "single_explicit_empty_trace:dataframe", "single_explicit_empty_trace:lazy",
    "explicit_empty_then_selfclosing:legacy", "explicit_empty_then_selfclosing:dataframe", "explicit_empty_then_selfclosing:lazy",
    "selfclosing_before_live:legacy", "selfclosing_before_live:dataframe", "selfclosing_before_live:lazy",
    "selfclosing_after_live:legacy", "selfclosing_after_live:dataframe", "selfclosing_after_live:lazy",
    "two_selfclosing_after_live:legacy", "two_selfclosing_after_live:dataframe", "two_selfclosing_after_live:lazy",
    "explicit_empty_before_live:legacy", "explicit_empty_before_live:dataframe", "explicit_empty_before_live:lazy",
    "explicit_empty_after_live:legacy", "explicit_empty_after_live:dataframe", "explicit_empty_after_live:lazy",
    "named_empty_trace:legacy", "named_empty_trace:dataframe", "named_empty_trace:lazy",
    "named_empty_then_selfclosing:legacy", "named_empty_then_selfclosing:dataframe", "named_empty_then_selfclosing:lazy",
    "named_empty_before_live:legacy", "named_empty_before_live:dataframe", "named_empty_before_live:lazy",
    "named_empty_after_live:legacy", "named_empty_after_live:dataframe", "named_empty_after_live:lazy",
    "selfclosing_event:legacy", "selfclosing_event:dataframe", "selfclosing_event:lazy",
    "explicit_empty_event:legacy", "explicit_empty_event:dataframe", "explicit_empty_event:lazy",
)

CAPTURE_MANIFESTS = {
    "rustxes_raw_reference": (77, _RAW_EXPECTED_OBSERVATION_IDENTITIES),
    "rustxes_conversion_reference": (41, _CONVERSION_EXPECTED_OBSERVATION_IDENTITIES),
    "rustxes_conversion_schema_reference": (7, _SCHEMA_EXPECTED_OBSERVATION_IDENTITIES),
    "rustxes_conversion_extensions_reference": (26, _EXTENSIONS_EXPECTED_OBSERVATION_IDENTITIES),
    "rustxes_empty_boundary_reference": (42, _EMPTY_BOUNDARY_EXPECTED_OBSERVATION_IDENTITIES),
}

# Keep the raw names as compatibility aliases for callers that used the first
# checker before it accepted the other RUSTXES evidence families.
CAPTURE_ID = "rustxes_raw_reference"
EXPECTED_OBSERVATION_COUNT = CAPTURE_MANIFESTS[CAPTURE_ID][0]


def _expected_observation_identities(capture_id=CAPTURE_ID):
    return list(CAPTURE_MANIFESTS[capture_id][1])


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


def _native_metadata(path):
    try:
        return _file_metadata(path)
    except (OSError, RuntimeError) as error:
        return {"path": str(Path(path).resolve()), "sha256": None, "error": str(error)}


def _load_reference_bytes(path):
    try:
        raw = path.read_bytes()
        if path.suffix == ".gz":
            document = json.loads(gzip.decompress(raw).decode("utf-8"))
        else:
            document = json.loads(raw.decode("utf-8"))
    except (OSError, UnicodeError, EOFError, json.JSONDecodeError) as error:
        raise RuntimeError(f"Unable to load reference report {path}: {error}") from error
    if not isinstance(document, dict):
        raise ValueError("reference report must be a JSON object")
    if document.get("reference") != REFERENCE_VERSION:
        raise ValueError(f"reference report requires reference=={REFERENCE_VERSION}")
    if document.get("reference_commit") != REFERENCE_COMMIT:
        raise ValueError("reference report has an unexpected reference commit")
    if document.get("reference_commit_scope") != REFERENCE_COMMIT_SCOPE:
        raise ValueError("reference report has an unexpected reference commit scope")
    if document.get("r4pm") != BACKEND_VERSION:
        raise ValueError(f"reference report requires r4pm=={BACKEND_VERSION}")
    module = document.get("reference_module")
    if (not isinstance(module, dict)
            or not isinstance(module.get("sha256"), str)
            or not re.fullmatch(r"[0-9a-fA-F]{64}", module["sha256"])):
        raise ValueError("reference report reference_module must include an opaque sha256")
    capture_id = document.get("capture_id")
    if not isinstance(capture_id, str):
        raise ValueError("reference report capture_id must be a string")
    manifest = CAPTURE_MANIFESTS.get(capture_id)
    if manifest is None:
        raise ValueError("reference report has an unsupported capture_id")
    expected_count, expected_ids_tuple = manifest
    if document.get("expected_observation_count") != expected_count:
        raise ValueError("reference report has an unexpected observation count manifest")
    expected_ids = list(expected_ids_tuple)
    if document.get("expected_observation_identities") != expected_ids:
        raise ValueError("reference report has an unexpected observation identity manifest")
    if document.get("errors") or document.get("validation_errors"):
        raise ValueError("reference capture contains errors or validation_errors")
    if not isinstance(document.get("observations"), list):
        raise ValueError("reference report observations must be a list")
    observations = document["observations"]
    if len(observations) != expected_count:
        raise ValueError(
            f"reference report has {len(observations)} observations; expected "
            f"{expected_count}")
    identities = [observation.get("observation_id") if isinstance(observation, dict) else None
                  for observation in observations]
    if (any(not isinstance(identity, str) for identity in identities)
            or identities != expected_ids
            or len(set(identities)) != len(expected_ids)):
        raise ValueError("reference observations are empty, truncated, duplicated, or out of order")
    return document, raw


def _load_reference(path):
    return _load_reference_bytes(path)[0]


def _write_report(path, report):
    path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


def cell(value):
    if value is None or value.get('null'):
        return None
    kind = value['kind']
    raw = value.get('typed_value', value['value'])
    if kind == 'date':
        return {'date': raw}
    if kind == 'float':
        number = float(raw)
        return number if math.isfinite(number) else {'float': 'nan' if math.isnan(number) else 'inf' if number > 0 else '-inf'}
    if kind == 'int':
        return int(raw)
    if kind == 'boolean':
        return raw if isinstance(raw, bool) else raw == 'true'
    return value['value']


def comparable(value, empty_key=False):
    if isinstance(value, dict):
        if set(value) == {'date'} and value['date'] != 'NaT':
            return {'date': datetime.datetime.fromisoformat(value['date'].replace('Z', '+00:00')).astimezone(datetime.timezone.utc).isoformat()}
        return {('__empty_event_key__' if empty_key and re.fullmatch(r'column_\d+', key) else key): comparable(item, empty_key) for key, item in value.items()}
    if isinstance(value, list):
        return [comparable(item, empty_key) for item in value]
    if type(value) in (int, float):
        return {'number_type': type(value).__name__, 'value': value}
    if isinstance(value, str) and empty_key and re.fullmatch(r'column_\d+', value):
        return '__empty_event_key__'
    return value


def native_result(body, empty_key, expected=None):
    result = {'type': body['type']}
    payload = body['result']
    if body['type'] == 'EventLog':
        traces = []
        for trace in payload['log']['traces']:
            attributes = {a['key']: cell(a) for a in trace['attributes']}
            if trace.get('case_id_attribute'):
                a = trace['case_id_attribute']
                attributes[a['key']] = cell(a)
            events = []
            for event in trace['events']:
                fields = {a['key']: cell(a) for a in event['attributes']}
                for key in ('activity_attribute', 'timestamp_attribute', 'resource_attribute', 'lifecycle_attribute'):
                    if event.get(key):
                        a = event[key]
                        fields[a['key']] = cell(a)
                events.append(fields)
            traces.append({'attributes': attributes, 'events': events})
        result['traces'] = traces
        log = payload['log']
        metadata = log['metadata']
        details = {
            'attributes': {attribute['key']: cell(attribute) for attribute in log['attributes']},
            'extensions': {extension['name']: {'prefix': extension['prefix'], 'uri': extension['uri']}
                           for extension in metadata['extensions']},
            'classifiers': {classifier['name']: classifier['keys'] for classifier in metadata['classifiers']},
            'properties': {attribute['key']: cell(attribute) for attribute in metadata['properties']},
            'omni_present': {scope['scope']: {attribute['key']: cell(attribute) for attribute in scope['attributes']}
                             for scope in metadata['globals']},
        }
        if len(details['extensions']) != len(metadata['extensions']):
            raise ValueError('Duplicate extension names')
        for key, value in details.items():
            if expected and key in expected:
                result[key] = value
        if expected and 'metadata' in expected:
            result['metadata'] = {key: details[key] for key in expected['metadata']}
        return result
    table = payload['table']
    columns = table['columns']
    if len(columns) != len(set(columns)) or len(columns) != len(body['dtypes']):
        raise ValueError('Native table columns and dtypes have inconsistent lengths')
    if bool(table.get('lazy')) != (body['type'] == 'LazyFrame'):
        raise ValueError('Native table lazy flag does not match result type')
    if empty_key:
        for index, key in enumerate(columns):
            if re.fullmatch(r'column_\d+', key):
                if key != f'column_{index}':
                    raise ValueError('Generated column does not match schema position')
    if not all(len(row) == len(columns) for row in table['rows']):
        raise ValueError('Native table row width does not match schema')
    result.update(columns=sorted(columns), dtypes=dict(zip(columns, body['dtypes'])),
                  rows=[dict(zip(columns, map(cell, row))) for row in table['rows']],
                  row_count=len(table['rows']))
    return result


def extension_arguments(value, reference):
    if value is None:
        return ['--xes-conversion-infer-extensions', 'true']
    if value in ({}, {'tuple': []}):
        value = []
    if not isinstance(value, list):
        return None
    result = ['--xes-conversion-infer-extensions', 'false']
    for extension in value:
        if isinstance(extension, str) and extension.startswith('XESExtension.'):
            fields = reference.get('public_xes_extension_enum', {}).get('members', {}).get(extension.split('.', 1)[1])
            if fields is None:
                return None
        elif isinstance(extension, dict) and set(extension) == {'SimpleNamespace'}:
            descriptor = extension['SimpleNamespace']
            if not all(isinstance(descriptor.get(key), str) for key in ('name', 'prefix', 'uri')):
                return None
            fields = [descriptor[key] for key in ('name', 'prefix', 'uri')]
        else:
            return None
        result.extend(['--xes-conversion-extension', *fields])
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--native', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--samples', type=int, default=4)
    parser.add_argument('--skip-unsupported-parameters', action='store_true')
    args = parser.parse_args()
    if args.samples <= 0:
        _write_report(args.output, {
            'reference': None, 'reference_commit': REFERENCE_COMMIT,
            'reference_commit_scope': REFERENCE_COMMIT_SCOPE,
            'environment': _environment(), 'native_executable': None,
            'checks': [], 'skipped': [],
            'errors': [{'kind': 'invalid_samples', 'samples': args.samples}],
        })
        print('samples must be positive')
        return 1
    try:
        reference, reference_bytes = _load_reference_bytes(args.reference)
    except (OSError, RuntimeError, ValueError, UnicodeError, EOFError, json.JSONDecodeError) as error:
        _write_report(args.output, {
            'reference': None, 'reference_commit': REFERENCE_COMMIT,
            'reference_commit_scope': REFERENCE_COMMIT_SCOPE,
            'environment': _environment(), 'native_executable': None,
            'checks': [], 'skipped': [],
            'errors': [{'kind': 'invalid_reference_report', 'error': type(error).__name__,
                        'message': str(error)}],
        })
        print(f'Reference report error: {error}')
        return 1
    checks = []
    skipped = []
    errors = []
    native_before = _native_metadata(args.native)
    with tempfile.TemporaryDirectory(prefix='minera-raw-native-') as directory:
        for observation in reference['observations']:
            if not isinstance(observation, dict):
                errors.append({'kind': 'malformed_observation', 'observation': repr(observation)})
                continue
            observation = dict(observation)
            observation.setdefault('case', observation.get('name', 'unnamed'))
            name = observation.get('name', observation['case'])
            parameters = observation.get('parameters')
            if not isinstance(parameters, dict):
                checks.append({'name': name, 'match': False,
                               'error': 'observation parameters must be an object',
                               'parameters': parameters})
                errors.append({'kind': 'malformed_observation_parameters', 'name': name})
                continue
            supported = {'return_legacy_log_object', 'return_pl_lazyframe',
                         'pm4py:param:case_id_key', 'case_attribute_prefix',
                         'stream_postprocessing', 'deepcopy', 'include_case_attributes', 'compress', 'extensions'}
            unsupported = set(parameters) - supported
            extension_flags = []
            if 'extensions' in parameters:
                extension_flags = extension_arguments(parameters['extensions'], reference)
                if extension_flags is None:
                    unsupported.add('extensions (Python object kind unavailable in native API)')
            if unsupported and args.skip_unsupported_parameters:
                skipped.append({'name': name, 'parameters': sorted(unsupported)})
                continue
            if unsupported:
                message = f'Unmapped native parameters: {sorted(unsupported)}'
                checks.append({'name': name, 'match': False, 'error': message,
                               'parameters': parameters})
                errors.append({'kind': 'unsupported_parameters', 'name': name,
                               'parameters': sorted(unsupported)})
                continue
            source = observation.get('result')
            if not isinstance(source, dict):
                checks.append({'name': name, 'match': False,
                               'error': 'observation result must be an object',
                               'actual': repr(source)})
                errors.append({'kind': 'malformed_observation_result', 'name': name})
                continue
            if source.get('status') == 'exception':
                if not isinstance(source.get('exception'), str) or not isinstance(source.get('message'), str):
                    checks.append({'name': name, 'match': False,
                                   'error': 'exception result must include exception and message strings',
                                   'actual': source})
                    errors.append({'kind': 'malformed_exception_result', 'name': name})
                    continue
                source = {'error': source['exception'], 'message': source['message']}
            elif source.get('status') == 'ok':
                if 'result' not in source:
                    checks.append({'name': name, 'match': False,
                                   'error': 'ok result must include result payload',
                                   'actual': source})
                    errors.append({'kind': 'malformed_ok_result', 'name': name})
                    continue
                source = source['result']
            if not isinstance(source, dict):
                checks.append({'name': name, 'match': False,
                               'error': 'unwrapped observation result must be an object',
                               'actual': repr(source)})
                errors.append({'kind': 'malformed_unwrapped_result', 'name': name})
                continue
            expected = {key: value for key, value in source.items() if key != 'module'}
            if 'rows' in expected:
                expected.setdefault('row_count', len(expected['rows']))
            empty_key = observation['case'] in ('empty_event_key', 'empty_trace_and_event_keys')
            for compressed in (False, True):
                path = Path(directory) / ('input.xes.gz' if compressed else 'input.xes')
                data = observation['xml'].encode('utf-8')
                path.write_bytes(gzip.compress(data) if compressed else data)
                command = [str(args.native.resolve()), str(path), '--xes-import-variant', 'rustxes',
                           '--xes-return-legacy-log-object', str(parameters.get('return_legacy_log_object', True)).lower(),
                           '--xes-return-pl-lazyframe', str(parameters.get('return_pl_lazyframe', False)).lower()]
                command.extend(extension_flags)
                for key, flag in (('pm4py:param:case_id_key', 'case-id-key'), ('case_attribute_prefix', 'case-attribute-prefix')):
                    if key in parameters:
                        if parameters[key] is None:
                            command.extend(['--xes-conversion-null-' + flag, 'true'])
                        else:
                            if not isinstance(parameters[key], str):
                                errors.append({'kind': 'malformed_parameter', 'name': name,
                                               'parameter': key, 'expected': 'string',
                                               'actual': repr(parameters[key])})
                                checks.append({'name': name, 'gzip': compressed,
                                               'sample': None, 'match': False,
                                               'error': f'{key} must be a string'})
                                continue
                            command.extend(['--xes-conversion-' + flag, parameters[key]])
                for key in ('stream_postprocessing', 'deepcopy', 'include_case_attributes', 'compress'):
                    if key in parameters:
                        if not isinstance(parameters[key], bool):
                            errors.append({'kind': 'malformed_parameter', 'name': name,
                                           'parameter': key, 'expected': 'boolean',
                                           'actual': repr(parameters[key])})
                            checks.append({'name': name, 'gzip': compressed,
                                           'sample': None, 'match': False,
                                           'error': f'{key} must be a boolean'})
                            continue
                        command.extend(['--xes-conversion-' + key.replace('_', '-'), str(parameters[key]).lower()])
                for sample in range(args.samples):
                    try:
                        process = subprocess.run(command, capture_output=True, text=True, encoding='utf-8')
                    except (OSError, subprocess.TimeoutExpired) as error:
                        checks.append({'name': name, 'gzip': compressed, 'sample': sample,
                                       'match': False,
                                       'error': {'native_process_error': type(error).__name__,
                                                 'message': str(error)}})
                        errors.append({'kind': 'native_process_error', 'name': name,
                                       'gzip': compressed, 'sample': sample,
                                       'error': type(error).__name__, 'message': str(error)})
                        continue
                    if 'error' in expected:
                        message = str(expected.get('message', ''))
                        if 'MissingLastTrace' in message:
                            error_key = 'MissingLastTrace'
                        elif 'Empty event not handled' in message:
                            error_key = 'Empty event not handled'
                        elif expected['error'] == 'KeyError':
                            key = parameters.get('pm4py:param:case_id_key', 'case:concept:name')
                            error_key = 'None' if key is None else key or 'missing'
                        else:
                            error_key = 'prefix'
                        matched = process.returncode != 0 and error_key in process.stderr
                        actual = {'returncode': process.returncode, 'stdout': process.stdout,
                                  'stderr': process.stderr}
                    else:
                        if process.returncode != 0:
                            matched = False
                            actual = {'returncode': process.returncode, 'stdout': process.stdout,
                                      'stderr': process.stderr}
                        else:
                            try:
                                actual = native_result(json.loads(process.stdout), empty_key, expected)
                                normalized_expected = comparable(expected, empty_key)
                                normalized_actual = comparable(actual, empty_key)
                                # Column ordering is intentionally unspecified.
                                if 'columns' in normalized_expected:
                                    normalized_expected['columns'].sort()
                                    normalized_actual['columns'].sort()
                                matched = normalized_actual == normalized_expected
                            except (KeyError, IndexError, TypeError, ValueError, json.JSONDecodeError) as error:
                                matched = False
                                actual = {'native_malformed_output': type(error).__name__,
                                          'message': str(error), 'stdout': process.stdout,
                                          'stderr': process.stderr}
                    checks.append({'name': name, 'gzip': compressed, 'sample': sample, 'match': matched,
                                   **({} if matched else {'expected': expected, 'actual': actual})})
                    if not matched:
                        errors.append({'kind': 'comparison_mismatch', 'name': name,
                                       'gzip': compressed, 'sample': sample,
                                       'expected': expected, 'actual': actual})
    native_after = _native_metadata(args.native)
    native_metadata = {
        'path': native_before.get('path', native_after.get('path')),
        'sha256_before': native_before.get('sha256'),
        'sha256_after': native_after.get('sha256'),
    }
    if native_after.get('sha256') is not None:
        native_metadata['sha256'] = native_after['sha256']
    if native_before.get('error'):
        native_metadata['error'] = native_before['error']
        native_metadata['error_before'] = native_before['error']
        errors.append({'kind': 'native_hash_error', 'phase': 'before',
                       'error': native_before['error']})
    if native_after.get('error'):
        native_metadata['error'] = native_after['error']
        native_metadata['error_after'] = native_after['error']
        errors.append({'kind': 'native_hash_error', 'phase': 'after',
                       'error': native_after['error']})
    if (native_before.get('sha256') is not None
            and native_after.get('sha256') is not None
            and native_before['sha256'] != native_after['sha256']):
        native_metadata['changed'] = True
        errors.append({'kind': 'native_executable_changed',
                       'sha256_before': native_before['sha256'],
                       'sha256_after': native_after['sha256']})
    if not checks:
        errors.append({'kind': 'no_comparisons', 'message': 'No native comparisons were performed'})
    report = {
        'capture_id': reference.get('capture_id'),
        'reference': reference.get('reference'),
        'reference_commit': reference.get('reference_commit'),
        'reference_commit_scope': reference.get('reference_commit_scope'),
        'environment': _environment(),
        'reference_module': reference.get('reference_module'),
        'input_report': str(args.reference.resolve()),
        'input_report_sha256': hashlib.sha256(reference_bytes).hexdigest(),
        'native_executable': native_metadata,
        'samples': args.samples,
        'checks': checks, 'skipped': skipped, 'errors': errors,
    }
    _write_report(args.output, report)
    failures = [check for check in checks if not check['match']]
    print(f'{len(checks) - len(failures)}/{len(checks)} native {reference.get("capture_id")} comparisons matched')
    if skipped:
        print(f'Skipped {len(skipped)} observations with parameters unavailable in the native CLI')
    for failure in failures[:8]:
        print(json.dumps(failure))
    return 1 if failures or errors else 0


if __name__ == '__main__':
    raise SystemExit(main())
