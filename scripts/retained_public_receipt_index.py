"""Index explicitly admitted own public observations; never import the reference.

CLI: python -B scripts/retained_public_receipt_index.py MANIFEST DATABASE REPORT

The v1 manifest declares root, inventory_complete, scope (a human-readable bounded
inventory description), modules, and sources. Each source declares path, bytes,
sha256, provenance='own-public-observation', format ('json', 'jsonl',
'gzip-json', 'gzip-jsonl', 'gzip-envelope-json', 'gzip-envelope-jsonl'), adapter
('receipt-v1', 'pandas-before-v1', 'polars-receipt-v1', 'log-attributes-packed-v1',
'pandas-attributes-before-v1', or 'digest-plan-v1'),
receipt_schema, reference, runtime, and role
('observations', 'plans', or 'intents'). JSON object wrappers require records_key;
otherwise JSON must be one record or an array of records. Envelope sources also
declare members: an explicit list of similarly configured path/bytes/sha256
bindings (formats json/jsonl only). Envelope entries have path/bytes/sha256,
and either encoding='base64', data, or utf8 (exact retained text). The source's
records_key selects a wrapper member array if present. Unlisted entries are
recorded without decoding; they are outside the explicitly admitted scope.

Pins are manifest assertions, not inferred or independently authenticated. A
certificate covers only this explicit inventory and previously indexed history;
it never certifies a directory tree, all archives, or reference parity. Missing
schemas/operands/pins remain gaps, never reconstructed by guessing. Cache hits
still require a fresh full container hash. Database history is append-only.
The closed receipt adapters admit the three versioned LTL observer schemas and
the separate LOG/pandas attributes observers listed in SCHEMAS. New families require an explicit
adapter/schema addition with operand-form and canonicalization validation.
"""

from __future__ import annotations

import argparse
import base64
import datetime
import gzip
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import re
import sqlite3

VERSION = 'minera-retained-public-receipt-index-v1'
PARSER_SHA256 = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
MANIFEST = 'minera-retained-public-receipt-manifest-v1'
SCHEMAS = {
    'receipt-v1': ('minera-log-ltl-observer-receipt-v1', 'positional_args_keyword_controls'),
    'pandas-before-v1': ('minera-pandas-ltl-observer-receipt-v1', 'dataframe-positional-plus-explicit-args-and-keywords'),
    'polars-receipt-v1': ('minera-polars-ltl-observer-receipt-v1', 'source_positional_all_options_keyword'),
    'log-attributes-packed-v1': ('minera-log-attributes-observer-receipt-v1', 'positional_args_keyword_controls'),
    'pandas-attributes-before-v1': ('minera-pandas-attributes-observer-receipt-v1', 'dataframe-positional-plus-explicit-args-and-keywords'),
}
LOG_ATTRIBUTES_MODULE = 'pm4py.algo.filtering.log.attributes.attributes_filter'
LOG_ATTRIBUTES_ARITIES = {
    'apply': {2}, 'apply_events': {2}, 'apply_trace_attribute': {2},
    'apply_numeric': {3}, 'apply_numeric_events': {3},
    'filter_log_by_attributes_threshold': {5, 6},
    'filter_log_on_max_no_activities': {1, 2},
    'filter_log_relative_occurrence_event_attribute': {2},
}
LOG_ATTRIBUTES_ENUM = {
    'ATTRIBUTE_KEY': 'pm4py:param:attribute_key', 'ACTIVITY_KEY': 'pm4py:param:activity_key',
    'CASE_ID_KEY': 'pm4py:param:case_id_key', 'PARAMETER_KEY_CASE_GLUE': 'case_id_glue',
    'DECREASING_FACTOR': 'decreasingFactor', 'POSITIVE': 'positive',
    'STREAM_FILTER_KEY1': 'stream_filter_key1', 'STREAM_FILTER_VALUE1': 'stream_filter_value1',
    'STREAM_FILTER_KEY2': 'stream_filter_key2', 'STREAM_FILTER_VALUE2': 'stream_filter_value2',
    'KEEP_ONCE_PER_CASE': 'keep_once_per_case',
}
EXCLUDED = {'pm4py', 'site-packages', 'reference-runtime',
            'heuristic-subsecond-reference-runtime', 'node_modules', '__pycache__',
            '.git', '.aws'}
ALLOWED_SUFFIXES = ('.json', '.jsonl', '.json.gz', '.jsonl.gz')
FORMATS = {'json', 'jsonl', 'gzip-json', 'gzip-jsonl',
           'gzip-envelope-json', 'gzip-envelope-jsonl'}


def canonical(value):
    return json.dumps(value, ensure_ascii=True, allow_nan=False, sort_keys=True,
                      separators=(',', ':')).encode('ascii')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def safe_path(value):
    if not isinstance(value, str) or not value or '\\' in value or ':' in value:
        raise ValueError('path must be a nonempty relative POSIX path')
    path = PurePosixPath(value)
    if path.is_absolute() or any(x in {'', '.', '..'} for x in value.split('/')):
        raise ValueError('absolute or traversing path')
    if any(x.casefold() in EXCLUDED for x in path.parts):
        raise ValueError('excluded reference, dependency, cache or private path')
    if not value.casefold().endswith(ALLOWED_SUFFIXES):
        raise ValueError('only public observation JSON/JSONL paths are admitted')
    return value


def bound(data, spec):
    if type(spec.get('bytes')) is not int or spec['bytes'] != len(data):
        raise ValueError('size binding mismatch')
    if not re.fullmatch('[0-9a-f]{64}', str(spec.get('sha256', ''))):
        raise ValueError('missing or invalid SHA-256 binding')
    if sha(data) != spec['sha256']:
        raise ValueError('SHA-256 binding mismatch')


def load_json(data):
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError('duplicate JSON object key')
            result[key] = value
        return result
    def finite_float(value):
        result = float(value)
        if not math.isfinite(result):
            raise ValueError('nonfinite JSON float')
        return result
    return json.loads(data, object_pairs_hook=pairs, parse_float=finite_float,
                      parse_constant=lambda value: (_ for _ in ()).throw(
                          ValueError('nonfinite JSON constant: ' + value)))


def records(data, fmt, records_key=None):
    """Yield decoded records and their original exact UTF-8 byte slices."""
    text = data.decode('utf-8-sig')
    if fmt == 'jsonl':
        for raw in data.splitlines(keepends=True):
            line = raw.decode('utf-8-sig')
            if line.strip():
                yield load_json(line), raw
        return
    # Find original element spans rather than reserializing records for hashing.
    load_json(text)  # validates every key, value and all trailing content
    decoder = json.JSONDecoder()
    start = len(text) - len(text.lstrip())
    if records_key is not None:
        if text[start] != '{':
            raise ValueError('records_key requires an object wrapper')
        pos = start + 1
        found = None
        while True:
            pos = _skip(text, pos)
            if text[pos] == '}':
                break
            key, pos = decoder.raw_decode(text, pos)
            pos = _skip(text, pos)
            if text[pos] != ':':
                raise ValueError('invalid object wrapper')
            pos = _skip(text, pos + 1)
            _, end = decoder.raw_decode(text, pos)
            if key == records_key:
                found = pos
            pos = _skip(text, end)
            if text[pos] == '}':
                break
            pos += 1
        if found is None:
            raise ValueError('missing declared records_key')
        start = found
        if text[start] != '[':
            raise ValueError('declared records_key must contain an array')
    if text[start] != '[':
        _, end = decoder.raw_decode(text, start)
        raw = text[start:end].encode('utf-8')
        yield load_json(raw), raw
        return
    pos = start + 1
    while True:
        pos = _skip(text, pos)
        if text[pos] == ']':
            break
        _, end = decoder.raw_decode(text, pos)
        raw = text[pos:end].encode('utf-8')
        yield load_json(raw), raw
        pos = _skip(text, end)
        if text[pos] == ']':
            break
        pos += 1


def _skip(text, pos):
    while pos < len(text) and text[pos].isspace():
        pos += 1
    return pos


def _attributes_exact(value, fields):
    if type(value) is not dict or set(value) != set(fields):
        raise ValueError('unknown/incomplete LOG attributes descriptor fields')


def _attributes_trace(value, source, depth):
    _attributes_exact(value, ['attributes', 'properties', 'events'])
    for key in ('attributes', 'properties'):
        _attributes_packed(value[key], source, depth + 1, 'dict')
    if type(value['events']) is not list:
        raise ValueError('LOG attributes trace events absent')
    for event in value['events']:
        _attributes_packed(event, source, depth + 1, 'dict')


def _attributes_snapshot(value, source, depth=0):
    _attributes_exact(value, ['attributes', 'extensions', 'classifiers', 'omni_present', 'properties', 'traces'])
    for key in ('attributes', 'extensions', 'classifiers', 'omni_present', 'properties'):
        _attributes_packed(value[key], source, depth + 1, 'dict')
    if type(value['traces']) is not list:
        raise ValueError('LOG attributes source traces absent')
    for trace in value['traces']:
        _attributes_trace(trace, source, depth + 1)


def _attributes_packed(value, source, depth=0, expected=None):
    if depth > 256 or type(value) is not dict or type(value.get('type')) is not str:
        raise ValueError('LOG attributes typed descriptor absent/too deep')
    kind = value['type']
    if expected is not None and kind != expected:
        raise ValueError('LOG attributes packed container type mismatch')
    if kind in {'NoneType', 'str', 'bool', 'int'}:
        _attributes_exact(value, ['type', 'value'])
        if type(value['value']) is not {'NoneType': type(None), 'str': str, 'bool': bool, 'int': int}[kind]:
            raise ValueError('LOG attributes typed scalar payload mismatch')
    elif kind == 'float':
        _attributes_exact(value, ['type', 'hex'])
        try:
            if type(value['hex']) is not str or float.fromhex(value['hex']).hex() != value['hex']:
                raise ValueError()
        except (ValueError, OverflowError):
            raise ValueError('invalid original LOG attributes float hex') from None
    elif kind == 'bytes':
        _attributes_exact(value, ['type', 'hex'])
        if type(value['hex']) is not str or not re.fullmatch('(?:[0-9a-f]{2})*', value['hex']):
            raise ValueError('invalid original LOG attributes bytes hex')
    elif kind in {'native_na', 'nat'}:
        _attributes_exact(value, ['type'])
    elif kind == 'uuid':
        _attributes_exact(value, ['type', 'value'])
        if type(value['value']) is not str or not re.fullmatch('[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}', value['value']):
            raise ValueError('invalid original LOG attributes UUID')
    elif kind == 'datetime':
        _attributes_exact(value, ['type', 'iso', 'fold', 'offset_seconds'])
        if type(value['iso']) is not str or type(value['fold']) is not int or value['fold'] not in (0, 1):
            raise ValueError('invalid LOG attributes datetime metadata')
        try:
            offset = datetime.datetime.fromisoformat(value['iso']).utcoffset()
        except (ValueError, OverflowError):
            raise ValueError('invalid LOG attributes datetime iso') from None
        seconds = value['offset_seconds']
        if not (seconds is None or type(seconds) in (int, float) and math.isfinite(seconds)) or seconds != (None if offset is None else offset.total_seconds()):
            raise ValueError('LOG attributes datetime offset mismatch')
    elif kind == 'enum':
        _attributes_exact(value, ['type', 'module', 'name', 'value'])
        if value['module'] != LOG_ATTRIBUTES_MODULE or type(value['name']) is not str or value['name'] not in LOG_ATTRIBUTES_ENUM or value['value'] != {'type': 'str', 'value': LOG_ATTRIBUTES_ENUM[value['name']]}:
            raise ValueError('unknown LOG attributes enum identity/value')
    elif kind in {'list', 'tuple', 'set', 'frozenset'}:
        _attributes_exact(value, ['type', 'values'])
        if type(value['values']) is not list:
            raise ValueError('LOG attributes typed sequence absent')
        for child in value['values']:
            _attributes_packed(child, source, depth + 1)
        if kind in {'set', 'frozenset'} and value['values'] != sorted(value['values'], key=lambda item: json.dumps(item, sort_keys=True)):
            raise ValueError('LOG attributes original set ordering mismatch')
    elif kind == 'dict':
        _attributes_exact(value, ['type', 'items'])
        if type(value['items']) is not list:
            raise ValueError('LOG attributes ordered dictionary entries absent')
        for pair in value['items']:
            if type(pair) is not list or len(pair) != 2:
                raise ValueError('LOG attributes ordered key/value pair absent')
            for child in pair:
                _attributes_packed(child, source, depth + 1)
    elif kind == 'log':
        _attributes_exact(value, ['type', 'snapshot'])
        _attributes_snapshot(value['snapshot'], source, depth + 1)
    elif kind == 'trace':
        _attributes_exact(value, ['type', 'snapshot', 'source_trace_index'])
        _attributes_trace(value['snapshot'], source, depth + 1)
        index = value['source_trace_index']
        if index is not None:
            if type(index) is not int or not 0 <= index < len(source['traces']) or value['snapshot'] != source['traces'][index]:
                raise ValueError('LOG attributes source trace alias mismatch')
    else:
        raise ValueError('unknown LOG attributes packed type')


def _attributes_receipt(receipt):
    _attributes_exact(receipt, ['module', 'function', 'args', 'kwargs', 'call_shape'])
    if receipt['module'] != LOG_ATTRIBUTES_MODULE or type(receipt['function']) is not str or receipt['function'] not in LOG_ATTRIBUTES_ARITIES:
        raise ValueError('unknown LOG attributes module/function')
    if receipt['call_shape'] != SCHEMAS['log-attributes-packed-v1'][1]:
        raise ValueError('unknown LOG attributes original call shape')
    args = receipt['args']
    _attributes_exact(args, ['type', 'values'])
    if args['type'] != 'list' or type(args['values']) is not list or len(args['values']) not in LOG_ATTRIBUTES_ARITIES[receipt['function']]:
        raise ValueError('LOG attributes positional operand arity mismatch')
    first = args['values'][0]
    _attributes_exact(first, ['type', 'snapshot'])
    if first['type'] != 'log':
        raise ValueError('complete LOG attributes primary source absent')
    source = first['snapshot']
    _attributes_snapshot(source, source)
    _attributes_packed(args, source, expected='list')
    _attributes_packed(receipt['kwargs'], source, expected='dict')
    keywords = []
    for key, _ in receipt['kwargs']['items']:
        if key.get('type') != 'str' or key.get('value') != 'parameters' or key['value'] in keywords:
            raise ValueError('unknown/duplicate LOG attributes keyword operand')
        keywords.append(key['value'])
    return source


def _attributes_outcome(row, receipt, source):
    """Validate the known full terminal observer shape; no case execution."""
    base = {'index', 'module', 'case', 'receipt_sha256', 'receipt', 'input_before', 'controls_before',
            'warnings', 'input_after', 'controls_after', 'input_unchanged', 'controls_unchanged'}
    success = 'error' not in row
    _attributes_exact(row, base | ({'result_kind', 'result', 'returned_input_identity', 'trace_aliases_source', 'event_source_positions'} if success else {'error'}))
    if type(row['index']) is not int or row['index'] < 1 or type(row['case']) is not dict or row['case'].get('function') != receipt['function'] or type(row['case'].get('name')) is not str:
        raise ValueError('LOG attributes observer index/case identity mismatch')
    if row['input_before'] != source or row['controls_before'] != {'args': receipt['args'], 'kwargs': receipt['kwargs']}:
        raise ValueError('LOG attributes original source/control before mismatch')
    after = row['input_after']
    _attributes_snapshot(after, after)
    controls = row['controls_after']
    _attributes_exact(controls, ['args', 'kwargs'])
    _attributes_packed(controls['args'], after, expected='list')
    _attributes_packed(controls['kwargs'], after, expected='dict')
    if len(controls['args']['values']) != len(receipt['args']['values']) or controls['args']['values'][0] != {'type': 'log', 'snapshot': after}:
        raise ValueError('LOG attributes after source alias mismatch')
    if [pair[0] for pair in controls['kwargs']['items']] != [pair[0] for pair in receipt['kwargs']['items']]:
        raise ValueError('LOG attributes top-level keyword identities mutated')
    if type(row['input_unchanged']) is not bool or row['input_unchanged'] != (source == after) or type(row['controls_unchanged']) is not bool or row['controls_unchanged'] != (row['controls_before'] == controls):
        raise ValueError('LOG attributes before/after mutation flag mismatch')
    if type(row['warnings']) is not list:
        raise ValueError('complete LOG attributes warnings absent')
    for warning in row['warnings']:
        _attributes_exact(warning, ['category', 'message'])
        if type(warning['category']) is not str or type(warning['message']) is not str:
            raise ValueError('invalid LOG attributes warning metadata')
    if not success:
        _attributes_exact(row['error'], ['type', 'message', 'args'])
        if type(row['error']['type']) is not str or not row['error']['type'] or type(row['error']['message']) is not str:
            raise ValueError('complete LOG attributes error absent')
        _attributes_packed(row['error']['args'], after, expected='tuple')
        return
    if row['result_kind'] != 'log' or type(row['returned_input_identity']) is not bool:
        raise ValueError('unknown LOG attributes successful outcome')
    _attributes_snapshot(row['result'], after)
    if row['returned_input_identity'] and row['result'] != after:
        raise ValueError('LOG attributes returned source identity contradicts its full snapshot')
    aliases = row['trace_aliases_source']; positions = row['event_source_positions']
    if type(aliases) is not list or type(positions) is not list or len(aliases) != len(row['result']['traces']) or len(positions) != len(aliases) or any(type(x) is not bool for x in aliases):
        raise ValueError('LOG attributes result alias metadata shape mismatch')
    if row['returned_input_identity'] and not all(aliases):
        raise ValueError('LOG attributes returned source identity contradicts trace alias flags')
    for trace, is_alias, trace_positions in zip(row['result']['traces'], aliases, positions):
        if is_alias and trace not in after['traces']:
            raise ValueError('LOG attributes returned source trace alias mismatch')
        if type(trace_positions) is not list:
            raise ValueError('LOG attributes event source positions absent')
        for pair in trace_positions:
            if type(pair) is not list or len(pair) != 2 or any(type(x) is not int for x in pair):
                raise ValueError('LOG attributes event source pair malformed')
            ti, ei = pair
            if not 0 <= ti < len(after['traces']) or not 0 <= ei < len(after['traces'][ti]['events']) or after['traces'][ti]['events'][ei] not in trace['events']:
                raise ValueError('LOG attributes event alias snapshot mismatch')


PANDAS_ATTRIBUTES_PROFILE = {'module': 'pm4py.algo.filtering.pandas.attributes.attributes_filter',
 'call_shape': 'dataframe-positional-plus-explicit-args-and-keywords',
 'frame_kind': 'pandas.DataFrame',
 'arities': {'apply': [1],
             'apply_events': [1],
             'apply_numeric': [2],
             'apply_numeric_events': [2],
             'filter_df_relative_occurrence_event_attribute': [1],
             'filter_df_keeping_activ_exc_thresh': [1],
             'filter_df_keeping_spno_activities': [0],
             'filter_df_on_attribute_values': [1]},
 'keywords': {'apply': ['parameters'],
              'apply_events': ['parameters'],
              'apply_numeric': ['parameters'],
              'apply_numeric_events': ['parameters'],
              'filter_df_relative_occurrence_event_attribute': ['parameters'],
              'filter_df_keeping_activ_exc_thresh': ['act_count0',
                                                     'activity_key',
                                                     'most_common_variant'],
              'filter_df_keeping_spno_activities': ['activity_key', 'max_no_activities'],
              'filter_df_on_attribute_values': ['case_id_glue',
                                                'attribute_key',
                                                'positive',
                                                'keep_nan_values']},
 'enum_values': {'ATTRIBUTE_KEY': {'type': 'str', 'value': 'pm4py:param:attribute_key'},
                 'ACTIVITY_KEY': {'type': 'str', 'value': 'pm4py:param:activity_key'},
                 'CASE_ID_KEY': {'type': 'str', 'value': 'pm4py:param:case_id_key'},
                 'DECREASING_FACTOR': {'type': 'str', 'value': 'decreasingFactor'},
                 'POSITIVE': {'type': 'str', 'value': 'positive'},
                 'STREAM_FILTER_KEY1': {'type': 'str', 'value': 'stream_filter_key1'},
                 'STREAM_FILTER_VALUE1': {'type': 'str', 'value': 'stream_filter_value1'},
                 'STREAM_FILTER_KEY2': {'type': 'str', 'value': 'stream_filter_key2'},
                 'STREAM_FILTER_VALUE2': {'type': 'str', 'value': 'stream_filter_value2'},
                 'KEEP_ONCE_PER_CASE': {'type': 'str', 'value': 'keep_once_per_case'},
                 'KEEP_NAN_VALUES': {'type': 'str', 'value': 'keep_nan_values'}}}


def _pandas_attributes_need(value, message):
    if not value:
        raise ValueError(message)


def _pandas_attributes_exact(value, fields, label):
    _pandas_attributes_need(type(value) is dict and set(value) == set(fields), label)


def _pandas_attributes_packed(value, profile, tags, depth=0, expected=None):
    _pandas_attributes_need(depth < 192 and type(value) is dict and (type(value.get('type')) is str), 'packed value absent/too deep')
    kind = value['type']
    tags[kind] = tags.get(kind, 0) + 1
    _pandas_attributes_need(expected is None or kind == expected, 'packed container kind mismatch')
    if kind in {'NoneType', 'str', 'bool', 'int'}:
        _pandas_attributes_exact(value, ['type', 'value'], 'unknown scalar descriptor fields')
        _pandas_attributes_need(type(value['value']) is {'NoneType': type(None), 'str': str, 'bool': bool, 'int': int}[kind], 'typed scalar payload mismatch')
    elif kind == 'float':
        _pandas_attributes_exact(value, ['type', 'hex'], 'unknown float descriptor fields')
        try:
            _pandas_attributes_need(type(value['hex']) is str and float.fromhex(value['hex']).hex() == value['hex'], 'noncanonical packed float hex')
        except (ValueError, OverflowError):
            raise ValueError('invalid packed float hex') from None
    elif kind == 'bytes':
        _pandas_attributes_exact(value, ['type', 'hex'], 'unknown bytes descriptor fields')
        _pandas_attributes_need(type(value['hex']) is str and re.fullmatch('(?:[0-9a-f]{2})*', value['hex']), 'invalid packed bytes hex')
    elif kind in {'pandas.NA', 'pandas.NaT'}:
        _pandas_attributes_exact(value, ['type'], 'unknown pandas special-value fields')
    elif kind in {'datetime', 'pandas.Timestamp'}:
        _pandas_attributes_exact(value, ['type', 'iso'] if kind == 'datetime' else ['type', 'iso', 'resolution', 'timezone'], 'unknown timestamp descriptor fields')
        _pandas_attributes_need(type(value['iso']) is str, 'timestamp iso absent')
        try:
            datetime.datetime.fromisoformat(value['iso'])
        except (ValueError, OverflowError):
            raise ValueError('invalid timestamp iso') from None
        if kind == 'pandas.Timestamp':
            _pandas_attributes_need(value['resolution'] in {'s', 'ms', 'us', 'ns'} and (value['timezone'] is None or type(value['timezone']) is str), 'timestamp resolution/timezone absent')
    elif kind == 'enum':
        _pandas_attributes_exact(value, ['type', 'module', 'name', 'value'], 'unknown enum descriptor fields')
        _pandas_attributes_need(value['module'] == profile['module'] and type(value['name']) is str and (value['name'] in profile['enum_values']) and (value['value'] == profile['enum_values'][value['name']]), 'unknown enum module/name/value identity')
    elif kind in {'list', 'tuple', 'set', 'frozenset'}:
        _pandas_attributes_exact(value, ['type', 'values'], 'unknown sequence descriptor fields')
        _pandas_attributes_need(type(value['values']) is list, 'packed sequence array absent')
        for child in value['values']:
            _pandas_attributes_packed(child, profile, tags, depth + 1)
        if kind in {'set', 'frozenset'}:
            _pandas_attributes_need(value['values'] == sorted(value['values'], key=lambda x: json.dumps(x, sort_keys=True)), 'original producer set order differs')
    elif kind == 'dict':
        _pandas_attributes_exact(value, ['type', 'items'], 'unknown dictionary descriptor fields')
        _pandas_attributes_need(type(value['items']) is list, 'ordered typed dictionary entries absent')
        for pair in value['items']:
            _pandas_attributes_need(type(pair) is list and len(pair) == 2, 'typed key/value pair absent')
            for child in pair:
                _pandas_attributes_packed(child, profile, tags, depth + 1)
    elif kind == 'pandas.DataFrame':
        _pandas_attributes_exact(value, ['type', 'frame'], 'unknown packed DataFrame fields')
        _pandas_attributes_frame(value['frame'], profile, tags, depth + 1)
    else:
        raise ValueError('unknown packed observer value kind')


def _pandas_attributes_frame(value, profile, tags, depth=0):
    _pandas_attributes_need(depth < 192, 'frame nesting too deep')
    _pandas_attributes_exact(value, ['kind', 'columns', 'column_names', 'dtypes', 'categorical_dtypes', 'index', 'attrs', 'rows'], 'unknown/incomplete frame snapshot')
    _pandas_attributes_need(value['kind'] == profile['frame_kind'], 'unreviewed frame class')
    _pandas_attributes_packed(value['columns'], profile, tags, depth + 1, 'list')
    _pandas_attributes_packed(value['column_names'], profile, tags, depth + 1, 'list')
    _pandas_attributes_packed(value['attrs'], profile, tags, depth + 1, 'dict')
    width = len(value['columns']['values'])
    _pandas_attributes_need(type(value['dtypes']) is list and len(value['dtypes']) == width and all((type(x) is str for x in value['dtypes'])), 'frame dtype cardinality mismatch')
    categories = value['categorical_dtypes']
    _pandas_attributes_need(type(categories) is list and len(categories) == width, 'frame categorical metadata cardinality mismatch')
    for dtype, category in zip(value['dtypes'], categories):
        _pandas_attributes_need((dtype == 'category') == (category is not None), 'categorical dtype/metadata mismatch')
        if category is not None:
            _pandas_attributes_exact(category, ['categories', 'categories_dtype', 'ordered'], 'unknown categorical metadata')
            _pandas_attributes_need(type(category['categories_dtype']) is str and type(category['ordered']) is bool, 'incomplete categorical metadata')
            _pandas_attributes_packed(category['categories'], profile, tags, depth + 1, 'list')
    index = value['index']
    _pandas_attributes_exact(index, ['kind', 'dtype', 'names', 'labels'], 'unknown/incomplete index snapshot')
    _pandas_attributes_need(index['kind'] in {'RangeIndex', 'Index', 'MultiIndex'} and type(index['dtype']) is str, 'unreviewed index kind/dtype')
    _pandas_attributes_packed(index['names'], profile, tags, depth + 1, 'list')
    _pandas_attributes_packed(index['labels'], profile, tags, depth + 1, 'list')
    _pandas_attributes_need(type(value['rows']) is list and len(value['rows']) == len(index['labels']['values']), 'row/index cardinality mismatch')
    for row in value['rows']:
        _pandas_attributes_packed(row, profile, tags, depth + 1, 'list')
        _pandas_attributes_need(len(row['values']) == width, 'row/column cardinality mismatch')
    _pandas_attributes_need(len(index['names']['values']) >= 1, 'index names absent')
    if index['kind'] == 'MultiIndex':
        for label in index['labels']['values']:
            _pandas_attributes_need(label['type'] == 'tuple' and len(label['values']) == len(index['names']['values']), 'multi-index label/name arity mismatch')
    else:
        _pandas_attributes_need(len(index['names']['values']) == 1, 'ordinary index name arity mismatch')


def _pandas_attributes_receipt_of(row):
    return {'module': row['module'], 'method': row['method'], 'input': row['input_before'], 'args': row['args_before'], 'kwargs': row['kwargs_before'], 'call_shape': row['call_shape']}


def _pandas_attributes_receipt(receipt, profile, tags):
    _pandas_attributes_exact(receipt, ['module', 'method', 'input', 'args', 'kwargs', 'call_shape'], 'unknown/incomplete original receipt')
    _pandas_attributes_need(receipt['module'] == profile['module'] and type(receipt['method']) is str and (receipt['method'] in profile['arities']) and (receipt['call_shape'] == profile['call_shape']), 'unreviewed module/method/call shape')
    _pandas_attributes_frame(receipt['input'], profile, tags)
    _pandas_attributes_packed(receipt['args'], profile, tags, expected='list')
    _pandas_attributes_packed(receipt['kwargs'], profile, tags, expected='dict')
    _pandas_attributes_need(len(receipt['args']['values']) in profile['arities'][receipt['method']], 'positional operand arity mismatch')
    seen = set()
    for key, _ in receipt['kwargs']['items']:
        _pandas_attributes_need(key['type'] == 'str' and key['value'] in profile['keywords'][receipt['method']] and (key['value'] not in seen), 'unreviewed/duplicate method keyword')
        seen.add(key['value'])


def _pandas_attributes_outcome(row, profile):
    _pandas_attributes_need(type(row) is dict, 'record is not an object')
    _pandas_attributes_need(any((k in row for k in ['result', 'error', 'value_result', 'result_value', 'output', 'returned', 'exception'])), 'record has no actual terminal outcome')
    base = {'index', 'case', 'module', 'method', 'receipt_sha256', 'input_before', 'args_before', 'kwargs_before', 'call_shape', 'warnings', 'input_after', 'args_after', 'kwargs_after', 'input_unchanged', 'arguments_unchanged'}
    success = 'error' not in row
    _pandas_attributes_exact(row, base | ({'result', 'returned_input_identity'} if success else {'error'}), 'unknown/mixed/incomplete observation fields')
    _pandas_attributes_need(type(row['index']) is int and row['index'] > 0, 'actual observer index must be a positive integer')
    receipt = _pandas_attributes_receipt_of(row)
    tags = {}
    _pandas_attributes_receipt(receipt, profile, tags)
    case = row['case']
    _pandas_attributes_exact(case, ['name', 'method', 'shape', 'args', 'kwargs'], 'unknown/incomplete saved case recipe')
    _pandas_attributes_need(type(case['name']) is str and type(case['shape']) is str and (case['method'] == row['method']) and type(case['args']) is list and type(case['kwargs']) is dict, 'case/method identity mismatch')
    _pandas_attributes_frame(row['input_after'], profile, tags)
    _pandas_attributes_packed(row['args_after'], profile, tags, expected='list')
    _pandas_attributes_packed(row['kwargs_after'], profile, tags, expected='dict')
    _pandas_attributes_need(len(row['args_after']['values']) == len(row['args_before']['values']) and [pair[0] for pair in row['kwargs_after']['items']] == [pair[0] for pair in row['kwargs_before']['items']], 'outer argument/keyword shape changed outside captured operands')
    _pandas_attributes_need(type(row['input_unchanged']) is bool and row['input_unchanged'] == (row['input_before'] == row['input_after']), 'source mutation flag mismatch')
    same_arguments = row['args_before'] == row['args_after'] and row['kwargs_before'] == row['kwargs_after']
    _pandas_attributes_need(type(row['arguments_unchanged']) is bool and row['arguments_unchanged'] == same_arguments, 'argument mutation flag mismatch')
    _pandas_attributes_need(type(row['warnings']) is list, 'full warnings array absent')
    for warning in row['warnings']:
        _pandas_attributes_exact(warning, ['category', 'message', 'filename', 'lineno'], 'unknown/incomplete warning metadata')
        _pandas_attributes_need(all((type(warning[k]) is str for k in ['category', 'message', 'filename'])) and type(warning['lineno']) is int, 'warning metadata type mismatch')
    if success:
        _pandas_attributes_frame(row['result'], profile, tags)
        _pandas_attributes_need(type(row['returned_input_identity']) is bool, 'returned input identity absent')
        _pandas_attributes_need(not row['returned_input_identity'] or row['result'] == row['input_after'], 'returned input identity/result mismatch')
    else:
        _pandas_attributes_exact(row['error'], ['type', 'message', 'args'], 'unknown/incomplete full error')
        _pandas_attributes_need(type(row['error']['type']) is str and row['error']['type'] and (type(row['error']['message']) is str), 'full error type/message absent')
        _pandas_attributes_packed(row['error']['args'], profile, tags, expected='tuple')
    encoded = canonical(receipt)
    digest = sha(encoded)
    _pandas_attributes_need(type(row['receipt_sha256']) is str and re.fullmatch('[0-9a-f]{64}', row['receipt_sha256']) and (row['receipt_sha256'] == digest), 'original canonical receipt SHA mismatch')
    return


def adapt(row, spec):
    if not isinstance(row, dict):
        raise ValueError('record must be an object')
    adapter = spec.get('adapter')
    if adapter not in {*SCHEMAS, 'digest-plan-v1'}:
        raise ValueError('unknown receipt adapter')
    if spec.get('role') not in {'observations', 'plans', 'intents'}:
        raise ValueError('unknown call provenance role')
    if adapter == 'digest-plan-v1':
        if spec.get('receipt_schema') not in {value[0] for value in SCHEMAS.values()}:
            raise ValueError('unknown original receipt schema')
    elif spec.get('receipt_schema') != SCHEMAS[adapter][0]:
        raise ValueError('unknown original receipt schema for adapter')
    attributes_schema = spec.get('receipt_schema') == SCHEMAS['log-attributes-packed-v1'][0]
    if attributes_schema:
        if row.get('module', LOG_ATTRIBUTES_MODULE) != LOG_ATTRIBUTES_MODULE or row.get('function', row.get('method')) is not None and row.get('function', row.get('method')) not in LOG_ATTRIBUTES_ARITIES:
            raise ValueError('unreviewed LOG attributes intent/plan identity')
        if adapter == 'digest-plan-v1' and any(key in row for key in ['result_kind', 'result_value', 'output', 'return', 'returned', 'exception', 'apply_returned', 'input_after', 'controls_after']):
            raise ValueError('LOG attributes digest-only plan contains possible entry/outcome')
    pandas_attributes_schema = spec.get('receipt_schema') == SCHEMAS['pandas-attributes-before-v1'][0]
    pandas_attributes_full = any(key in row for key in ['receipt', 'input_before', 'args_before', 'kwargs_before', 'call_shape',
        'result', 'error', 'result_value', 'value_result', 'output', 'return', 'returned', 'exception', 'result_kind',
        'input_after', 'args_after', 'kwargs_after', 'apply_returned', 'returned_input_identity'])
    if pandas_attributes_schema:
        if row.get('module', PANDAS_ATTRIBUTES_PROFILE['module']) != PANDAS_ATTRIBUTES_PROFILE['module']:
            raise ValueError('pandas attributes module conflict')
        hints = [row[key] for key in ('method', 'function') if key in row]
        if any(type(value) is not str or value not in PANDAS_ATTRIBUTES_PROFILE['arities'] for value in hints) or len(set(hints)) > 1:
            raise ValueError('pandas attributes method identity conflict')
        if adapter == 'digest-plan-v1' and (pandas_attributes_full or set(row) - {'index', 'case', 'module', 'method', 'function', 'receipt_sha256'}):
            raise ValueError('pandas attributes digest-only plan contains full/unknown entry evidence')
        if spec.get('role') == 'intents' and not pandas_attributes_full and set(row) - {'index', 'case', 'module', 'method', 'function', 'receipt_sha256', 'entered'}:
            raise ValueError('pandas attributes lightweight intent has unknown fields')
    if adapter == 'digest-plan-v1' and any(key in row for key in ['result', 'error', 'value_result', 'receipt']):
        raise ValueError('digest-only plan contains full receipt or outcome; explicit full adapter required')
    if adapter == 'digest-plan-v1' or (spec.get('role') == 'intents' and 'receipt' not in row and not (pandas_attributes_schema and pandas_attributes_full)):
        if adapter == 'digest-plan-v1' and spec.get('role') != 'plans':
            raise ValueError('digest-plan adapter requires planned-only provenance')
        digest = row.get('receipt_sha256')
        if not re.fullmatch('[0-9a-f]{64}', str(digest)):
            raise ValueError('invalid digest-only receipt binding')
        return (row.get('module'), row.get('function', row.get('method')), None,
                digest, 'digest_only_plan' if adapter == 'digest-plan-v1' else 'entered_without_outcome')
    if adapter in {'receipt-v1', 'polars-receipt-v1', 'log-attributes-packed-v1'}:
        receipt = row.get('receipt')
    elif adapter in {'pandas-before-v1', 'pandas-attributes-before-v1'}:
        if 'receipt' in row:
            receipt = row['receipt']
        else:
            receipt = {key: row[source] for key, source in [
                ('module', 'module'), ('method', 'method'), ('input', 'input_before'),
                ('args', 'args_before'), ('kwargs', 'kwargs_before'),
                ('call_shape', 'call_shape')]}
    else:
        raise ValueError('unknown receipt adapter')
    if not isinstance(receipt, dict):
        raise ValueError('full original receipt absent; possible entry remains unresolved')
    attributes_source = _attributes_receipt(receipt) if adapter == 'log-attributes-packed-v1' else None
    if adapter == 'pandas-attributes-before-v1':
        _pandas_attributes_receipt(receipt, PANDAS_ATTRIBUTES_PROFILE, {})
    required = ['module', 'kwargs', 'call_shape']
    required += ['input', 'category_observables', 'input_kind'] if adapter == 'polars-receipt-v1' else ['args']
    for field in required:
        if field not in receipt:
            raise ValueError('incomplete receipt: ' + field)
    module = receipt['module']
    function = receipt.get('function', receipt.get('method'))
    if not isinstance(module, str) or not module.startswith('pm4py.'):
        raise ValueError('qualified public module absent')
    if not isinstance(function, str) or not function:
        raise ValueError('function absent')
    if not isinstance(receipt['call_shape'], str) or not receipt['call_shape']:
        raise ValueError('call shape absent')
    if receipt['call_shape'] != SCHEMAS[adapter][1]:
        raise ValueError('unsupported original call shape')
    if not isinstance(receipt['kwargs'], dict) or receipt['kwargs'].get('type') != 'dict' or not isinstance(receipt['kwargs'].get('items'), list):
        raise ValueError('complete typed keyword binding absent')
    if adapter != 'polars-receipt-v1':
        arguments = receipt['args']
        if not isinstance(arguments, dict) or arguments.get('type') != 'list' or not isinstance(arguments.get('values'), list):
            raise ValueError('complete typed positional binding absent')
    if adapter == 'receipt-v1':
        args = receipt['args']['values']
        if function.startswith('timestamp_list_'):
            if len(args) != 2:
                raise ValueError('helper positional operand count mismatch')
        else:
            if not args or not isinstance(args[0], dict) or args[0].get('type') != 'log':
                raise ValueError('complete log source operand absent')
            snapshot = args[0].get('snapshot')
            if not isinstance(snapshot, dict) or not {'attributes', 'extensions', 'classifiers', 'omni_present', 'properties', 'traces'} <= snapshot.keys():
                raise ValueError('complete log snapshot absent')
    elif adapter == 'pandas-before-v1':
        source = receipt['input']
        if not isinstance(source, dict) or not {'kind', 'columns', 'column_names', 'dtypes', 'categorical_dtypes', 'index', 'attrs', 'rows'} <= source.keys():
            raise ValueError('complete pandas snapshot absent')
    elif adapter == 'polars-receipt-v1':
        source = receipt['input']
        if not isinstance(source, dict) or not {'columns', 'dtypes', 'shape', 'rows'} <= source.keys() or receipt['input_kind'] not in {'DataFrame', 'LazyFrame'}:
            raise ValueError('complete Polars source snapshot/kind absent')
    for field, value in [('module', module), ('function', function), ('method', function)]:
        if field in row and row[field] != value:
            raise ValueError('outer and receipt identities disagree')
    raw = canonical(receipt)
    digest = sha(raw)
    if row.get('receipt_sha256') != digest:
        raise ValueError('full original receipt hash mismatch')
    if adapter == 'log-attributes-packed-v1':
        if any(key in row for key in ['result', 'error', 'value_result', 'result_value', 'result_kind', 'output', 'return', 'returned', 'exception']):
            _attributes_outcome(row, receipt, attributes_source)
        elif any(key in row for key in ['input_after', 'controls_after', 'apply_returned']):
            return module, function, raw, digest, 'entered_without_outcome'
    if adapter == 'pandas-attributes-before-v1':
        if any(key in row for key in ['result', 'error', 'value_result', 'result_value', 'result_kind', 'output', 'return', 'returned', 'exception']):
            _pandas_attributes_outcome(row, PANDAS_ATTRIBUTES_PROFILE)
        elif any(key in row for key in ['input_after', 'args_after', 'kwargs_after', 'apply_returned', 'returned_input_identity']):
            return module, function, raw, digest, 'entered_without_outcome'
        elif 'receipt' not in row or set(row) - {'receipt', 'receipt_sha256', 'index', 'case', 'module', 'method', 'function'}:
            raise ValueError('pandas attributes full plan requires embedded receipt and exact reviewed metadata fields')
    # Outcome FIRST: modern records have both full receipt and result/error.
    if any(key in row for key in ['result', 'error', 'value_result']):
        status = 'actual_outcome'
    elif spec.get('role') == 'intents' or row.get('entered') is True:
        status = 'entered_without_outcome'
    elif spec.get('role') == 'plans':
        status = 'planned_only'
    else:
        raise ValueError('reference call has no recognized terminal outcome or entry status')
    return module, function, raw, digest, status


class Index:
    def __init__(self, path):
        self.parser_sha256 = PARSER_SHA256
        self.db = sqlite3.connect(path)
        self.db.executescript('''
          CREATE TABLE IF NOT EXISTS containers (
            content_key TEXT PRIMARY KEY, sha256 TEXT, bytes INTEGER, config TEXT);
          CREATE TABLE IF NOT EXISTS members (
            content_key TEXT, path TEXT, sha256 TEXT, bytes INTEGER, disposition TEXT,
            PRIMARY KEY(content_key,path));
          CREATE TABLE IF NOT EXISTS observations (
            content_key TEXT, member TEXT, ordinal INTEGER, record_sha256 TEXT,
            module TEXT, function TEXT, receipt_schema TEXT, partition TEXT,
            receipt_sha256 TEXT, canonical BLOB, status TEXT,
            PRIMARY KEY(content_key,member,ordinal));
          CREATE TABLE IF NOT EXISTS gaps (
            content_key TEXT, member TEXT, ordinal INTEGER, module TEXT, reason TEXT);
          CREATE TABLE IF NOT EXISTS generations (
            id INTEGER PRIMARY KEY, manifest_sha256 TEXT, report TEXT);
          CREATE TABLE IF NOT EXISTS bindings (
            generation INTEGER, path TEXT, content_key TEXT);
        ''')

    def gap(self, key, member, ordinal, row, reason):
        hint = row.get('module') if isinstance(row, dict) else None
        if isinstance(row, dict) and isinstance(row.get('receipt'), dict):
            embedded = row['receipt'].get('module')
            if hint is not None and embedded is not None and hint != embedded:
                hint = None  # contradictory identities can affect either scope
            else:
                hint = embedded if embedded is not None else hint
        if not isinstance(hint, str) or not hint.startswith('pm4py.'):
            hint = None
        self.db.execute('INSERT INTO gaps VALUES(?,?,?,?,?)',
                        (key, member, ordinal, hint, reason))

    def ingest_records(self, key, member, data, spec):
        try:
            if spec.get('provenance') != 'own-public-observation':
                raise ValueError('unknown public-observer provenance')
            for field in ['reference', 'runtime', 'receipt_schema']:
                if not spec.get(field):
                    raise ValueError('missing declared ' + field)
            partition = canonical([spec['reference'], spec['runtime']]).decode()
            for ordinal, (row, raw) in enumerate(records(data, spec['format'], spec.get('records_key')), 1):
                try:
                    module, function, full, digest, status = adapt(row, spec)
                    self.db.execute('INSERT INTO observations VALUES(?,?,?,?,?,?,?,?,?,?,?)',
                        (key, member, ordinal, sha(raw), module, function,
                         spec['receipt_schema'], partition, digest, full, status))
                except (ValueError, KeyError, TypeError) as error:
                    gap_row = row
                    if spec['receipt_schema'] == SCHEMAS['pandas-attributes-before-v1'][0] and isinstance(row, dict):
                        # A legacy row can lack an embedded module. Conflicting
                        # outer identities must not certify this closed scope.
                        hints = [row['module']] if 'module' in row else []
                        if isinstance(row.get('receipt'), dict) and 'module' in row['receipt']:
                            hints.append(row['receipt']['module'])
                        if any(hint != PANDAS_ATTRIBUTES_PROFILE['module'] for hint in hints):
                            gap_row = None
                    self.gap(key, member, ordinal, gap_row, str(error))
        except (ValueError, KeyError, TypeError, UnicodeError) as error:
            self.gap(key, member, 0, None, 'member parsing: ' + str(error))

    def ingest(self, root, spec):
        config = canonical({'version': VERSION, 'parser_sha256': self.parser_sha256,
                            'spec': {k: v for k, v in spec.items() if k != 'path'}})
        key = sha(config)
        try:
            path = safe_path(spec['path'])
            target = (root / path).resolve()
            target.relative_to(root)
            if any(part.casefold() in EXCLUDED for part in target.parts):
                raise ValueError('resolved target lies under excluded reference/dependency path')
            data = target.read_bytes()
            bound(data, spec)
            if spec.get('provenance') != 'own-public-observation':
                raise ValueError('unknown container provenance')
            if spec.get('format') not in FORMATS:
                raise ValueError('unknown container format')
        except (ValueError, KeyError, TypeError, OSError) as error:
            # A failed current binding must never reuse a previously valid cache.
            self.gap('failed:' + key, str(spec.get('path')), 0, None, str(error))
            return 'failed:' + key, False
        if self.db.execute('SELECT 1 FROM containers WHERE content_key=?', (key,)).fetchone():
            return key, True
        self.db.execute('INSERT INTO containers VALUES(?,?,?,?)',
                        (key, sha(data), len(data), config.decode()))
        try:
            fmt = spec['format']
            if fmt.startswith('gzip-'):
                data = gzip.decompress(data)
                fmt = fmt[5:]
            if fmt.startswith('envelope-'):
                wanted = {}
                for member in spec.get('members', []):
                    name = safe_path(member['path'])
                    if name in wanted:
                        raise ValueError('duplicate member allowlist path')
                    if member.get('format') not in {'json', 'jsonl'}:
                        raise ValueError('nested or unknown member format')
                    wanted[name] = member
                seen = set()
                for row, _ in records(data, fmt[9:], spec.get('records_key')):
                    if not isinstance(row, dict) or not isinstance(row.get('path'), str):
                        raise ValueError('unknown archive envelope entry')
                    name = row['path']
                    if name in seen:
                        raise ValueError('duplicate archive member path')
                    seen.add(name)
                    if name not in wanted:
                        self.db.execute('INSERT INTO members VALUES(?,?,?,?,?)',
                            (key, name, row.get('sha256'), row.get('bytes'), 'unlisted-not-decoded'))
                        continue
                    safe_path(name)
                    if row.get('encoding') == 'base64' and 'utf8' not in row:
                        payload = base64.b64decode(row['data'], validate=True)
                    elif 'utf8' in row and 'encoding' not in row and 'data' not in row:
                        payload = row['utf8'].encode('utf-8')
                    else:
                        raise ValueError('unknown member encoding')
                    bound(payload, row)
                    bound(payload, wanted[name])
                    self.db.execute('INSERT INTO members VALUES(?,?,?,?,?)',
                        (key, name, sha(payload), len(payload), 'admitted'))
                    self.ingest_records(key, name, payload, wanted[name])
                if set(wanted) - seen:
                    raise ValueError('allowlisted archive members absent')
            else:
                self.ingest_records(key, '', data, {**spec, 'format': fmt})
        except (ValueError, KeyError, TypeError, OSError, UnicodeError, EOFError) as error:
            self.gap(key, '', 0, None, 'container parsing: ' + str(error))
        return key, False

    def run(self, manifest):
        if manifest.get('schema') != MANIFEST:
            raise ValueError('unknown manifest schema')
        root = Path(manifest['root']).resolve()
        if any(part.casefold() in EXCLUDED for part in root.parts):
            raise ValueError('inventory root lies under excluded reference/dependency path')
        modules = manifest['modules']
        if not modules or any(not isinstance(x, str) or not x.startswith('pm4py.') for x in modules):
            raise ValueError('explicit qualified module scope required')
        if not isinstance(manifest.get('scope'), str) or not manifest['scope'].strip():
            raise ValueError('bounded inventory scope description required')
        sources = manifest['sources']
        paths = [s['path'] for s in sources]
        if len(set(paths)) != len(paths):
            raise ValueError('duplicate source paths')
        with self.db:
            generation = self.db.execute('INSERT INTO generations(manifest_sha256) VALUES(?)',
                                         (sha(canonical(manifest)),)).lastrowid
            hits = 0
            for source in sources:
                key, cached = self.ingest(root, source)
                hits += cached
                self.db.execute('INSERT INTO bindings VALUES(?,?,?)', (generation, source['path'], key))
            # Concurrent captures may have changed an early source while later
            # containers were parsed. Revalidate every binding before publishing.
            for source in sources:
                try:
                    path = safe_path(source['path'])
                    target = (root / path).resolve()
                    target.relative_to(root)
                    if any(part.casefold() in EXCLUDED for part in target.parts):
                        raise ValueError('resolved target exclusion changed')
                    bound(target.read_bytes(), source)
                except (ValueError, KeyError, TypeError, OSError) as error:
                    self.gap('generation:' + str(generation), str(source.get('path')), 0, None,
                             'final binding revalidation: ' + str(error))
            if sha(Path(__file__).read_bytes()) != self.parser_sha256:
                self.gap('generation:' + str(generation), '', 0, None,
                         'executing parser file changed during generation')
            per_module = {}
            # Resolve lightweight intents only against exact validated full receipts
            # in the same original schema and reference/runtime partition. Never
            # assign a module from the filename or from a bare digest-only plan.
            observations = self.db.execute('SELECT module,function,receipt_schema,partition,receipt_sha256,canonical,status FROM observations').fetchall()
            parser_sha = self.parser_sha256
            configs = [load_json(row[0]) for row in self.db.execute('SELECT config FROM containers')]
            def base_config(config):
                return canonical({k: v for k, v in config.items() if k != 'parser_sha256'})
            current_configs = {base_config(config) for config in configs if config.get('parser_sha256') == parser_sha}
            stale_containers = sum(config.get('parser_sha256') != parser_sha and base_config(config) not in current_configs for config in configs)
            integrity_gaps = []
            for item in observations:
                if item[5] is not None:
                    try:
                        original = load_json(item[5])
                        if canonical(original) != item[5] or sha(item[5]) != item[4]:
                            raise ValueError('saved canonical receipt integrity mismatch')
                    except (ValueError, TypeError, UnicodeError):
                        integrity_gaps.append(item[0])
            terminals = {}
            for item in observations:
                if item[6] == 'actual_outcome':
                    terminals.setdefault(tuple(item[2:5]), []).append(item)
            unresolved = []
            resolved = {}
            for item in observations:
                if item[6] != 'entered_without_outcome':
                    continue
                matches = [x for x in terminals.get(tuple(item[2:5]), [])
                           if (item[0] is None or x[0] == item[0])
                           and (item[1] is None or x[1] == item[1])
                           and (item[5] is None or x[5] == item[5])]
                identities = {(x[0], x[1], x[5]) for x in matches}
                if len(identities) == 1:
                    module = matches[0][0]
                    resolved[module] = resolved.get(module, 0) + 1
                else:
                    unresolved.append(item[0])
            for module in modules:
                counts = dict(self.db.execute('SELECT status,COUNT(*) FROM observations WHERE module=? GROUP BY status', (module,)))
                gaps = self.db.execute('SELECT COUNT(*) FROM gaps WHERE module=? OR module IS NULL', (module,)).fetchone()[0]
                gaps += sum(hint is None or hint == module for hint in unresolved)
                gaps += stale_containers
                gaps += sum(hint is None or hint == module for hint in integrity_gaps)
                complete = manifest.get('inventory_complete') is True and gaps == 0
                unique_actual = len({tuple(x[2:6]) for x in observations if x[0] == module and x[6] == 'actual_outcome'})
                per_module[module] = {'historical_observation_copies': counts, 'blocking_gaps': gaps,
                    'distinct_actual_receipts': unique_actual, 'intents_with_terminal_outcome': resolved.get(module, 0),
                    'complete_within_declared_inventory': complete,
                    'no_prior_calls_within_declared_inventory': complete and not counts.get('actual_outcome') and not counts.get('entered_without_outcome') and not resolved.get(module)}
            report = {'schema': VERSION, 'generation': generation, 'scope': manifest['scope'],
                'manifest_sha256': sha(canonical(manifest)), 'containers': len(sources),
                'cache_hits': hits, 'per_module': per_module, 'catalogue_audit_complete': False,
                'parser_sha256': self.parser_sha256,
                'historical_containers_pending_parser_upgrade': stale_containers,
                'reference_calls': 0, 'reference_implementation_inspected': False}
            self.db.execute('UPDATE generations SET report=? WHERE id=?', (canonical(report).decode(), generation))
        return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('database', type=Path)
    parser.add_argument('report', type=Path)
    args = parser.parse_args()
    index = Index(args.database)
    try:
        result = index.run(load_json(args.manifest.read_bytes()))
        temporary = args.report.with_suffix(args.report.suffix + '.tmp')
        temporary.write_bytes(canonical(result) + b'\n')
        temporary.replace(args.report)
    finally:
        index.db.close()


if __name__ == '__main__':
    main()
