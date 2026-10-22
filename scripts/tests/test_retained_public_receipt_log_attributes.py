"""Closed LOG attributes observer tests. Synthetic, offline, no reference calls."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('receipt_index_attributes', ROOT / 'scripts/retained_public_receipt_index.py')
M = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(M)
MODULE = 'pm4py.algo.filtering.log.attributes.attributes_filter'
ADAPTER = 'log-attributes-packed-v1'
SCHEMA = 'minera-log-attributes-observer-receipt-v1'


def scalar(value):
    return {'type': type(value).__name__, 'value': value}


def mapping(*pairs):
    return {'type': 'dict', 'items': [[key, value] for key, value in pairs]}


def snapshot():
    source = {key: mapping() for key in ('attributes', 'extensions', 'classifiers', 'omni_present', 'properties')}
    source['traces'] = [{'attributes': mapping((scalar('concept:name'), scalar('case'))), 'properties': mapping(),
                         'events': [mapping((scalar('activity'), scalar('A')), (scalar('integer'), scalar(9007199254740993)))]}]
    return source


def terminal(threshold=False):
    source = snapshot()
    operands = [{'type': 'log', 'snapshot': source}, {'type': 'list', 'values': [scalar('A')]}]
    function = 'apply'
    if threshold:
        function = 'filter_log_by_attributes_threshold'
        alias = {'type': 'trace', 'snapshot': copy.deepcopy(source['traces'][0]), 'source_trace_index': 0}
        operands = [operands[0], mapping((scalar('A'), scalar(1))),
                    mapping(({'type': 'tuple', 'values': [scalar('A')]}, {'type': 'list', 'values': [alias]})),
                    {'type': 'list', 'values': [{'type': 'tuple', 'values': [{'type': 'tuple', 'values': [scalar('A')]}, scalar(1)]}]}, scalar(1)]
    receipt = {'module': MODULE, 'function': function, 'args': {'type': 'list', 'values': operands},
               'kwargs': mapping(), 'call_shape': 'positional_args_keyword_controls'}
    row = {'index': 1, 'module': MODULE, 'case': {'name': 'synthetic', 'function': function}, 'receipt': receipt,
           'receipt_sha256': '', 'input_before': copy.deepcopy(source),
           'controls_before': {'args': copy.deepcopy(receipt['args']), 'kwargs': copy.deepcopy(receipt['kwargs'])},
           'input_after': copy.deepcopy(source), 'controls_after': {'args': copy.deepcopy(receipt['args']), 'kwargs': copy.deepcopy(receipt['kwargs'])},
           'input_unchanged': True, 'controls_unchanged': True, 'warnings': [], 'result_kind': 'log', 'result': copy.deepcopy(source),
           'returned_input_identity': True, 'trace_aliases_source': [True], 'event_source_positions': [[[0, 0]]]}
    return rebind(row)


def rebind(row):
    row['receipt_sha256'] = M.sha(M.canonical(row['receipt']))
    return row


def synchronize_before(row):
    row['controls_before'] = {'args': copy.deepcopy(row['receipt']['args']), 'kwargs': copy.deepcopy(row['receipt']['kwargs'])}
    row['controls_after'] = copy.deepcopy(row['controls_before'])
    return rebind(row)


def parameters(row, key=None, value=None):
    enum = {'type': 'enum', 'module': MODULE, 'name': 'POSITIVE', 'value': scalar('positive')}
    row['receipt']['kwargs'] = mapping((scalar('parameters'), mapping((key or enum, value or scalar(True)))))
    return synchronize_before(row)


def spec(role='observations'):
    return {'adapter': ADAPTER, 'receipt_schema': SCHEMA, 'role': role}


class LogAttributesAdapterTests(unittest.TestCase):
    def test_original_full_canonical_bytes_and_actual_before_plan(self):
        row = terminal()
        expected = M.canonical(row['receipt'])
        for role in ('observations', 'plans', 'intents'):
            adapted = M.adapt(row, spec(role))
            self.assertEqual(adapted, (MODULE, 'apply', expected, M.sha(expected), 'actual_outcome'))

    def test_error_complete_source_and_control_mutation(self):
        row = terminal()
        for key in ('result', 'result_kind', 'returned_input_identity', 'trace_aliases_source', 'event_source_positions'):
            row.pop(key)
        row['error'] = {'type': 'builtins.KeyError', 'message': "'retained full diagnostic'", 'args': {'type': 'tuple', 'values': [scalar('retained full diagnostic')]}}
        row['input_after']['traces'][0]['events'][0]['items'].append([scalar('case:concept:name'), scalar('case')])
        row['controls_after']['args']['values'][0]['snapshot'] = copy.deepcopy(row['input_after'])
        row['input_unchanged'] = row['controls_unchanged'] = False
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        row['controls_unchanged'] = True
        with self.assertRaises(ValueError): M.adapt(row, spec())

    def test_returned_source_and_external_cache_trace_descriptors(self):
        row = terminal(threshold=True)
        aliases = row['receipt']['args']['values'][2]['items'][0][1]['values']
        external = copy.deepcopy(aliases[0]); external['source_trace_index'] = None
        external['snapshot']['properties'] = mapping((scalar('external'), scalar(True)))
        aliases.append(external); synchronize_before(row)
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')

    def test_source_alias_indices_snapshot_and_exact_fields(self):
        for change in ('index', 'boolean', 'snapshot', 'unknown_field'):
            row = terminal(threshold=True)
            alias = row['receipt']['args']['values'][2]['items'][0][1]['values'][0]
            if change == 'index': alias['source_trace_index'] = 4
            if change == 'boolean': alias['source_trace_index'] = False
            if change == 'snapshot': alias['snapshot']['attributes'] = mapping()
            if change == 'unknown_field': alias['invented_identity'] = 'no'
            synchronize_before(row)
            with self.subTest(change=change), self.assertRaises(ValueError): M.adapt(row, spec())

    def test_after_cache_alias_checked_against_after_source(self):
        row = terminal(threshold=True)
        row['input_after']['traces'][0]['properties'] = mapping((scalar('changed'), scalar(True)))
        row['controls_after']['args']['values'][0]['snapshot'] = copy.deepcopy(row['input_after'])
        row['input_unchanged'] = row['controls_unchanged'] = False
        with self.assertRaises(ValueError): M.adapt(row, spec())
        row['controls_after']['args']['values'][2]['items'][0][1]['values'][0]['snapshot'] = copy.deepcopy(row['input_after']['traces'][0])
        row['result'] = copy.deepcopy(row['input_after'])
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')

    def test_enum_identity_value_and_ordered_string_collision(self):
        row = parameters(terminal())
        entries = row['receipt']['kwargs']['items'][0][1]['items']
        entries.append([scalar('positive'), scalar(False)])
        synchronize_before(row)
        full = M.adapt(row, spec())[2]
        self.assertEqual(full, M.canonical(row['receipt']))
        for field, bad in [('module', 'pm4py.another'), ('name', 'UNKNOWN'), ('value', scalar('wrong'))]:
            badrow = copy.deepcopy(row)
            badrow['receipt']['kwargs']['items'][0][1]['items'][0][0][field] = bad
            synchronize_before(badrow)
            with self.subTest(field=field), self.assertRaises(ValueError): M.adapt(badrow, spec())

    def test_hard_closed_module_function_arity_and_schema(self):
        for mutation in ('module', 'function', 'arity', 'call_shape', 'schema'):
            row = terminal(); configuration = spec()
            if mutation == 'module': row['module'] = row['receipt']['module'] = 'pm4py.algo.filtering.log.ltl.ltl_checker'
            if mutation == 'function': row['receipt']['function'] = 'unreviewed'
            if mutation == 'arity': row['receipt']['args']['values'].append(scalar('extra'))
            if mutation == 'call_shape': row['receipt']['call_shape'] = 'guessed'
            if mutation == 'schema': configuration['receipt_schema'] = M.SCHEMAS['receipt-v1'][0]
            synchronize_before(row)
            with self.subTest(mutation=mutation), self.assertRaises(ValueError): M.adapt(row, configuration)

    def test_nested_packed_scalars_remain_exact_and_unknowns_gap(self):
        values = [scalar(2**80), {'type': 'bytes', 'hex': '00ff'}, {'type': 'float', 'hex': 'nan'},
                  {'type': 'native_na'}, {'type': 'nat'}, {'type': 'uuid', 'value': '12345678-1234-1234-1234-123456789abc'},
                  {'type': 'datetime', 'iso': '2026-06-10T10:11:12+00:00', 'fold': 0, 'offset_seconds': 0.0}]
        for value in values:
            row = parameters(terminal(), value=value)
            self.assertEqual(M.adapt(row, spec())[2], M.canonical(row['receipt']))
        for value in ({'type': 'mystery'}, {'type': 'bytes', 'hex': '0'}, {'type': 'int', 'value': True},
                      {'type': 'float', 'hex': '0x1p999999'}, {'type': 'native_na', 'value': None}):
            row = parameters(terminal(), value=value)
            with self.subTest(type=value['type']), self.assertRaises(ValueError): M.adapt(row, spec())

    def test_original_hash_and_source_control_binding_not_reconstructed(self):
        for change in ('digest', 'input_before', 'controls_before', 'after_arity', 'after_keywords'):
            row = terminal()
            if change == 'digest': row['receipt_sha256'] = '0' * 64
            if change == 'input_before': row['input_before']['traces'] = []
            if change == 'controls_before': row['controls_before']['args']['values'][1]['values'] = []
            if change == 'after_arity': row['controls_after']['args']['values'].append(scalar('x'))
            if change == 'after_keywords': row['controls_after']['kwargs']['items'].append([scalar('new'), scalar(True)])
            with self.subTest(change=change), self.assertRaises(ValueError): M.adapt(row, spec())

    def test_outcome_metadata_and_return_alias_fail_closed(self):
        for change in ('missing_error_args', 'mixed', 'incomplete_result', 'result_alias', 'event_alias', 'warning'):
            row = terminal()
            if change == 'missing_error_args': row['error'] = {'type': 'builtins.KeyError', 'message': 'x'}
            if change == 'mixed': row['error'] = {'type': 'builtins.ValueError', 'message': 'x', 'args': {'type': 'tuple', 'values': []}}
            if change == 'incomplete_result': row['result'].pop('properties')
            if change == 'result_alias': row['result']['traces'][0]['properties'] = mapping((scalar('different'), scalar(True)))
            if change == 'event_alias': row['event_source_positions'] = [[[0, 8]]]
            if change == 'warning': row['warnings'] = [{'message': 'missing category'}]
            with self.subTest(change=change), self.assertRaises(ValueError): M.adapt(row, spec())

    def test_full_plans_and_entered_mutation_evidence(self):
        original = terminal(); minimal = {'receipt': original['receipt'], 'receipt_sha256': original['receipt_sha256']}
        self.assertEqual(M.adapt(minimal, spec('plans'))[-1], 'planned_only')
        self.assertEqual(M.adapt(minimal, spec('intents'))[-1], 'entered_without_outcome')
        for key in ('controls_after', 'input_after', 'apply_returned'):
            row = {**minimal, key: True}
            self.assertEqual(M.adapt(row, spec('plans'))[-1], 'entered_without_outcome')

    def test_returned_input_identity_requires_complete_after_snapshot(self):
        row = terminal()
        row['trace_aliases_source'] = [False]
        with self.assertRaisesRegex(ValueError, 'returned source identity'):
            M.adapt(row, spec())
        row['result']['traces'][0]['properties'] = mapping((scalar('different'), scalar(True)))
        row['trace_aliases_source'] = [False]
        row['event_source_positions'] = [[]]
        with self.assertRaisesRegex(ValueError, 'returned source identity'):
            M.adapt(row, spec())
        row['returned_input_identity'] = False
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')

    def test_unknown_terminal_markers_cannot_hide_as_plans(self):
        original = terminal()
        for marker in ('result_value', 'output', 'return', 'returned', 'exception', 'value_result', 'result_kind'):
            row = {'receipt': original['receipt'], 'receipt_sha256': original['receipt_sha256'], marker: True}
            with self.subTest(marker=marker), self.assertRaises(ValueError): M.adapt(row, spec('plans'))

    def test_digest_plans_do_not_supply_receipts_or_hide_entry(self):
        config = {**spec('plans'), 'adapter': 'digest-plan-v1'}
        digest = terminal()['receipt_sha256']
        self.assertEqual(M.adapt({'receipt_sha256': digest}, config)[-1], 'digest_only_plan')
        for marker in ('receipt', 'result', 'controls_after', 'apply_returned', 'returned'):
            with self.subTest(marker=marker), self.assertRaises(ValueError): M.adapt({'receipt_sha256': digest, marker: True}, config)

    def test_index_intent_resolution_exact_partition_record_hash_and_cache(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); row = terminal(threshold=True)
            raw = b'\xef\xbb\xbf' + json.dumps(row, ensure_ascii=True).encode() + b'\r\n'
            intent = M.canonical({'index': 1, 'module': MODULE, 'function': row['receipt']['function'], 'receipt_sha256': row['receipt_sha256']}) + b'\n'
            specs = []
            for name, content, role in [('records.jsonl', raw, 'observations'), ('intents.jsonl', intent, 'intents')]:
                (root / name).write_bytes(content)
                specs.append({**spec(role), 'path': name, 'bytes': len(content), 'sha256': M.sha(content), 'format': 'jsonl',
                              'provenance': 'own-public-observation', 'reference': {'commit': 'synthetic'}, 'runtime': {'python': 'synthetic'}})
            manifest = {'schema': M.MANIFEST, 'root': str(root), 'inventory_complete': True, 'scope': 'synthetic attrs only', 'modules': [MODULE], 'sources': specs}
            index = M.Index(root / 'index.sqlite')
            try:
                first = index.run(manifest); stats = first['per_module'][MODULE]
                self.assertEqual(stats['distinct_actual_receipts'], 1)
                self.assertEqual(stats['intents_with_terminal_outcome'], 1)
                self.assertEqual(stats['blocking_gaps'], 0)
                self.assertFalse(stats['no_prior_calls_within_declared_inventory'])
                self.assertFalse(first['catalogue_audit_complete'])
                self.assertEqual(index.db.execute("SELECT record_sha256 FROM observations WHERE status='actual_outcome'").fetchone()[0], M.sha(raw))
                self.assertEqual(index.run(manifest)['cache_hits'], 2)
            finally:
                index.db.close()


if __name__ == '__main__':
    unittest.main()
