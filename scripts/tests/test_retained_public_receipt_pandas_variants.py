"""Synthetic closed pandas variants receipts; no pandas/reference imports."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('receipt_index_pandas_variants', ROOT / 'scripts/retained_public_receipt_index.py')
M = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(M)
MODULE = 'pm4py.algo.filtering.pandas.variants.variants_filter'
ADAPTER = 'pandas-variants-before-v1'
SCHEMA = 'minera-pandas-variants-observer-receipt-v1'


def scalar(value):
    return {'type': type(value).__name__, 'value': value}


def sequence(*values, kind='list'):
    return {'type': kind, 'values': list(values)}


def dictionary(*pairs):
    return {'type': 'dict', 'items': [[key, value] for key, value in pairs]}


def frame():
    return {'kind': 'pandas.DataFrame', 'columns': sequence(scalar('value'), scalar('category')),
            'column_names': sequence(scalar(None)), 'dtypes': ['object', 'category'],
            'categorical_dtypes': [None, {'categories': sequence(scalar('unused'), scalar('a')), 'categories_dtype': 'object', 'ordered': True}],
            'index': {'kind': 'MultiIndex', 'dtype': 'object', 'names': sequence(scalar('outer'), scalar('inner')),
                      'labels': sequence(sequence(scalar('row'), scalar(2**64-1), kind='tuple'))},
            'attrs': dictionary((scalar('bytes'), {'type': 'bytes', 'hex': '00ff'})),
            'rows': [sequence(scalar(9007199254740993), scalar('a'))]}


def receipt(row):
    return {key: row[field] for key, field in [('module', 'module'), ('method', 'method'), ('input', 'input_before'),
                                               ('args', 'args_before'), ('kwargs', 'kwargs_before'), ('call_shape', 'call_shape')]}


def rebind(row):
    row['receipt_sha256'] = M.sha(M.canonical(receipt(row)))
    return row


def row_for(method='apply'):
    args = sequence(sequence(scalar('a'))) if method == 'apply' else sequence(scalar(2))
    value = {'index': 1, 'case': {'name': 'synthetic', 'method': method, 'shape': 'base', 'args': [], 'kwargs': {}},
             'module': MODULE, 'method': method, 'receipt_sha256': '', 'input_before': frame(), 'args_before': args,
             'kwargs_before': dictionary(), 'call_shape': 'dataframe-positional-plus-explicit-args-and-keywords',
             'warnings': [], 'input_after': frame(), 'args_after': copy.deepcopy(args), 'kwargs_after': dictionary(),
             'input_unchanged': True, 'arguments_unchanged': True, 'result': frame(), 'returned_input_identity': True}
    return rebind(value)


def parameters(row, entries=None, raw=None):
    parameter = dictionary(*(entries or [])) if raw is None else raw
    row['kwargs_before'] = dictionary((scalar('parameters'), parameter))
    row['kwargs_after'] = copy.deepcopy(row['kwargs_before'])
    return rebind(row)


def spec(role='observations'):
    return {'adapter': ADAPTER, 'receipt_schema': SCHEMA, 'role': role}


class PandasVariantsAdapterTests(unittest.TestCase):
    def test_exact_original_six_fields_without_embedded_receipt(self):
        row = row_for(); expected = M.canonical(receipt(row))
        self.assertNotIn('receipt', row)
        for role in ('observations', 'plans', 'intents'):
            self.assertEqual(M.adapt(row, spec(role)), (MODULE, 'apply', expected, M.sha(expected), 'actual_outcome'))

    def test_distinct_schema_module_method_and_observed_arity(self):
        for change in ('schema', 'module', 'method', 'arity', 'frame_kind'):
            row = row_for(); configuration = spec()
            if change == 'schema': configuration['receipt_schema'] = M.SCHEMAS['pandas-before-v1'][0]
            if change == 'module': row['module'] = 'pm4py.algo.filtering.pandas.ltl.ltl_checker'
            if change == 'method': row['method'] = 'unknown'
            if change == 'arity': row['args_before']['values'].append(scalar('extra'))
            if change == 'frame_kind': row['input_before']['kind'] = 'pandas.core.frame.DataFrame'
            rebind(row)
            with self.subTest(change=change), self.assertRaises(ValueError): M.adapt(row, configuration)

    def test_all_four_methods_one_operand_and_only_parameters_keyword(self):
        for method in M.PANDAS_VARIANTS_PROFILE['arities']:
            row = row_for(method)
            self.assertEqual(M.adapt(row, spec())[1], method)
            for operands in ([], [scalar(1), scalar(2)]):
                bad = copy.deepcopy(row); bad['args_before'] = sequence(*operands); rebind(bad)
                with self.subTest(method=method, operands=len(operands)), self.assertRaises(ValueError): M.adapt(bad, spec())
        for keyword in ('values', 'variants_df', 'keep_once_per_case', 'positive'):
            row = row_for(); row['kwargs_before'] = dictionary((scalar(keyword), scalar(True))); rebind(row)
            with self.subTest(keyword=keyword), self.assertRaises(ValueError): M.adapt(row, spec())

    def test_raw_none_na_and_ordered_enum_string_parameters_unchanged(self):
        enum = {'type': 'enum', 'module': MODULE, 'name': 'ACTIVITY_KEY', 'value': scalar('pm4py:param:activity_key')}
        for raw in ({'type': 'pandas.NA'}, scalar(None), scalar(False), scalar('raw'), sequence(scalar(1)), dictionary()):
            row = parameters(row_for(), raw=raw)
            self.assertEqual(M.adapt(row, spec())[2], M.canonical(receipt(row)))
        row = parameters(row_for(), entries=[(enum, scalar('value')), (scalar('pm4py:param:activity_key'), scalar('category'))])
        expected = M.canonical(receipt(row))
        self.assertEqual(M.adapt(row, spec())[2], expected)
        row['kwargs_before']['items'][0][1]['items'].reverse(); rebind(row)
        self.assertNotEqual(M.canonical(receipt(row)), expected)

    def test_enum_module_name_value_identity_is_closed(self):
        for field, bad in [('module', 'pm4py.other'), ('name', 'UNKNOWN'), ('value', scalar('wrong'))]:
            enum = {'type': 'enum', 'module': MODULE, 'name': 'POSITIVE', 'value': scalar('positive')}
            enum[field] = bad; row = parameters(row_for(), entries=[(enum, scalar(True))])
            with self.subTest(field=field), self.assertRaises(ValueError): M.adapt(row, spec())

    def test_nanosecond_timestamp_special_bytes_and_nested_frame_stay_exact(self):
        values = [{'type': 'pandas.Timestamp', 'iso': '2026-06-02T00:00:00.000000001+00:00', 'resolution': 'ns', 'timezone': 'UTC'},
                  {'type': 'pandas.NA'}, {'type': 'pandas.NaT'}, {'type': 'float', 'hex': 'nan'},
                  {'type': 'float', 'hex': '-0x0.0p+0'}, {'type': 'bytes', 'hex': '00ff'},
                  {'type': 'pandas.DataFrame', 'frame': frame()}]
        for value in values:
            row = parameters(row_for(), raw=value)
            self.assertEqual(M.adapt(row, spec())[2], M.canonical(receipt(row)))

    def test_unknown_nested_descriptors_and_timestamp_metadata_gap(self):
        for value in [{'type': 'unknown'}, {'type': 'int', 'value': True}, {'type': 'bytes', 'hex': 'f'},
                      {'type': 'pandas.NaT', 'value': None}, {'type': 'float', 'hex': '0x1p999999'},
                      {'type': 'pandas.Timestamp', 'iso': '2026-01-01', 'resolution': 'guess', 'timezone': None}]:
            with self.subTest(kind=value['type']), self.assertRaises(ValueError): M.adapt(parameters(row_for(), raw=value), spec())

    def test_full_frame_cardinality_index_attrs_and_categories(self):
        for change in ('index', 'width', 'dtype', 'category', 'attrs', 'index_name', 'unknown'):
            row = row_for(); source = row['input_before']
            if change == 'index': source['index']['labels']['values'] = []
            if change == 'width': source['rows'][0]['values'].pop()
            if change == 'dtype': source['dtypes'].pop()
            if change == 'category': source['categorical_dtypes'][1] = None
            if change == 'attrs': source['attrs'] = {}
            if change == 'index_name': source['index']['names']['values'].pop()
            if change == 'unknown': source['undocumented'] = True
            rebind(row)
            with self.subTest(change=change), self.assertRaises(ValueError): M.adapt(row, spec())

    def test_original_digest_no_reconstruction_from_cases_or_after(self):
        row = row_for(); row['input_before']['rows'][0]['values'][0]['value'] += 1
        with self.assertRaises(ValueError): M.adapt(row, spec())

    def test_actual_observer_index_requires_positive_integer(self):
        for index in (True, False, '1', 0, -1, 1.0):
            row = row_for(); row['index'] = index
            with self.subTest(index=index), self.assertRaises(ValueError): M.adapt(row, spec())

    def test_source_argument_mutation_flags_and_outer_shape(self):
        for change in ('source_flag', 'argument_flag', 'outer_args', 'outer_kwargs'):
            row = row_for()
            if change == 'source_flag': row['input_after']['attrs']['items'].append([scalar('changed'), scalar(True)])
            if change == 'argument_flag': row['args_after']['values'][0]['values'].append(scalar('b'))
            if change == 'outer_args': row['args_after']['values'].append(scalar('b')); row['arguments_unchanged'] = False
            if change == 'outer_kwargs': row['kwargs_after']['items'].append([scalar('parameters'), dictionary()]); row['arguments_unchanged'] = False
            with self.subTest(change=change), self.assertRaises(ValueError): M.adapt(row, spec())

    def test_returned_identity_requires_exact_source_after(self):
        row = row_for(); row['result']['attrs']['items'].append([scalar('different'), scalar(True)])
        with self.assertRaises(ValueError): M.adapt(row, spec())
        row['returned_input_identity'] = False
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')

    def test_full_error_args_warning_location_and_after_state(self):
        row = row_for(); row.pop('result'); row.pop('returned_input_identity')
        row['error'] = {'type': 'builtins.TypeError', 'message': 'complete synthetic diagnostic', 'args': sequence(scalar('complete synthetic diagnostic'), kind='tuple')}
        row['warnings'] = [{'category': 'builtins.UserWarning', 'message': 'synthetic', 'filename': 'observer.py', 'lineno': 7}]
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        for missing in ('error_args', 'warning_location', 'after'):
            bad = copy.deepcopy(row)
            if missing == 'error_args': bad['error'].pop('args')
            if missing == 'warning_location': bad['warnings'][0].pop('lineno')
            if missing == 'after': bad.pop('input_after')
            with self.subTest(missing=missing), self.assertRaises(ValueError): M.adapt(bad, spec())

    def test_mixed_and_unknown_terminal_records_cannot_be_plans_or_intents(self):
        for marker in ('error', 'value_result', 'result_value', 'output', 'return', 'returned', 'exception'):
            row = row_for(); row[marker] = True
            for role in ('plans', 'intents'):
                with self.subTest(marker=marker, role=role), self.assertRaises(ValueError): M.adapt(row, spec(role))

    def test_full_plan_and_after_state_remain_separate_history(self):
        original = row_for(); plan = {'index': 1, 'case': 'synthetic', 'receipt': receipt(original), 'receipt_sha256': original['receipt_sha256']}
        self.assertEqual(M.adapt(plan, spec('plans'))[-1], 'planned_only')
        for key in ('input_after', 'args_after', 'kwargs_after', 'apply_returned'):
            candidate = {**plan, key: (copy.deepcopy(original[key]) if key in original else True)}
            self.assertEqual(M.adapt(candidate, spec('plans'))[-1], 'entered_without_outcome')
        for marker in ('controls_after', 'error_type', 'unknown_entry'):
            for role in ('plans', 'intents', 'observations'):
                with self.subTest(marker=marker, role=role), self.assertRaises(ValueError):
                    M.adapt({**plan, marker: True}, spec(role))
        legacy_before_only = {key: value for key, value in original.items()
                              if key not in {'result', 'input_after', 'args_after', 'kwargs_after', 'returned_input_identity'}}
        for role in ('plans', 'intents', 'observations'):
            with self.subTest(legacy_before_only=role), self.assertRaises(ValueError):
                M.adapt(legacy_before_only, spec(role))

    def test_digest_plans_and_unknown_light_intents_do_not_hide_outcomes(self):
        digest = row_for()['receipt_sha256']; config = {**spec('plans'), 'adapter': 'digest-plan-v1'}
        self.assertEqual(M.adapt({'receipt_sha256': digest, 'case': 'synthetic'}, config)[-1], 'digest_only_plan')
        for marker in ('args_after', 'kwargs_after', 'result', 'receipt', 'unknown_entry'):
            with self.subTest(marker=marker), self.assertRaises(ValueError): M.adapt({'receipt_sha256': digest, marker: True}, config)
        with self.assertRaises(ValueError): M.adapt({'receipt_sha256': digest, 'unknown_entry': True}, spec('intents'))

    def test_closed_full_plan_and_intent_metadata_types_and_digest(self):
        row = row_for()
        plan = {'index': 1, 'case': 'synthetic', 'receipt': receipt(row), 'receipt_sha256': row['receipt_sha256']}
        intent = {'index': 1, 'case': 'synthetic', 'module': MODULE, 'method': 'apply', 'receipt_sha256': row['receipt_sha256']}
        self.assertEqual(M.adapt(intent, spec('intents'))[-1], 'entered_without_outcome')
        for key, value in [('index', True), ('index', 0), ('index', '1'), ('case', {'apply_returned': True}), ('case', '')]:
            for original, role in ((plan, 'plans'), (intent, 'intents')):
                bad = copy.deepcopy(original); bad[key] = value
                with self.subTest(key=key, value=value, role=role), self.assertRaises(ValueError): M.adapt(bad, spec(role))
        bad = copy.deepcopy(plan); bad['receipt']['args']['values'] = sequence(scalar(True))['values']
        with self.assertRaises(ValueError): M.adapt(bad, spec('plans'))
        for marker in ('controls_after', 'error_type', 'unexpected', 'module'):
            with self.subTest(marker=marker), self.assertRaises(ValueError): M.adapt({**plan, marker: True}, spec('plans'))

    def test_complete_digest_audit_links_counts_and_unknown_wrapper_fields(self):
        digest = row_for()['receipt_sha256']
        audit = {'candidates': 2, 'prior_rows_read': 878, 'removed_duplicates': [{'first': 'synthetic', 'removed': 'duplicate', 'receipt_sha256': digest}],
                 'removed_prior_overlaps': [], 'receipts': [{'case': 'synthetic', 'receipt_sha256': digest}]}
        config = {**spec('plans'), 'adapter': 'digest-plan-v1', 'format': 'json', 'records_key': 'receipts'}
        M._pandas_variants_audit(M.canonical(audit), config)
        for change in ('count', 'bool_prior', 'duplicate_name', 'duplicate_digest', 'target', 'removed_name', 'unknown_wrapper', 'unknown_row', 'entry_wrapper', 'prior_overlap'):
            bad = copy.deepcopy(audit)
            if change == 'count': bad['candidates'] = 1
            if change == 'bool_prior': bad['prior_rows_read'] = True
            if change == 'duplicate_name': bad['receipts'].append(copy.deepcopy(bad['receipts'][0])); bad['candidates'] = 3
            if change == 'duplicate_digest': bad['receipts'].append({'case': 'another', 'receipt_sha256': digest}); bad['candidates'] = 3
            if change == 'target': bad['removed_duplicates'][0]['first'] = 'missing'
            if change == 'removed_name': bad['removed_duplicates'][0]['removed'] = 'synthetic'
            if change == 'unknown_wrapper': bad['unexpected'] = True
            if change == 'unknown_row': bad['receipts'][0]['apply_returned'] = True
            if change == 'entry_wrapper': bad['controls_after'] = {}
            if change == 'prior_overlap': bad['removed_prior_overlaps'] = [{'receipt_sha256': digest}]
            with self.subTest(change=change), self.assertRaises(ValueError): M._pandas_variants_audit(M.canonical(bad), config)

    def test_actual_outcomes_precede_even_digest_plan_provenance(self):
        row = row_for(); config = {**spec('plans'), 'adapter': 'digest-plan-v1'}
        self.assertEqual(M.adapt(row, config)[-1], 'actual_outcome')
        row.pop('result'); row.pop('returned_input_identity')
        row['error'] = {'type': 'builtins.TypeError', 'message': 'synthetic full diagnostic', 'args': sequence(scalar('synthetic full diagnostic'), kind='tuple')}
        self.assertEqual(M.adapt(row, config)[-1], 'actual_outcome')

    def test_nested_supplied_frame_mutation_and_ordered_parameter_collision(self):
        enum = {'type': 'enum', 'module': MODULE, 'name': 'ACTIVITY_KEY', 'value': scalar('pm4py:param:activity_key')}
        row = parameters(row_for(), entries=[(enum, scalar('value')), (scalar('pm4py:param:activity_key'), scalar('category')),
                                             (scalar('variants_df'), {'type': 'pandas.DataFrame', 'frame': frame()})])
        before = M.canonical(receipt(row))
        row['kwargs_after']['items'][0][1]['items'][2][1]['frame']['rows'][0]['values'][0] = scalar(1)
        with self.assertRaises(ValueError): M.adapt(row, spec())
        row['arguments_unchanged'] = False
        self.assertEqual(M.adapt(row, spec())[2], before)
        row['kwargs_before']['items'][0][1]['items'].reverse(); rebind(row)
        self.assertNotEqual(M.adapt(row, spec())[2], before)

    def test_intent_resolution_does_not_cross_runtime_partitions(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); row = row_for()
            raw = M.canonical(row) + b'\n'
            intent = M.canonical({'index': 1, 'case': 'synthetic', 'module': MODULE, 'method': 'apply', 'receipt_sha256': row['receipt_sha256']}) + b'\n'
            sources = []
            for name, data, role, runtime in [('records.jsonl', raw, 'observations', 'one'), ('intents.jsonl', intent, 'intents', 'two')]:
                (root / name).write_bytes(data)
                sources.append({**spec(role), 'path': name, 'bytes': len(data), 'sha256': M.sha(data), 'format': 'jsonl',
                                'provenance': 'own-public-observation', 'reference': 'synthetic', 'runtime': runtime})
            manifest = {'schema': M.MANIFEST, 'root': str(root), 'scope': 'synthetic two partitions', 'inventory_complete': True, 'modules': [MODULE], 'sources': sources}
            index = M.Index(root / 'index.sqlite')
            try:
                result = index.run(manifest)
                self.assertEqual(result['per_module'][MODULE]['intents_with_terminal_outcome'], 0)
                self.assertFalse(result['per_module'][MODULE]['complete_within_declared_inventory'])
            finally: index.db.close()

    def test_index_exact_record_hash_intent_partition_and_rehashed_cache(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); row = row_for(); raw = json.dumps(row).encode() + b'\r\n'
            intent = M.canonical({'index': 1, 'case': 'synthetic', 'module': MODULE, 'method': 'apply', 'receipt_sha256': row['receipt_sha256']}) + b'\n'
            plans = M.canonical({'index': 1, 'case': 'synthetic', 'receipt': receipt(row), 'receipt_sha256': row['receipt_sha256']}) + b'\n'
            audit = M.canonical({'candidates': 1, 'prior_rows_read': 0, 'removed_duplicates': [], 'removed_prior_overlaps': [],
                                 'receipts': [{'case': 'synthetic', 'receipt_sha256': row['receipt_sha256']}]})
            sources = []
            for name, content, role in [('records.jsonl', raw, 'observations'), ('intents.jsonl', intent, 'intents'), ('plans.jsonl', plans, 'plans'), ('audit.json', audit, 'plans')]:
                (root / name).write_bytes(content)
                sources.append({**spec(role), 'path': name, 'bytes': len(content), 'sha256': M.sha(content), 'format': 'jsonl',
                                'provenance': 'own-public-observation', 'reference': {'commit': 'synthetic'}, 'runtime': {'python': 'synthetic'}})
                if name == 'audit.json': sources[-1].update(adapter='digest-plan-v1', format='json', records_key='receipts')
            manifest = {'schema': M.MANIFEST, 'root': str(root), 'scope': 'synthetic pandas variants only', 'inventory_complete': True, 'modules': [MODULE], 'sources': sources}
            index = M.Index(root / 'index.sqlite')
            try:
                first = index.run(manifest); stats = first['per_module'][MODULE]
                self.assertEqual(stats['distinct_actual_receipts'], 1)
                self.assertEqual(stats['intents_with_terminal_outcome'], 1)
                self.assertEqual(stats['blocking_gaps'], 0)
                self.assertFalse(stats['no_prior_calls_within_declared_inventory'])
                self.assertFalse(first['catalogue_audit_complete'])
                self.assertEqual(index.db.execute("SELECT record_sha256 FROM observations WHERE status='actual_outcome'").fetchone()[0], M.sha(raw))
                self.assertEqual(index.run(manifest)['cache_hits'], 4)
                self.assertEqual(dict(index.db.execute('SELECT status,COUNT(*) FROM observations GROUP BY status')),
                                 {'actual_outcome': 1, 'planned_only': 1, 'entered_without_outcome': 1, 'digest_only_plan': 1})
            finally: index.db.close()

    def test_conflicting_legacy_or_embedded_module_blocks_both_scopes(self):
        other = 'pm4py.algo.filtering.pandas.ltl.ltl_checker'
        for embedded in (False, True):
            with tempfile.TemporaryDirectory() as folder:
                root = Path(folder); row = row_for()
                if embedded:
                    full = receipt(row); full['module'] = other
                    row = {'receipt': full, 'receipt_sha256': M.sha(M.canonical(full))}
                else: row['module'] = other; rebind(row)
                raw = M.canonical(row); (root / 'bad.jsonl').write_bytes(raw)
                source = {**spec('plans'), 'path': 'bad.jsonl', 'bytes': len(raw), 'sha256': M.sha(raw), 'format': 'jsonl',
                          'provenance': 'own-public-observation', 'reference': 'synthetic', 'runtime': 'synthetic'}
                manifest = {'schema': M.MANIFEST, 'root': str(root), 'scope': 'synthetic identity conflict', 'inventory_complete': True, 'modules': [MODULE, other], 'sources': [source]}
                index = M.Index(root / 'index.sqlite')
                try:
                    result = index.run(manifest)
                    for module in (MODULE, other):
                        self.assertFalse(result['per_module'][module]['complete_within_declared_inventory'])
                        self.assertFalse(result['per_module'][module]['no_prior_calls_within_declared_inventory'])
                finally: index.db.close()


if __name__ == '__main__':
    unittest.main()
