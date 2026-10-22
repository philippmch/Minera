"""Synthetic closed LOG variants observer integrity; no reference imports."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('log_variants_index', Path(__file__).parents[1] / 'retained_public_receipt_index.py')
M = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(M)
MODULE = 'pm4py.algo.filtering.log.variants.variants_filter'
OTHER = 'pm4py.algo.filtering.log.attributes.attributes_filter'
ADAPTER = 'log-variants-packed-v1'


def scalar(x):
    return {'type': 'float', 'hex': x.hex()} if type(x) is float else {'type': type(x).__name__, 'value': x}


def seq(*values, kind='list'): return {'type': kind, 'values': list(values)}


def mapping(*pairs): return {'type': 'dict', 'items': [list(p) for p in pairs]}


def snapshot():
    return {**{k: mapping() for k in ('attributes', 'extensions', 'classifiers', 'omni_present', 'properties')},
            'traces': [{'attributes': mapping((scalar('concept:name'), scalar('case'))), 'properties': mapping(),
                        'events': [mapping((scalar('concept:name'), scalar('A')), (scalar('position'), scalar(0)))]}]}


def spec(role='observations', adapter=ADAPTER):
    return {'adapter': adapter, 'receipt_schema': M.SCHEMAS[ADAPTER][0], 'role': role,
            'reference': {'commit': 'own-synthetic-pin'}, 'runtime': {'version': 'own-synthetic'}}


def rebind(row): row['receipt_sha256'] = M.sha(M.canonical(row['receipt']));return row


def row_for(function='apply', error=False):
    source = snapshot();args = [dict(type='log', snapshot=source)]
    if function == 'apply': args += [seq(seq(scalar('A'), kind='tuple'))]
    elif function in {'find_auto_threshold', 'filter_variants_variants_percentage'}:
        args += [mapping((seq(scalar('A'), kind='tuple'), seq(dict(type='trace', snapshot=source['traces'][0], source_trace_index=0)))), scalar(0.5)]
    else: args += [scalar(0.5)]
    receipt = {'module': MODULE, 'function': function, 'args': seq(*args), 'kwargs': mapping(), 'call_shape': 'positional_args_keyword_controls'}
    row = {'index': 1, 'module': MODULE, 'case': {'name': 'own synthetic case', 'function': function}, 'receipt': receipt,
           'input_before': copy.deepcopy(source), 'controls_before': {'args': copy.deepcopy(receipt['args']), 'kwargs': mapping()},
           'input_after': copy.deepcopy(source), 'controls_after': {'args': copy.deepcopy(receipt['args']), 'kwargs': mapping()},
           'input_unchanged': True, 'controls_unchanged': True, 'warnings': []}
    if error: row['error'] = {'type': 'builtins.TypeError', 'message': 'OWN full synthetic error', 'args': seq(scalar('OWN full synthetic error'), kind='tuple')}
    elif function == 'find_auto_threshold': row.update(result_kind='value', value_result=scalar(-0.0))
    else: row.update(result_kind='log', result=copy.deepcopy(source), returned_input_identity=False, trace_aliases_source=[True], event_source_positions=[[[0, 0]]])
    return rebind(row)


def plan(row): return {'receipt': copy.deepcopy(row['receipt']), 'receipt_sha256': row['receipt_sha256'], 'case': 'own full plan', 'index': 1}


def intent(row): return {'index': 1, 'module': MODULE, 'function': row['receipt']['function'], 'case': row['case']['name'], 'receipt_sha256': row['receipt_sha256']}


def audit(row):
    return {'planned': 1, 'prior_paths': [], 'prior_receipt_count': 0, 'prior_full_receipt_audits': [], 'duplicates': [],
            'prior_overlaps': [], 'receipts': [{'case': row['case']['name'], 'receipt_sha256': row['receipt_sha256']}]}


def source(root, name, rows, role='observations', **extra):
    raw = b''.join(json.dumps(x, ensure_ascii=True).encode() + b'\r\n' for x in rows)
    (root/name).write_bytes(raw)
    return {**spec(role), 'path': name, 'bytes': len(raw), 'sha256': M.sha(raw), 'format': 'jsonl', 'provenance': 'own-public-observation', **extra}


def manifest(root, sources, modules=None):
    return {'schema': M.MANIFEST, 'root': str(root), 'scope': 'explicit own synthetic variants only', 'inventory_complete': True,
            'modules': modules or [MODULE], 'sources': sources}


class LogVariantsReceiptTests(unittest.TestCase):
    def reject(self, row, role='observations', adapter=ADAPTER):
        with self.assertRaises((ValueError, KeyError, TypeError)): M.adapt(row, spec(role, adapter))

    def test_original_five_fields_and_full_typed_bytes_preserved(self):
        row = row_for();result = M.adapt(row, spec())
        self.assertEqual(result[2], M.canonical(row['receipt']))
        self.assertEqual(result[3], row['receipt_sha256']);self.assertEqual(result[-1], 'actual_outcome')
        row['receipt']['input'] = snapshot();rebind(row);self.reject(row)

    def test_all_seven_function_domains_and_default_omission(self):
        for function in M.LOG_VARIANTS_ARITIES:
            with self.subTest(function=function): self.assertEqual(M.adapt(row_for(function), spec())[-1], 'actual_outcome')
        row = row_for('filter_log_variants_percentage');row['receipt']['args']['values'].pop()
        row['controls_before']['args'] = copy.deepcopy(row['receipt']['args']);row['controls_after']['args'] = copy.deepcopy(row['receipt']['args']);rebind(row)
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        row['receipt']['args']['values'] = [];rebind(row);self.reject(row)

    def test_terminal_first_under_plan_and_intent_roles(self):
        for function, error in [('apply', False), ('apply', True), ('find_auto_threshold', False)]:
            row = row_for(function, error)
            for role in ('plans', 'intents'): self.assertEqual(M.adapt(row, spec(role))[-1], 'actual_outcome')
            self.reject(row, 'plans', 'digest-plan-v1')

    def test_full_error_and_scalar_outcome_exclusivity(self):
        row = row_for(error=True);self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        for mutation in ('missing_args', 'wrong_args', 'mixed_terminal', 'wrong_type'):
            bad = copy.deepcopy(row)
            if mutation == 'missing_args': del bad['error']['args']
            elif mutation == 'wrong_args': bad['error']['args']['type'] = 'list'
            elif mutation == 'mixed_terminal': bad['value_result'] = scalar(1.0)
            else: bad['error']['type'] = 'unqualified'
            self.reject(bad)
        row = row_for('find_auto_threshold');row['value_result'] = scalar(1);self.reject(row)
        row = row_for();row.pop('result');row.update(value_result=scalar(0.5));self.reject(row)

    def test_full_metadata_exact_index_and_case(self):
        for value in (True, 0, -1, '1'):
            row = row_for();row['index'] = value;self.reject(row)
        for value in ({'name': 'x', 'function': 'unknown'}, {'name': 'x', 'function': 'apply', 'apply_returned': True}, 'flat-name'):
            row = row_for();row['case'] = value;self.reject(row)

    def test_enum_identities_and_typed_tuple_dictionary_keys(self):
        row = row_for();enum = {'type': 'enum', 'module': MODULE, 'name': 'POSITIVE', 'value': scalar('positive')}
        kwargs = mapping((scalar('parameters'), mapping((enum, scalar(False)))))
        row['receipt']['kwargs'] = copy.deepcopy(kwargs);row['controls_before']['kwargs'] = copy.deepcopy(kwargs);row['controls_after']['kwargs'] = copy.deepcopy(kwargs);rebind(row)
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        for field, value in [('module', OTHER), ('name', 'UNKNOWN'), ('value', scalar('different'))]:
            bad = copy.deepcopy(row);bad['receipt']['kwargs']['items'][0][1]['items'][0][0][field] = value;rebind(bad);self.reject(bad)

    def test_special_values_full_context_and_unknown_descriptor(self):
        values = [{'type': 'native_na'}, {'type': 'nat'}, {'type': 'uuid', 'value': '12345678-1234-1234-1234-123456789abc'},
                  {'type': 'bytes', 'hex': '00ff'}, scalar(2**100), scalar(float('nan')), scalar(float('inf')),
                  {'type': 'datetime', 'iso': '2026-10-22T12:00:00+02:00', 'fold': 1, 'offset_seconds': 7200.0},
                  seq(scalar('A'), kind='tuple'), seq(scalar('A'), kind='set'), seq(scalar('A'), kind='frozenset')]
        for value in values:
            row = row_for();row['receipt']['args']['values'][1] = value;row['controls_before']['args'] = copy.deepcopy(row['receipt']['args']);row['controls_after']['args'] = copy.deepcopy(row['receipt']['args']);rebind(row)
            with self.subTest(value=value): self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        row = row_for();row['receipt']['args']['values'][1] = {'type': 'unknown'};rebind(row);self.reject(row)

    def test_source_trace_handles_and_after_control_bindings(self):
        row = row_for('find_auto_threshold');self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        for index in (True, -1, 1):
            bad = copy.deepcopy(row);bad['receipt']['args']['values'][1]['items'][0][1]['values'][0]['source_trace_index'] = index;rebind(bad);self.reject(bad)
        bad = copy.deepcopy(row);bad['controls_after']['args']['values'][1]['items'][0][1]['values'][0]['snapshot']['events'] = [];bad['controls_unchanged'] = False;self.reject(bad)

    def test_full_source_and_flag_mutation_consistency(self):
        row = row_for();row['input_after']['attributes'] = mapping((scalar('mutated'), scalar(True)))
        row['controls_after']['args']['values'][0]['snapshot'] = copy.deepcopy(row['input_after'])
        row['input_unchanged'] = False;row['controls_unchanged'] = False
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        row['input_unchanged'] = True;self.reject(row)
        row = row_for();row['input_before'].pop('extensions');self.reject(row)

    def test_returned_identity_and_trace_alias_integrity(self):
        row = row_for();row['returned_input_identity'] = True
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        row['trace_aliases_source'] = [False];self.reject(row)
        row = row_for();row['result']['attributes'] = mapping((scalar('different'), scalar(1)));row['returned_input_identity'] = True;self.reject(row)
        row = row_for();row['result']['traces'][0]['properties'] = mapping((scalar('different'), scalar(1)));self.reject(row)

    def test_flattened_event_groups_validate_order_completeness_and_bounds(self):
        row = row_for();row['event_source_positions'] = [[[0, 0], [0, 0]]]
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        for refs in ([], [[0, 1]], [[True, 0]], [[0, 0, 0]]):
            bad = row_for();bad['event_source_positions'] = [refs];self.reject(bad)

    def test_keyword_arity_and_cross_family_rejection(self):
        row = row_for();row['receipt']['kwargs'] = mapping((scalar('decreasing_factor'), scalar(0.5)));rebind(row);self.reject(row)
        row = row_for();row['receipt']['module'] = OTHER;rebind(row);self.reject(row)
        row = row_for();row['module'] = OTHER;self.reject(row)
        row = row_for();wrong = spec();wrong['receipt_schema'] = M.SCHEMAS['log-attributes-packed-v1'][0]
        with self.assertRaises(ValueError): M.adapt(row, wrong)

    def test_warning_error_args_and_unknown_terminal_fields(self):
        row = row_for();row['warnings'] = [{'category': 'builtins.UserWarning', 'message': 'OWN warning\n\0Ω'}]
        self.assertEqual(M.adapt(row, spec())[-1], 'actual_outcome')
        row['warnings'][0]['filename'] = 'not-observed';self.reject(row)
        for key in ('output', 'return', 'exception', 'error_type', 'result_value'):
            row = plan(row_for());row[key] = 'possible outcome';self.reject(row, 'plans')

    def test_closed_full_plan_digest_plan_and_partial_entries(self):
        row = row_for();full = plan(row)
        self.assertEqual(M.adapt(full, spec('plans'))[-1], 'planned_only')
        digest = {'case': 'own digest plan', 'receipt_sha256': row['receipt_sha256']}
        self.assertEqual(M.adapt(digest, spec('plans', 'digest-plan-v1'))[-1], 'digest_only_plan')
        for base, adapter in [(full, ADAPTER), (digest, 'digest-plan-v1')]:
            for key, value in [('index', True), ('case', {'apply_returned': True}), ('unknown_entry', True), ('error_type', 'TypeError')]:
                bad = copy.deepcopy(base);bad[key] = value;self.reject(bad, 'plans', adapter)
        entry = {**full, 'input_after': snapshot(), 'controls_after': copy.deepcopy(row['controls_after'])}
        self.assertEqual(M.adapt(entry, spec('plans'))[-1], 'entered_without_outcome')
        for key in ('input_before', 'controls_before'):
            bad = copy.deepcopy(full);bad[key] = copy.deepcopy(row[key]);self.reject(bad, 'plans')

    def test_intents_exact_shape_metadata_and_digest(self):
        row = intent(row_for());self.assertEqual(M.adapt(row, spec('intents'))[-1], 'entered_without_outcome')
        for key, value in [('index', True), ('module', OTHER), ('case', {'entered': True}), ('function', 'unknown'), ('receipt_sha256', 'x'), ('controls_after', {})]:
            bad = copy.deepcopy(row);bad[key] = value;self.reject(bad, 'intents')

    def test_full_audit_digest_rows_duplicates_and_prior_binding(self):
        row = row_for();a = audit(row);s = {**spec('plans', 'digest-plan-v1'), 'records_key': 'receipts'}
        M._log_variants_audit(M.canonical(a), s)
        a['planned'] = 2;a['duplicates'] = [{'first': row['case']['name'], 'duplicate': 'own duplicate', 'receipt_sha256': row['receipt_sha256']}]
        M._log_variants_audit(M.canonical(a), s)
        for change in ('entry', 'count', 'duplicate_hash', 'prior', 'receipt_extra'):
            bad = copy.deepcopy(a)
            if change == 'entry': bad['apply_returned'] = True
            elif change == 'count': bad['planned'] = True
            elif change == 'duplicate_hash': bad['duplicates'][0]['receipt_sha256'] = '0'*64
            elif change == 'prior': bad['prior_receipt_count'] = 1
            else: bad['receipts'][0]['controls_after'] = {}
            with self.assertRaises((ValueError, KeyError, TypeError)): M._log_variants_audit(M.canonical(bad), s)

    def test_actual_resolution_exact_raw_slices_and_rehashed_cache(self):
        with tempfile.TemporaryDirectory(dir=Path.cwd()/'build') as folder:
            root = Path(folder);row = row_for();sources = [source(root, 'actual.jsonl', [row]), source(root, 'plans.jsonl', [plan(row)], 'plans'), source(root, 'intents.jsonl', [intent(row)], 'intents')]
            index = M.Index(root/'own.sqlite')
            try:
                m = manifest(root, sources);first = index.run(m);state = first['per_module'][MODULE]
                self.assertEqual(state['distinct_actual_receipts'], 1);self.assertEqual(state['intents_with_terminal_outcome'], 1);self.assertEqual(state['blocking_gaps'], 0)
                self.assertFalse(state['no_prior_calls_within_declared_inventory'])
                self.assertEqual(index.db.execute("SELECT record_sha256 FROM observations WHERE status='actual_outcome'").fetchone()[0], M.sha((root/'actual.jsonl').read_bytes()))
                second = index.run(m);self.assertEqual(second['cache_hits'], 3);self.assertEqual(first['per_module'], second['per_module'])
            finally: index.db.close()

    def test_unresolved_partition_and_conflicting_module_block_absence(self):
        with tempfile.TemporaryDirectory(dir=Path.cwd()/'build') as folder:
            root = Path(folder);row = row_for();entered = source(root, 'intent.jsonl', [intent(row)], 'intents');entered['runtime'] = {'different': True}
            bad = row_for();bad['module'] = OTHER
            sources = [source(root, 'actual.jsonl', [row]), entered, source(root, 'bad.jsonl', [bad])]
            index = M.Index(root/'own.sqlite')
            try:
                result = index.run(manifest(root, sources, [MODULE, OTHER]))
                for name in (MODULE, OTHER): self.assertGreater(result['per_module'][name]['blocking_gaps'], 0);self.assertFalse(result['per_module'][name]['no_prior_calls_within_declared_inventory'])
            finally: index.db.close()


if __name__ == '__main__': unittest.main()
