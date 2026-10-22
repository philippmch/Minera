"""Synthetic Polars variants receipt integrity; no Polars/reference imports."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('polars_variants_receipt_index', Path(__file__).parents[1] / 'retained_public_receipt_index.py')
M = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(M)
MODULE = 'pm4py.algo.filtering.polars.variants.variants_filter'
ADAPTER = 'polars-variants-receipt-v1'
SCHEMA = 'minera-polars-variants-observer-receipt-v1'


def scalar(value):
    return {'type': 'float', 'hex': value.hex()} if type(value) is float else {'type': type(value).__name__, 'value': value}


def sequence(*values, kind='list'):
    return {'type': kind, 'values': list(values)}


def dictionary(*pairs):
    return {'type': 'dict', 'items': [list(pair) for pair in pairs]}


def frame():
    return {'columns': ['', 'wide'], 'dtypes': ['String', 'UInt64'], 'shape': [1, 2],
            'rows': [sequence(scalar('A\0Ω'), scalar(2**64 - 1), kind='tuple')]}


def spec(role='observations'):
    return {'adapter': ADAPTER, 'receipt_schema': SCHEMA, 'role': role,
            'provenance': 'own-public-observation', 'reference': {'commit': 'synthetic'}, 'runtime': {'polars': 'synthetic'}}


def rebind(row):
    row['receipt_sha256'] = M.sha(M.canonical(row['receipt']))
    return row


def row_for(function='apply', mode='frame', eager=False):
    operands = {'apply': [('admitted_variants', sequence(sequence(scalar('A\0Ω'))))],
                'apply_from_variant_list': [('variants', sequence(scalar('A\0Ω')))],
                'filter_variants_top_k': [('k', scalar(1))], 'apply_auto_filter': []}
    receipt = {'module': MODULE, 'function': function, 'input': frame(), 'category_observables': [],
               'kwargs': dictionary(*[(scalar(key), value) for key, value in operands[function]]),
               'call_shape': 'source_positional_all_options_keyword', 'input_kind': 'DataFrame' if eager else 'LazyFrame'}
    row = {'index': 1, 'module': MODULE, 'case': {'name': 'own synthetic', 'function': function, 'fixture': {}, 'kwargs': {}, 'eager': eager},
           'receipt': receipt, 'receipt_sha256': '', 'input_before': copy.deepcopy(receipt['input']),
           'input_categories_before': [], 'kwargs_before': copy.deepcopy(receipt['kwargs']),
           'input_after': copy.deepcopy(receipt['input']), 'input_categories_after': [], 'kwargs_after': copy.deepcopy(receipt['kwargs']),
           'input_values_unchanged': True, 'input_categories_unchanged': True, 'input_unchanged': True, 'kwargs_unchanged': True,
           'warnings': [], 'phase': 'complete', 'apply_returned': True}
    if mode == 'apply-error':
        row.update(phase='apply', apply_returned=False,
                   error={'type': 'builtins.TypeError', 'message': '', 'args': sequence(scalar('full\0Ω'), scalar(9007199254740993), kind='tuple'), 'phase': 'apply'})
    else:
        row.update(result_type='polars.dataframe.frame.DataFrame' if eager else 'polars.lazyframe.frame.LazyFrame', returned_input_identity=False)
        if not eager:
            row.update(public_unoptimized_plan='OWN complete public plan\nEND', public_plan_warnings=[])
        if mode == 'collect-error':
            row.update(phase='collect', error={'type': 'polars.exceptions.ComputeError', 'message': 'full\ncollect diagnostic',
                                              'args': sequence(scalar('full\ncollect diagnostic'), kind='tuple'), 'phase': 'collect'})
        else:
            row.update(result_kind='frame', result=frame(), result_categories=[])
    return rebind(row)


def plan(row):
    return {'case': row['case']['name'], 'receipt': copy.deepcopy(row['receipt']), 'receipt_sha256': row['receipt_sha256']}


def intent(row):
    return {'index': row['index'], 'function': row['receipt']['function'], 'case': row['case']['name'], 'receipt_sha256': row['receipt_sha256']}


def categories(row, kind='Categorical'):
    row = copy.deepcopy(row)
    dtype = 'Categorical(Categories(name="own", namespace="Ω", physical=pl.UInt32))' if kind == 'Categorical' else "Enum(categories=['unused', 'A\\x00Ω'])"
    source = frame(); source['dtypes'][0] = dtype
    series = {'name': '', 'dtype': 'String', 'length': 2, 'values': [scalar('unused'), scalar('A\0Ω')]}
    category = {'column_index': 0, 'column': '', 'kind': kind, 'dtype': dtype, 'get_categories': copy.deepcopy(series),
                'observation_warnings': [{'type': 'builtins.DeprecationWarning', 'message': 'complete observer warning'}]}
    if kind == 'Enum':
        category['declared_categories'] = copy.deepcopy(series)
    else:
        category.update(context={'name': 'own', 'namespace': 'Ω', 'physical_dtype': 'UInt32', 'is_global': False}, ordering='lexical')
    row['receipt']['input'] = copy.deepcopy(source); row['receipt']['category_observables'] = [copy.deepcopy(category)]
    for name in ('input_before', 'input_after', 'result'):
        if name in row: row[name] = copy.deepcopy(source)
    for name in ('input_categories_before', 'input_categories_after', 'result_categories'):
        if name in row: row[name] = [copy.deepcopy(category)]
    return rebind(row)


class PolarsVariantsAdapterTests(unittest.TestCase):
    def test_all_four_methods_eager_lazy_and_exact_seven_field_canonical(self):
        for function in M.POLARS_VARIANTS_KEYWORDS:
            for eager in (False, True):
                row = row_for(function, eager=eager)
                value = M.adapt(row, spec())
                self.assertEqual(value, (MODULE, function, M.canonical(row['receipt']), row['receipt_sha256'], 'actual_outcome'))
                self.assertEqual(set(row['receipt']), {'module', 'function', 'input', 'category_observables', 'kwargs', 'call_shape', 'input_kind'})

    def test_complete_apply_and_collect_error_metadata(self):
        for mode in ('apply-error', 'collect-error'):
            row = row_for(mode=mode)
            self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
            for missing in ('type', 'message', 'args', 'phase'):
                bad = copy.deepcopy(row); del bad['error'][missing]
                with self.subTest(mode=mode, missing=missing), self.assertRaises(ValueError): M.adapt(bad, spec())

    def test_error_apply_returned_and_phase_are_consistent(self):
        for change in ('phase', 'error_phase', 'return_flag', 'unqualified', 'nonlazy'):
            row = row_for(mode='collect-error')
            if change == 'phase': row['phase'] = 'apply'
            if change == 'error_phase': row['error']['phase'] = 'apply'
            if change == 'return_flag': row['apply_returned'] = False
            if change == 'unqualified': row['error']['type'] = 'ComputeError'
            if change == 'nonlazy': row['result_type'] = 'polars.dataframe.frame.DataFrame'
            with self.subTest(change=change), self.assertRaises(ValueError): M.adapt(row, spec())

    def test_full_public_plan_observer_error_and_warnings(self):
        row = row_for(); del row['public_unoptimized_plan']
        row['public_plan_error'] = {'type': 'polars.exceptions.ComputeError', 'message': 'own observer error\nend'}
        row['public_plan_warnings'] = [{'type': 'builtins.UserWarning', 'message': 'plan warning'}]
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        for change in ('both', 'no_warnings', 'unknown_error_field', 'partial_warning'):
            bad = copy.deepcopy(row)
            if change == 'both': bad['public_unoptimized_plan'] = 'plan'
            if change == 'no_warnings': del bad['public_plan_warnings']
            if change == 'unknown_error_field': bad['public_plan_error']['args'] = []
            if change == 'partial_warning': del bad['public_plan_warnings'][0]['message']
            with self.subTest(change=change), self.assertRaises(ValueError): M.adapt(bad, spec())

    def test_eager_has_no_lazy_plan_observer(self):
        row = row_for(eager=True); row['public_plan_warnings'] = []
        with self.assertRaises(ValueError): M.adapt(row, spec())

    def test_unknown_terminal_and_observation_fields_block(self):
        for field in ('result_value', 'value_result', 'output', 'return', 'returned', 'exception', 'unreviewed'):
            row = row_for(); row[field] = None
            with self.subTest(field=field), self.assertRaises(ValueError): M.adapt(row, spec())

    def test_missing_terminal_never_becomes_planned_or_actual(self):
        row = row_for(); del row['result']; del row['result_categories']; del row['result_kind']
        self.assertEqual(M.adapt(row, spec())[-1], 'entered_without_outcome')
        row['unreviewed_result'] = {}
        with self.assertRaises(ValueError): M.adapt(row, spec())

    def test_full_plan_and_light_intent_remain_distinct(self):
        row = row_for()
        self.assertEqual(M.adapt(plan(row), spec('plans'))[-1], 'planned_only')
        self.assertEqual(M.adapt(intent(row), spec('intents')), (MODULE, 'apply', None, row['receipt_sha256'], 'entered_without_outcome'))
        self.assertEqual(M.adapt(plan(row), spec('intents'))[-1], 'entered_without_outcome')
        with self.assertRaises(ValueError): M.adapt(plan(row), spec('observations'))

    def test_module_free_intent_requires_exact_closed_provenance(self):
        row = intent(row_for())
        for field in ('provenance', 'reference', 'runtime'):
            config = spec('intents'); config.pop(field)
            with self.subTest(field=field), self.assertRaises(ValueError): M.adapt(row, config)
        for field in ('module', 'entered', 'input_before', 'result', 'phase'):
            bad = dict(row); bad[field] = MODULE
            with self.subTest(field=field), self.assertRaises(ValueError): M.adapt(bad, spec('intents'))
        with self.assertRaises(ValueError): M.adapt(row, spec('plans'))

    def test_closed_profile_never_uses_digest_plan_adapter(self):
        for row in (plan(row_for()), intent(row_for()), row_for()):
            with self.assertRaises(ValueError): M.adapt(row, {**spec('plans'), 'adapter': 'digest-plan-v1'})

    def test_category_context_unused_dictionary_and_observer_warnings(self):
        for kind in ('Categorical', 'Enum'):
            row = categories(row_for(), kind)
            raw = M.adapt(row, spec())[2]
            self.assertIn(b'unused', raw)
            self.assertIn(b'observation_warnings', raw)
            self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')

    def test_category_context_and_dictionary_changes_affect_original_hash(self):
        row = categories(row_for()); before = row['receipt_sha256']
        row['receipt']['category_observables'][0]['context']['namespace'] = 'different'
        rebind(row)
        self.assertNotEqual(row['receipt_sha256'], before)
        row['input_categories_before'] = copy.deepcopy(row['receipt']['category_observables'])
        row['input_categories_unchanged'] = False; row['input_unchanged'] = False
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')

    def test_incomplete_category_or_wrong_column_metadata_blocks(self):
        for change in ('missing', 'unknown', 'duplicate', 'column', 'context', 'warning'):
            row = categories(row_for())
            category = row['receipt']['category_observables'][0]
            if change == 'missing': row['receipt']['category_observables'] = []
            if change == 'unknown': category['physical_codes'] = [0]
            if change == 'duplicate': row['receipt']['category_observables'].append(copy.deepcopy(category))
            if change == 'column': category['column_index'] = 1
            if change == 'context': del category['context']['namespace']
            if change == 'warning': del category['observation_warnings'][0]['message']
            rebind(row)
            with self.subTest(change=change), self.assertRaises(ValueError): M.adapt(row, spec())

    def test_enum_declared_dictionary_is_complete_and_ordered(self):
        row = categories(row_for(), 'Enum')
        row['receipt']['category_observables'][0]['declared_categories']['values'].reverse(); rebind(row)
        with self.assertRaises(ValueError): M.adapt(row, spec())

    def test_actual_source_and_kwargs_mutation_flags_are_evidence(self):
        row = row_for(); row['input_after']['rows'][0]['values'][0] = scalar('changed')
        with self.assertRaises(ValueError): M.adapt(row, spec())
        row['input_values_unchanged'] = False; row['input_unchanged'] = False
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        row['kwargs_after']['items'][0][1] = scalar(None)
        with self.assertRaises(ValueError): M.adapt(row, spec())
        row['kwargs_unchanged'] = False
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')

    def test_returned_identity_binds_result_source_and_input_class(self):
        row = row_for(); row['returned_input_identity'] = True
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        row['result']['rows'][0]['values'][0] = scalar('other')
        with self.assertRaises(ValueError): M.adapt(row, spec())

    def test_all_packed_shapes_are_preserved_with_ordered_enum_collision(self):
        row = row_for('apply_auto_filter')
        enum = {'type': 'enum', 'module': MODULE, 'name': 'ACTIVITY_KEY', 'value': scalar('pm4py:param:activity_key')}
        packed = dictionary((enum, scalar('x')), (scalar('pm4py:param:activity_key'), sequence(scalar(1), kind='tuple')),
                            (scalar('extra'), sequence(scalar(-0.0), {'type': 'date', 'iso': '2026-10-22'},
                                                      {'type': 'datetime', 'iso': '2026-10-22T12:00:00+00:00', 'fold': 1, 'offset_seconds': 0.0})),
                            (scalar('set'), sequence(scalar('a'), scalar('b'), kind='set')))
        row['receipt']['kwargs'] = dictionary((scalar('parameters'), packed))
        row['kwargs_before'] = copy.deepcopy(row['receipt']['kwargs']); row['kwargs_after'] = copy.deepcopy(row['receipt']['kwargs']); rebind(row)
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        before = row['receipt_sha256']; row['receipt']['kwargs']['items'][0][1]['items'].reverse(); rebind(row)
        self.assertNotEqual(before, row['receipt_sha256'])

    def test_unknown_typed_tag_enum_or_datetime_metadata_blocks(self):
        for value in ({'type': 'future', 'value': 1}, {'type': 'enum', 'module': MODULE, 'name': 'FUTURE', 'value': scalar('future')},
                      {'type': 'datetime', 'iso': '2026-10-22T00:00:00+00:00', 'fold': 2, 'offset_seconds': 0.0}):
            row = row_for('apply_auto_filter'); row['receipt']['kwargs'] = dictionary((scalar('parameters'), value)); rebind(row)
            with self.assertRaises(ValueError): M.adapt(row, spec())

    def test_frame_dimensions_empty_names_and_wide_values(self):
        row = row_for(); self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        for change in ('shape', 'row', 'dtype', 'duplicate_column'):
            bad = copy.deepcopy(row)
            if change == 'shape': bad['receipt']['input']['shape'][0] = 2
            if change == 'row': bad['receipt']['input']['rows'][0]['values'].pop()
            if change == 'dtype': bad['receipt']['input']['dtypes'].pop()
            if change == 'duplicate_column': bad['receipt']['input']['columns'][1] = ''
            rebind(bad)
            with self.subTest(change=change), self.assertRaises(ValueError): M.adapt(bad, spec())

    def test_wrong_method_keyword_or_canonical_operand_blocks(self):
        for change in ('function', 'keyword', 'duplicate', 'digest', 'kind', 'shape', 'extra_canonical'):
            row = row_for()
            if change == 'function': row['receipt']['function'] = 'apply_events'
            if change == 'keyword': row['receipt']['kwargs']['items'][0][0] = scalar('variants')
            if change == 'duplicate': row['receipt']['kwargs']['items'].append(copy.deepcopy(row['receipt']['kwargs']['items'][0]))
            if change == 'kind': row['receipt']['input_kind'] = 'Frame'
            if change == 'shape': row['receipt']['call_shape'] = 'unknown'
            if change == 'extra_canonical': row['receipt']['args'] = sequence()
            if change != 'digest': rebind(row)
            else: row['receipt_sha256'] = '0' * 64
            with self.subTest(change=change), self.assertRaises(ValueError): M.adapt(row, spec())

    def test_partial_entry_requires_real_entry_and_consistent_full_captured_fields(self):
        row = row_for(); partial = {'receipt': row['receipt'], 'receipt_sha256': row['receipt_sha256'], 'apply_returned': False,
                                    'input_after': copy.deepcopy(row['input_after']), 'input_categories_after': [], 'input_unchanged': True}
        self.assertEqual(M.adapt(partial, spec())[-1], 'entered_without_outcome')
        partial['input_unchanged'] = False
        with self.assertRaises(ValueError): M.adapt(partial, spec())
        with self.assertRaises(ValueError): M.adapt({'receipt': row['receipt'], 'receipt_sha256': row['receipt_sha256'], 'input_before': frame()}, spec('plans'))

    def test_full_audit_wrapper_counts_and_unexpected_entry_fields_block(self):
        row = row_for(); audit = {'planned': 1, 'prior_receipts_verified': 0, 'duplicates': [], 'prior_overlaps': [], 'receipts': [plan(row)]}
        config = {**spec('plans'), 'format': 'json', 'records_key': 'receipts'}
        M._polars_variants_audit(M.canonical(audit), config)
        for change in ('count', 'duplicate', 'overlap', 'unknown_wrapper', 'unknown_row', 'digest', 'case'):
            bad = copy.deepcopy(audit)
            if change == 'count': bad['planned'] = 2
            if change == 'duplicate': bad['duplicates'] = [{'first': 'own'}]
            if change == 'overlap': bad['prior_overlaps'] = [{'case': 'own'}]
            if change == 'unknown_wrapper': bad['phase'] = 'apply'
            if change == 'unknown_row': bad['receipts'][0]['apply_returned'] = True
            if change == 'digest': bad['receipts'][0]['receipt_sha256'] = '0' * 64
            if change == 'case': bad['receipts'][0]['case'] = ''
            with self.subTest(change=change), self.assertRaises(ValueError): M._polars_variants_audit(M.canonical(bad), config)

    def test_index_raw_slices_full_plans_module_free_intents_and_cache(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); row = row_for()
            raw = json.dumps(row).encode() + b'\r\n'
            sources = []
            audit = {'planned': 1, 'prior_receipts_verified': 0, 'duplicates': [], 'prior_overlaps': [], 'receipts': [plan(row)]}
            for name, content, role, fmt in [('records.jsonl', raw, 'observations', 'jsonl'),
                                             ('intents.jsonl', M.canonical(intent(row)) + b'\n', 'intents', 'jsonl'),
                                             ('audit.json', M.canonical(audit), 'plans', 'json')]:
                (root / name).write_bytes(content)
                source = {**spec(role), 'path': name, 'bytes': len(content), 'sha256': M.sha(content), 'format': fmt}
                if name == 'audit.json': source['records_key'] = 'receipts'
                sources.append(source)
            manifest = {'schema': M.MANIFEST, 'root': str(root), 'scope': 'own synthetic', 'inventory_complete': True, 'modules': [MODULE], 'sources': sources}
            index = M.Index(root / 'index.sqlite')
            try:
                first = index.run(manifest); state = first['per_module'][MODULE]
                self.assertEqual(state['distinct_actual_receipts'], 1)
                self.assertEqual(state['intents_with_terminal_outcome'], 1)
                self.assertEqual(state['blocking_gaps'], 0)
                self.assertFalse(first['catalogue_audit_complete'])
                self.assertFalse(state['no_prior_calls_within_declared_inventory'])
                self.assertEqual(dict(index.db.execute('SELECT status,COUNT(*) FROM observations GROUP BY status')),
                                 {'actual_outcome': 1, 'planned_only': 1, 'entered_without_outcome': 1})
                self.assertEqual(index.db.execute("SELECT record_sha256 FROM observations WHERE status='actual_outcome'").fetchone()[0], M.sha(raw))
                self.assertEqual(index.run(manifest)['cache_hits'], 3)
            finally:
                index.db.close()

    def test_unknown_and_conflicting_modules_block_all_affected_scopes(self):
        other = 'pm4py.algo.filtering.polars.ltl.ltl_checker'
        for conflict in (False, True):
            with tempfile.TemporaryDirectory() as folder:
                root = Path(folder); row = row_for()
                if conflict: row['module'] = other
                else: row['unexpected_terminal'] = True
                content = M.canonical(row); (root / 'bad.jsonl').write_bytes(content)
                source = {**spec(), 'path': 'bad.jsonl', 'bytes': len(content), 'sha256': M.sha(content), 'format': 'jsonl'}
                manifest = {'schema': M.MANIFEST, 'root': str(root), 'scope': 'own synthetic', 'inventory_complete': True, 'modules': [MODULE, other], 'sources': [source]}
                index = M.Index(root / 'index.sqlite')
                try:
                    report = index.run(manifest)
                    self.assertFalse(report['per_module'][MODULE]['complete_within_declared_inventory'])
                    if conflict: self.assertFalse(report['per_module'][other]['complete_within_declared_inventory'])
                finally:
                    index.db.close()

    def test_intent_resolution_does_not_cross_runtime_partitions(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); row = row_for(); sources = []
            for name, content, role, runtime in [('records.jsonl', M.canonical(row), 'observations', 'one'),
                                                ('intents.jsonl', M.canonical(intent(row)), 'intents', 'two')]:
                (root / name).write_bytes(content)
                sources.append({**spec(role), 'path': name, 'bytes': len(content), 'sha256': M.sha(content), 'format': 'jsonl', 'runtime': runtime})
            index = M.Index(root / 'index.sqlite')
            try:
                result = index.run({'schema': M.MANIFEST, 'root': str(root), 'scope': 'own synthetic', 'inventory_complete': True, 'modules': [MODULE], 'sources': sources})
                self.assertEqual(result['per_module'][MODULE]['intents_with_terminal_outcome'], 0)
                self.assertFalse(result['per_module'][MODULE]['complete_within_declared_inventory'])
            finally:
                index.db.close()


if __name__ == '__main__':
    unittest.main()
