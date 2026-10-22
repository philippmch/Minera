"""Synthetic offline integrity tests; no reference imports or algorithm calls."""

import base64
import gzip
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('receipt_index', Path(__file__).parents[1] / 'retained_public_receipt_index.py')
M = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(M)
MODULE = 'pm4py.algo.filtering.log.ltl.ltl_checker'
OTHER = 'pm4py.algo.filtering.pandas.ltl.ltl_checker'


def outcome():
    snapshot = {k: {} for k in ['attributes', 'extensions', 'classifiers', 'omni_present', 'properties']}
    snapshot['traces'] = [{'events': [['A', 9007199254740993, {'bytes': '00ff'}]]}]
    receipt = {'module': MODULE, 'function': 'eventually_follows',
               'args': {'type': 'list', 'values': [{'type': 'log', 'snapshot': snapshot}, ['A']]},
               'kwargs': {'type': 'dict', 'items': [['parameters', None]]}, 'call_shape': 'positional_args_keyword_controls'}
    return {'receipt': receipt, 'receipt_sha256': M.sha(M.canonical(receipt)), 'result': None}


class ReceiptIndexTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.index = M.Index(self.root / 'index.sqlite')
        self.manifest = {'schema': M.MANIFEST, 'root': str(self.root),
                         'scope': 'synthetic explicit observation files only',
                         'inventory_complete': True, 'modules': [MODULE, OTHER], 'sources': []}

    def tearDown(self):
        self.index.db.close()
        self.temporary.cleanup()

    def source(self, path, data, **changes):
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        spec = {'path': path, 'bytes': len(data), 'sha256': M.sha(data),
                'provenance': 'own-public-observation', 'format': 'jsonl',
                'adapter': 'receipt-v1', 'receipt_schema': M.SCHEMAS['receipt-v1'][0],
                'reference': {'commit': 'opaque-public-pin'},
                'runtime': {'python': '3.11'}, 'role': 'observations', **changes}
        self.manifest['sources'].append(spec)
        if 'adapter' in changes and changes['adapter'] in M.SCHEMAS and 'receipt_schema' not in changes:
            spec['receipt_schema'] = M.SCHEMAS[changes['adapter']][0]
        return spec

    def run_index(self):
        return self.index.run(self.manifest)

    def result(self, module=MODULE):
        return self.run_index()['per_module'][module]

    def test_modern_receipt_plus_outcome_and_exact_record_hash(self):
        raw = b'\xef\xbb\xbf' + json.dumps(outcome(), indent=None).encode() + b'\r\n'
        self.source('records.jsonl', raw)
        result = self.result()
        self.assertEqual(result['distinct_actual_receipts'], 1)
        self.assertFalse(result['no_prior_calls_within_declared_inventory'])
        self.assertTrue(result['complete_within_declared_inventory'])
        self.assertEqual(self.index.db.execute('SELECT record_sha256 FROM observations').fetchone()[0], M.sha(raw))

    def test_legacy_pandas_exact_original_hash(self):
        source = {'kind': 'pandas.DataFrame', 'columns': ['value'], 'column_names': [], 'index': ['z', 'z'], 'dtypes': ['Int64'], 'categorical_dtypes': [None], 'attrs': {}, 'rows': [[1], [2]]}
        receipt = {'module': OTHER, 'method': 'A_next_B_next_C', 'input': source,
                   'args': {'type': 'list', 'values': ['A', 'B', 'C']}, 'kwargs': {'type': 'dict', 'items': []}, 'call_shape': 'dataframe-positional-plus-explicit-args-and-keywords'}
        row = {'module': OTHER, 'method': receipt['method'], 'input_before': receipt['input'],
               'args_before': receipt['args'], 'kwargs_before': receipt['kwargs'],
               'call_shape': receipt['call_shape'], 'receipt_sha256': M.sha(M.canonical(receipt)), 'error': {'message': 'opaque'}}
        self.source('pandas.jsonl', M.canonical(row), adapter='pandas-before-v1')
        self.assertEqual(self.result(OTHER)['distinct_actual_receipts'], 1)
        stored = self.index.db.execute('SELECT canonical FROM observations').fetchone()[0]
        self.assertEqual(stored, M.canonical(receipt))

    def test_polars_original_omits_args(self):
        row = outcome()
        row['receipt'].pop('args')
        row['receipt'].update(input={'columns': ['value'], 'dtypes': ['String'], 'shape': [1, 1], 'rows': [['A']]}, category_observables=[], input_kind='LazyFrame', call_shape='source_positional_all_options_keyword')
        row['receipt_sha256'] = M.sha(M.canonical(row['receipt']))
        self.source('polars.jsonl', M.canonical(row), adapter='polars-receipt-v1')
        self.assertEqual(self.result()['distinct_actual_receipts'], 1)

    def test_corrupt_original_hash_blocks_every_affected_certificate(self):
        row = outcome()
        row['receipt']['args']['values'][0]['snapshot']['traces'][0]['events'][0][1] += 1
        self.source('bad.jsonl', M.canonical(row))
        self.assertEqual(self.result()['blocking_gaps'], 1)
        self.assertFalse(self.result()['complete_within_declared_inventory'])

    def test_plans_never_count_as_outcomes(self):
        row = outcome()
        del row['result']
        self.source('plans.jsonl', M.canonical(row), role='plans')
        result = self.result()
        self.assertEqual(result['distinct_actual_receipts'], 0)
        self.assertTrue(result['no_prior_calls_within_declared_inventory'])

    def test_intent_missing_module_resolves_only_by_same_partition_terminal(self):
        row = outcome()
        intent = {'function': row['receipt']['function'], 'receipt_sha256': row['receipt_sha256']}
        self.source('intents.jsonl', M.canonical(intent), role='intents')
        self.source('records.jsonl', M.canonical(row))
        result = self.result()
        self.assertEqual(result['intents_with_terminal_outcome'], 1)
        self.assertEqual(result['blocking_gaps'], 0)
        self.assertFalse(result['no_prior_calls_within_declared_inventory'])

    def test_unknown_intent_blocks_all_modules_and_different_pin_does_not_resolve(self):
        row = outcome()
        self.source('intents.jsonl', M.canonical({'receipt_sha256': row['receipt_sha256']}), role='intents')
        self.source('records.jsonl', M.canonical(row), runtime={'python': 'different'})
        report = self.run_index()
        self.assertEqual(report['per_module'][MODULE]['blocking_gaps'], 1)
        self.assertEqual(report['per_module'][OTHER]['blocking_gaps'], 1)

    def test_full_entered_without_outcome_stays_reserved_even_after_source_deleted(self):
        row = outcome()
        del row['result']
        self.source('intent.jsonl', M.canonical(row), role='intents')
        self.assertFalse(self.result()['complete_within_declared_inventory'])
        (self.root / 'intent.jsonl').unlink()
        self.manifest['sources'] = []
        self.assertEqual(self.result()['historical_observation_copies']['entered_without_outcome'], 1)

    def test_digest_only_plans_are_not_full_receipt_or_intent_resolution(self):
        digest = outcome()['receipt_sha256']
        self.source('dedup.json', M.canonical({'receipts': [{'receipt_sha256': digest}]}),
                    format='json', records_key='receipts', adapter='digest-plan-v1', role='plans')
        self.source('intent.jsonl', M.canonical({'receipt_sha256': digest}), role='intents')
        self.assertEqual(self.result()['blocking_gaps'], 1)
        self.assertEqual(self.index.db.execute("SELECT canonical FROM observations WHERE status='digest_only_plan'").fetchone()[0], None)

    def test_digest_plan_cannot_hide_modern_outcome(self):
        self.source('wrong-role.jsonl', M.canonical(outcome()), adapter='digest-plan-v1', role='plans')
        self.assertEqual(self.result()['blocking_gaps'], 1)

    def test_scalar_value_result_is_an_actual_outcome(self):
        row = outcome()
        del row['result']
        row['value_result'] = {'type': 'bool', 'value': False}
        self.source('helper.jsonl', M.canonical(row))
        self.assertEqual(self.result()['distinct_actual_receipts'], 1)

    def test_archive_utf8_crlf_base64_and_unlisted_source_not_decoded(self):
        payload = M.canonical(outcome()) + b'\r\n'
        members = []
        allowlist = []
        for name, encoding in [('build/utf8.jsonl', 'utf8'), ('build/base64.jsonl', 'base64')]:
            binding = {'path': name, 'bytes': len(payload), 'sha256': M.sha(payload)}
            members.append({**binding, **({'utf8': payload.decode()} if encoding == 'utf8' else {'encoding': 'base64', 'data': base64.b64encode(payload).decode()})})
            allowlist.append({**binding, 'format': 'jsonl', 'adapter': 'receipt-v1', 'role': 'observations',
                              'reference': 'pin', 'runtime': 'runtime', 'receipt_schema': M.SCHEMAS['receipt-v1'][0], 'provenance': 'own-public-observation'})
        members.append({'path': 'reference-runtime/pm4py/source.py', 'bytes': 3, 'sha256': 'opaque', 'encoding': 'base64', 'data': 'DO NOT DECODE'})
        self.source('archive.json.gz', gzip.compress(M.canonical({'members': members})),
                    format='gzip-envelope-json', records_key='members', members=allowlist)
        result = self.result()
        self.assertEqual(result['distinct_actual_receipts'], 1)
        self.assertEqual(result['historical_observation_copies']['actual_outcome'], 2)
        self.assertEqual(self.index.db.execute("SELECT COUNT(*) FROM members WHERE disposition='unlisted-not-decoded'").fetchone()[0], 1)

    def test_archive_member_binding_failure_is_gap(self):
        payload = M.canonical(outcome())
        binding = {'path': 'safe.jsonl', 'bytes': len(payload), 'sha256': M.sha(payload)}
        member = {**binding, 'utf8': payload.decode() + ' '}
        self.source('archive.json.gz', gzip.compress(M.canonical([member])),
                    format='gzip-envelope-json', members=[{**binding, 'format': 'jsonl'}])
        self.assertGreater(self.result()['blocking_gaps'], 0)

    def test_changed_binding_cannot_hit_cache_and_moved_copy_keeps_provenance(self):
        raw = M.canonical(outcome())
        self.source('one.jsonl', raw)
        self.assertEqual(self.run_index()['cache_hits'], 0)
        spec = self.source('two.jsonl', raw)
        self.assertEqual(self.run_index()['cache_hits'], 2)
        self.assertEqual(self.index.db.execute('SELECT COUNT(*) FROM observations').fetchone()[0], 1)
        (self.root / spec['path']).write_bytes(raw + b' ')
        report = self.run_index()
        self.assertEqual(report['cache_hits'], 1)
        self.assertFalse(report['per_module'][MODULE]['complete_within_declared_inventory'])

    def test_unknown_provenance_adapter_and_record_shapes_fail_closed(self):
        self.source('unknown.jsonl', M.canonical(outcome()), provenance='untrusted')
        self.source('adapter.jsonl', M.canonical(outcome()), adapter='unknown')
        self.source('shape.jsonl', b'{"module":"' + MODULE.encode() + b'","receipt_sha256":"none"}')
        self.assertGreaterEqual(self.result()['blocking_gaps'], 3)

    def test_path_traversal_reference_and_symlink_escape_are_rejected(self):
        for value in ['../bad.json', '/bad.json', 'C:/bad.json', 'build/pm4py/a.json',
                      'build/site-packages/a.json', 'x\\a.json', 'build/a.py', 'build/./a.json']:
            with self.subTest(value=value), self.assertRaises(ValueError):
                M.safe_path(value)

    def test_excluded_inventory_root_rejected_before_read(self):
        self.manifest['root'] = str(self.root / 'reference-runtime')
        with self.assertRaisesRegex(ValueError, 'inventory root'):
            self.run_index()

    def test_json_exact_array_record_slices_and_duplicate_key_rejection(self):
        raw = b'{"records": [  {"a":1},\n { "b" : 2 } ]}'
        self.assertEqual([x[1] for x in M.records(raw, 'json', 'records')], [b'{"a":1}', b'{ "b" : 2 }'])
        with self.assertRaises(ValueError):
            list(M.records(b'{"receipt":1,"receipt":2}', 'json'))
        with self.assertRaises(ValueError):
            M.load_json('{"value":1e999}')

    def test_unknown_original_schema_or_missing_source_never_certifies(self):
        self.source('unknown.jsonl', M.canonical(outcome()), receipt_schema='unknown')
        row = outcome()
        row['receipt']['args'] = {'type': 'list', 'values': []}
        row['receipt_sha256'] = M.sha(M.canonical(row['receipt']))
        self.source('missing-source.jsonl', M.canonical(row))
        self.assertEqual(self.result()['blocking_gaps'], 2)

    def test_conflicting_or_malformed_module_hints_block_both_scopes(self):
        row = outcome()
        row['module'] = OTHER
        self.source('conflicting.jsonl', M.canonical(row))
        self.source('malformed.jsonl', M.canonical({'module': 'unqualified'}))
        report = self.run_index()
        self.assertEqual(report['per_module'][MODULE]['blocking_gaps'], 2)
        self.assertEqual(report['per_module'][OTHER]['blocking_gaps'], 2)

    def test_concurrent_source_mutation_during_generation_blocks_certificate(self):
        self.source('source.jsonl', M.canonical(outcome()))
        original = self.index.ingest
        def mutating_ingest(root, spec):
            result = original(root, spec)
            (root / spec['path']).write_bytes(b'changed')
            return result
        self.index.ingest = mutating_ingest
        self.assertFalse(self.result()['complete_within_declared_inventory'])

    def test_old_parser_history_requires_reclassification_before_certificate(self):
        self.source('source.jsonl', M.canonical(outcome()))
        self.assertTrue(self.result()['complete_within_declared_inventory'])
        key, config = self.index.db.execute('SELECT content_key,config FROM containers').fetchone()
        config = json.loads(config)
        config['parser_sha256'] = 'older-parser'
        self.index.db.execute('UPDATE containers SET config=? WHERE content_key=?', (json.dumps(config), key))
        self.index.db.commit()
        self.manifest['sources'] = []
        result = self.run_index()
        self.assertEqual(result['historical_containers_pending_parser_upgrade'], 1)
        self.assertFalse(result['per_module'][MODULE]['complete_within_declared_inventory'])

    def test_saved_canonical_receipt_corruption_blocks_certificate(self):
        self.source('source.jsonl', M.canonical(outcome()))
        self.assertTrue(self.result()['complete_within_declared_inventory'])
        self.index.db.execute('UPDATE observations SET canonical=?', (b'{}',))
        self.index.db.commit()
        self.assertFalse(self.result()['complete_within_declared_inventory'])

    def test_changed_parser_identity_blocks_certificate(self):
        self.index.parser_sha256 = 'different-executing-parser'
        self.assertFalse(self.result()['complete_within_declared_inventory'])

    def test_incomplete_inventory_never_certifies_absence(self):
        self.manifest['inventory_complete'] = False
        report = self.run_index()
        self.assertFalse(report['catalogue_audit_complete'])
        self.assertFalse(report['per_module'][MODULE]['no_prior_calls_within_declared_inventory'])


if __name__ == '__main__':
    unittest.main()
