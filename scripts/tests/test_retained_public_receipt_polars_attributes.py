"""Synthetic Polars attributes observer integrity; no reference/runtime imports."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('polars_attributes_index', Path(__file__).parents[1] / 'retained_public_receipt_index.py')
M = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(M)
MODULE = 'pm4py.algo.filtering.polars.attributes.attributes_filter'
OTHER = 'pm4py.algo.filtering.polars.ltl.ltl_checker'
ADAPTER = 'polars-attributes-receipt-v1'


def scalar(value):
    if type(value) is float: return {'type': 'float', 'hex': value.hex()}
    return {'type': type(value).__name__, 'value': value}


def sequence(*values, kind='list'): return {'type': kind, 'values': list(values)}


def dictionary(*pairs): return {'type': 'dict', 'items': [list(pair) for pair in pairs]}


def frame(): return {'columns': ['x'], 'dtypes': ['String'], 'shape': [1, 1], 'rows': [sequence(scalar('A'), kind='tuple')]}


def spec(role='observations'):
    return {'adapter': ADAPTER, 'receipt_schema': M.SCHEMAS[ADAPTER][0], 'role': role,
            'reference': {'commit': 'synthetic-public-pin'}, 'runtime': {'polars': 'synthetic'}}


def rebind(row):
    row['receipt_sha256'] = M.sha(M.canonical(row['receipt']))
    return row


def row_for(function='apply', mode='frame', eager=False):
    defaults = {'apply': [('values', sequence(scalar('A')))], 'apply_events': [('values', sequence(scalar('A')))],
                'apply_numeric': [('int1', scalar(-0.0)), ('int2', scalar(1.0))],
                'apply_numeric_events': [('int1', scalar(-0.0)), ('int2', scalar(1.0))],
                'filter_df_keeping_activ_exc_thresh': [('thresh', scalar(1))],
                'filter_df_keeping_spno_activities': [('max_no_activities', scalar(2))],
                'filter_df_on_attribute_values': [('values', sequence(scalar('A')))],
                'filter_df_relative_occurrence_event_attribute': [('min_relative_stake', scalar(0.5))],
                'get_attribute_values': [('attribute_key', scalar('x'))]}
    receipt = {'module': MODULE, 'function': function, 'input': frame(), 'category_observables': [],
               'kwargs': dictionary(*[(scalar(k), v) for k, v in defaults[function]]),
               'call_shape': 'source_positional_all_options_keyword', 'input_kind': 'DataFrame' if eager else 'LazyFrame'}
    row = {'index': 1, 'module': MODULE, 'case': {'name': 'own synthetic', 'function': function, 'eager': eager, 'fixture': {}, 'kwargs': {}},
           'receipt': receipt, 'receipt_sha256': '', 'input_before': copy.deepcopy(receipt['input']),
           'input_categories_before': [], 'kwargs_before': copy.deepcopy(receipt['kwargs']),
           'input_after': copy.deepcopy(receipt['input']), 'input_categories_after': [], 'kwargs_after': copy.deepcopy(receipt['kwargs']),
           'input_values_unchanged': True, 'input_categories_unchanged': True, 'input_unchanged': True, 'kwargs_unchanged': True,
           'warnings': [], 'phase': 'complete', 'apply_returned': True}
    if mode == 'apply-error':
        row.update(phase='apply', apply_returned=False, error={'type': 'builtins.TypeError', 'message': 'full synthetic diagnostic', 'args': sequence(scalar('full synthetic diagnostic'), kind='tuple'), 'phase': 'apply'})
    else:
        row.update(result_type='builtins.dict' if function == 'get_attribute_values' else ('polars.dataframe.frame.DataFrame' if eager else 'polars.lazyframe.frame.LazyFrame'), returned_input_identity=False)
        if row['result_type'] == 'polars.lazyframe.frame.LazyFrame': row.update(public_unoptimized_plan='OWN public plan', public_plan_warnings=[])
        if mode == 'collect-error':
            row.update(phase='collect', error={'type': 'polars.exceptions.ComputeError', 'message': 'full collect diagnostic', 'args': sequence(scalar('full collect diagnostic'), kind='tuple'), 'phase': 'collect'})
        elif function == 'get_attribute_values': row.update(result_kind='scalar', result_value=dictionary((scalar('A'), scalar(1))))
        else: row.update(result_kind='frame', result=frame(), result_categories=[])
    return rebind(row)


def set_source(row, source, categories):
    row['receipt']['input'] = copy.deepcopy(source); row['receipt']['category_observables'] = copy.deepcopy(categories)
    row['input_before'] = copy.deepcopy(source); row['input_categories_before'] = copy.deepcopy(categories)
    row['input_after'] = copy.deepcopy(source); row['input_categories_after'] = copy.deepcopy(categories)
    if 'result' in row: row['result'] = copy.deepcopy(source); row['result_categories'] = copy.deepcopy(categories)
    return rebind(row)


def enum_source():
    values = [scalar('unused'), scalar('A')]
    series = {'name': 'x', 'dtype': 'String', 'length': 2, 'values': values}
    dtype = "Enum(categories=['unused', 'A'])"
    source = frame();source['dtypes'] = [dtype]
    return source, [{'column_index': 0, 'column': 'x', 'kind': 'Enum', 'dtype': dtype,
                     'get_categories': series, 'declared_categories': copy.deepcopy(series), 'observation_warnings': []}]


def manifest(root, sources, modules=None):
    return {'schema': M.MANIFEST, 'root': str(root), 'scope': 'own synthetic explicit flat files only', 'inventory_complete': True,
            'modules': modules or [MODULE], 'sources': sources}


def source(root, name, rows, role='observations', **changes):
    raw = b''.join(json.dumps(row, ensure_ascii=True).encode() + b'\r\n' for row in rows)
    (root / name).write_bytes(raw)
    return {**spec(role), 'path': name, 'bytes': len(raw), 'sha256': M.sha(raw), 'format': 'jsonl', 'provenance': 'own-public-observation', **changes}


class PolarsAttributesReceiptTests(unittest.TestCase):
    def reject(self, row, role='observations'):
        with self.assertRaises((ValueError, KeyError, TypeError)): M.adapt(row, spec(role))

    def test_original_seven_field_canonical_and_kind_are_preserved(self):
        row = row_for(); adapted = M.adapt(row, spec())
        self.assertEqual(adapted[2], M.canonical(row['receipt']))
        self.assertEqual(adapted[-1], 'actual_outcome')
        eager = row_for(eager=True)
        self.assertNotEqual(row['receipt_sha256'], eager['receipt_sha256'])
        self.assertEqual(M.adapt(eager, spec())[-1], 'actual_outcome')
        eager['receipt']['input_kind'] = 'LazyFrame'; self.reject(eager)

    def test_all_nine_functions_and_missing_required_keyword_errors(self):
        for function in M.POLARS_ATTRIBUTES_KEYWORDS:
            row = row_for(function)
            with self.subTest(function=function): self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
            failed = row_for(function, 'apply-error');failed['receipt']['kwargs'] = dictionary()
            failed['kwargs_before'] = dictionary();failed['kwargs_after'] = dictionary();rebind(failed)
            with self.subTest(missing=function): self.assertEqual(M.adapt(failed, spec())[-1], 'actual_outcome')

    def test_terminal_first_and_unknown_terminals_never_become_plans(self):
        for row in (row_for(), row_for(mode='apply-error'), row_for(mode='collect-error'), row_for('get_attribute_values')):
            for role in ('plans', 'intents'):
                self.assertEqual(M.adapt(row, spec(role))[-1], 'actual_outcome')
        good = row_for();plan = {'receipt': good['receipt'], 'receipt_sha256': good['receipt_sha256']}
        for marker in ('result_kind', 'result_value', 'error_type', 'output', 'return', 'returned', 'exception', 'value_result'):
            for role in ('plans', 'intents'):
                with self.subTest(marker=marker, role=role): self.reject({**plan, marker: True}, role)

    def test_typed_count_map_key_types_order_and_nan_observations(self):
        row = row_for('get_attribute_values')
        date = {'type': 'date', 'iso': '2026-10-22'}
        stamp = {'type': 'datetime', 'iso': '2026-10-22T01:02:03+00:00', 'fold': 0, 'offset_seconds': 0.0}
        row['result_value'] = dictionary(*[(key, scalar(1)) for key in (scalar(None), scalar(True), scalar(3), scalar(float('nan')), scalar('x'), date, stamp)])
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        row['result_value']['items'][0][1] = scalar(True);self.reject(row)
        row = row_for('get_attribute_values');row['result_value']['items'][0][0] = sequence(scalar('x'));self.reject(row)

    def test_apply_collect_error_full_args_and_phase(self):
        for mode in ('apply-error', 'collect-error'):
            row = row_for(mode=mode);self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
            row['error']['phase'] = 'complete';self.reject(row)
            row = row_for(mode=mode);row['error'].pop('args');self.reject(row)
        row = row_for(mode='collect-error');row['apply_returned'] = False;self.reject(row)

    def test_lazy_plan_error_warning_conditionals_are_complete(self):
        row = row_for(mode='collect-error');row.pop('public_unoptimized_plan')
        row['public_plan_error'] = {'type': 'polars.exceptions.ComputeError', 'message': 'own explain failure'}
        row['public_plan_warnings'] = [{'type': 'builtins.DeprecationWarning', 'message': 'own warning'}]
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        row['public_unoptimized_plan'] = 'also present';self.reject(row)
        row = row_for();row.pop('public_plan_warnings');self.reject(row)
        row = row_for(eager=True);row['public_plan_warnings'] = [];self.reject(row)

    def test_complete_enum_category_dictionary_and_unused_categories(self):
        source_frame, categories = enum_source();row = set_source(row_for(), source_frame, categories)
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        bad = copy.deepcopy(row);bad['receipt']['category_observables'][0]['declared_categories']['values'].pop();rebind(bad);self.reject(bad)
        bad = copy.deepcopy(row);bad['input_categories_after'] = [];bad['input_categories_unchanged'] = False;bad['input_unchanged'] = False;self.reject(bad)
        bad = copy.deepcopy(row);bad['result_categories'][0]['column_index'] = True;self.reject(bad)

    def test_named_categorical_context_dtype_and_order_must_agree(self):
        source_frame = frame();dtype = 'Categorical(Categories(name="own", namespace="synthetic", physical=pl.UInt32))';source_frame['dtypes'] = [dtype]
        series = {'name': 'x', 'dtype': 'String', 'length': 1, 'values': [scalar('A')]}
        category = {'column_index': 0, 'column': 'x', 'kind': 'Categorical', 'dtype': dtype, 'get_categories': series,
                    'context': {'name': 'own', 'namespace': 'synthetic', 'physical_dtype': 'UInt32', 'is_global': False}, 'ordering': 'lexical', 'observation_warnings': []}
        row = set_source(row_for(), source_frame, [category]);self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        for field, value in [('name', 'wrong'), ('physical_dtype', 'UInt64'), ('is_global', True)]:
            bad = copy.deepcopy(row);bad['receipt']['category_observables'][0]['context'][field] = value;rebind(bad)
            with self.subTest(field=field): self.reject(bad)

    def test_full_frame_dimensions_rows_dtypes_and_category_series(self):
        for mode in ('height', 'width', 'row_kind', 'dtype', 'unknown'):
            row = row_for();value = row['receipt']['input']
            if mode == 'height': value['shape'][0] = True
            if mode == 'width': value['dtypes'].append('String')
            if mode == 'row_kind': value['rows'][0]['type'] = 'list'
            if mode == 'dtype': value['dtypes'][0] = 'Object'
            if mode == 'unknown': value['schema'] = 'unreviewed'
            rebind(row)
            with self.subTest(mode=mode): self.reject(row)

    def test_datetime_date_float_and_nested_descriptor_grammar(self):
        row = row_for();stamp = {'type': 'datetime', 'iso': '2026-10-22T01:02:03.123456+00:00', 'fold': 1, 'offset_seconds': 0.0}
        row['receipt']['kwargs']['items'].append([scalar('parameters'), dictionary((scalar('own'), sequence(stamp, {'type': 'date', 'iso': '2026-10-22'}, scalar(-0.0))))])
        row['kwargs_before'] = copy.deepcopy(row['receipt']['kwargs']);row['kwargs_after'] = copy.deepcopy(row['receipt']['kwargs']);rebind(row)
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        row['receipt']['kwargs']['items'][-1][1]['items'][0][1]['values'][0]['offset_seconds'] = 3600.0;rebind(row);self.reject(row)

    def test_packed_unknowns_cross_family_and_bad_json_remain_gaps(self):
        for value in ({'type': 'bytes', 'hex': '00'}, {'type': 'pandas.NA'}, {'type': 'float', 'hex': '1.0'}, {'type': 'int', 'value': True}, {'type': 'str', 'value': 'x', 'extra': 1}):
            row = row_for();row['receipt']['kwargs']['items'][0][1] = value;rebind(row)
            with self.subTest(value=value): self.reject(row)
        for raw in (b'{"x":1,"x":2}', b'{"x":1e999}', b'{"x":NaN}'):
            with self.assertRaises(ValueError): M.load_json(raw)

    def test_enum_module_name_value_and_ordered_string_collision(self):
        row = row_for();enum = {'type': 'enum', 'module': MODULE, 'name': 'ATTRIBUTE_KEY', 'value': scalar('pm4py:param:attribute_key')}
        row['receipt']['kwargs']['items'].append([scalar('parameters'), dictionary((scalar('pm4py:param:attribute_key'), scalar('literal')), (enum, scalar('enum')))])
        row['kwargs_before'] = copy.deepcopy(row['receipt']['kwargs']);row['kwargs_after'] = copy.deepcopy(row['receipt']['kwargs']);rebind(row)
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        bad = copy.deepcopy(row);bad['receipt']['kwargs']['items'][-1][1]['items'][-1][0]['module'] += '.Parameters';rebind(bad);self.reject(bad)
        bad = copy.deepcopy(row);bad['receipt']['kwargs']['items'][-1][1]['items'][-1][0]['value'] = scalar('wrong');rebind(bad);self.reject(bad)

    def test_original_before_after_flags_and_returned_input_identity(self):
        for field in ('input_values_unchanged', 'input_categories_unchanged', 'input_unchanged', 'kwargs_unchanged'):
            row = row_for();row[field] = False
            with self.subTest(field=field): self.reject(row)
        row = row_for();row['returned_input_identity'] = True;self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        row['result']['rows'][0]['values'][0] = scalar('other');self.reject(row)
        row = row_for();row['input_before']['rows'][0]['values'][0] = scalar('tampered');self.reject(row)
        row = row_for();row['kwargs_after']['items'].reverse();row['kwargs_after']['items'].append([scalar('parameters'), dictionary()]);row['kwargs_unchanged'] = False;self.reject(row)

    def test_original_digest_positive_actual_index_and_case_kind(self):
        row = row_for();row['receipt_sha256'] = '0' * 64;self.reject(row)
        for index in (False, True, 0, -1, '1', 1.0):
            row = row_for();row['index'] = index
            with self.subTest(index=index): self.reject(row)
        row = row_for();row['case']['eager'] = True;self.reject(row)
        row = row_for();row['receipt']['extra'] = 'unknown';rebind(row);self.reject(row)

    def test_full_plans_entries_and_unknown_evidence_are_distinct(self):
        row = row_for();plan = {'case': 'own', 'receipt': row['receipt'], 'receipt_sha256': row['receipt_sha256']}
        self.assertEqual(M.adapt(plan, spec('plans'))[-1], 'planned_only')
        self.assertEqual(M.adapt({**plan, 'apply_returned': False}, spec('plans'))[-1], 'entered_without_outcome')
        for marker in ('controls_after', 'unknown_entry', 'error_type'):
            for role in ('plans', 'intents'):
                with self.subTest(marker=marker, role=role): self.reject({**plan, marker: True}, role)
        self.reject({**plan, 'input_before': row['input_before']}, 'plans')
        self.reject({**plan, 'input_after': True}, 'plans')
        self.reject({**plan, 'apply_returned': 'true'}, 'plans')
        self.assertEqual(M.adapt({**plan, 'index': 1}, spec('plans'))[-1], 'planned_only')
        for key, value in [('index', True), ('index', 0), ('index', '1'), ('case', {'apply_returned': True}), ('case', ''), ('case', False)]:
            with self.subTest(metadata=key, value=value): self.reject({**plan, key: value}, 'plans')
        self.reject({**plan, 'index': True, 'apply_returned': False}, 'plans')

    def test_light_intents_module_free_exact_metadata_and_digest_only_plans(self):
        row = row_for();intent = {'index': 1, 'case': 'own', 'function': 'apply', 'receipt_sha256': row['receipt_sha256']}
        adapted = M.adapt(intent, spec('intents'));self.assertIsNone(adapted[0]);self.assertIsNone(adapted[2])
        for marker in ('module', 'entered', 'input_after', 'unknown_entry'):
            with self.subTest(marker=marker): self.reject({**intent, marker: True}, 'intents')
        config = {**spec('plans'), 'adapter': 'digest-plan-v1'}
        self.assertEqual(M.adapt({'receipt_sha256': row['receipt_sha256']}, config)[-1], 'digest_only_plan')
        with self.assertRaises(ValueError): M.adapt({'receipt_sha256': row['receipt_sha256'], 'result_value': {}}, config)
        for key, value in [('index', False), ('index', -1), ('index', 1.0), ('case', {'apply_returned': True}), ('case', 1), ('case', '')]:
            with self.subTest(digest_metadata=key, value=value), self.assertRaises(ValueError):
                M.adapt({'receipt_sha256': row['receipt_sha256'], key: value}, config)

    def test_exact_raw_record_full_plan_and_resolved_intent_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory);row = row_for();intent = {'index': 1, 'case': 'own', 'function': 'apply', 'receipt_sha256': row['receipt_sha256']}
            plan = {'case': 'own', 'receipt': row['receipt'], 'receipt_sha256': row['receipt_sha256']}
            sources = [source(root, 'records.jsonl', [row]), source(root, 'intents.jsonl', [intent], 'intents'), source(root, 'plans.jsonl', [plan], 'plans')]
            index = M.Index(root / 'index.sqlite')
            try:
                first = index.run(manifest(root, sources));state = first['per_module'][MODULE]
                self.assertEqual(state['distinct_actual_receipts'], 1);self.assertEqual(state['intents_with_terminal_outcome'], 1);self.assertEqual(state['blocking_gaps'], 0)
                raw = (root / 'records.jsonl').read_bytes().splitlines(keepends=True)[0]
                stored = index.db.execute("SELECT canonical,record_sha256 FROM observations WHERE status='actual_outcome'").fetchone()
                self.assertEqual(stored, (M.canonical(row['receipt']), M.sha(raw)))
                self.assertEqual(index.run(manifest(root, sources))['cache_hits'], 3)
                (root / 'records.jsonl').write_bytes(b'{}\n')
                self.assertGreater(index.run(manifest(root, sources))['per_module'][MODULE]['blocking_gaps'], 0)
            finally: index.db.close()

    def test_unresolved_intent_never_guesses_module_or_partition(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory);row = row_for();intent = {'index': 1, 'case': 'own', 'function': 'apply', 'receipt_sha256': row['receipt_sha256']}
            one = source(root, 'records.jsonl', [row]);two = source(root, 'intent.jsonl', [intent], 'intents', runtime={'polars': 'other'})
            index = M.Index(root / 'index.sqlite')
            try:
                state = index.run(manifest(root, [one, two]))['per_module'][MODULE]
                self.assertGreater(state['blocking_gaps'], 0)
            finally: index.db.close()

    def test_cross_module_conflict_blocks_both_bounded_scopes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory);row = row_for();row['module'] = OTHER
            entry = source(root, 'conflict.jsonl', [row])
            index = M.Index(root / 'index.sqlite')
            try:
                report = index.run(manifest(root, [entry], [MODULE, OTHER]))
                for module in (MODULE, OTHER): self.assertGreater(report['per_module'][module]['blocking_gaps'], 0)
            finally: index.db.close()

    def test_closed_audit_wrapper_blocks_hidden_outer_entry_and_bad_counts(self):
        row = row_for();audit = {'planned': 1, 'prior_receipts_verified': 0, 'duplicates': [], 'prior_overlaps': [],
                                 'receipts': [{'case': 'own', 'receipt': row['receipt'], 'receipt_sha256': row['receipt_sha256']}]}
        for mode in ('valid', 'unknown_entry', 'count', 'duplicate', 'row_entry'):
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory);value = copy.deepcopy(audit)
                if mode == 'unknown_entry': value['controls_after'] = True
                if mode == 'count': value['planned'] = True
                if mode == 'duplicate': value['duplicates'] = [{'receipt_sha256': row['receipt_sha256']}]
                if mode == 'row_entry': value['receipts'][0]['apply_returned'] = True
                raw = json.dumps(value).encode();(root / 'audit.json').write_bytes(raw)
                entry = {**spec('plans'), 'path': 'audit.json', 'bytes': len(raw), 'sha256': M.sha(raw), 'format': 'json',
                         'records_key': 'receipts', 'provenance': 'own-public-observation'}
                index = M.Index(root / 'index.sqlite')
                try:
                    state = index.run(manifest(root, [entry]))['per_module'][MODULE]
                    with self.subTest(mode=mode): self.assertEqual(state['blocking_gaps'] == 0, mode == 'valid')
                finally: index.db.close()

    def test_closed_schema_module_function_keyword_and_shape(self):
        for mode in ('module', 'function', 'keyword', 'shape', 'kind'):
            row = row_for()
            if mode == 'module': row['receipt']['module'] = OTHER
            if mode == 'function': row['receipt']['function'] = 'unreviewed'
            if mode == 'keyword': row['receipt']['kwargs']['items'].append([scalar('unknown'), scalar(1)])
            if mode == 'shape': row['receipt']['call_shape'] = 'different'
            if mode == 'kind': row['receipt']['input_kind'] = 'UnknownFrame'
            rebind(row)
            with self.subTest(mode=mode): self.reject(row)
        config = spec();config['receipt_schema'] = M.SCHEMAS['polars-receipt-v1'][0]
        with self.assertRaises(ValueError): M.adapt(row_for(), config)


if __name__ == '__main__': unittest.main()
