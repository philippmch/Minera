"""Streaming public-only inventory tests, with forbidden payload decoder guards."""

import base64
import gzip
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import tracemalloc
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location('observation_inventory', Path(__file__).parents[1] / 'retained_public_observation_inventory.py')
M = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(M)
MODULE = 'pm4py.algo.filtering.log.ltl.ltl_checker'


def row():
    return {'module': MODULE, 'receipt': {'module': MODULE, 'function': 'eventually_follows',
            'args': {'type': 'list', 'values': []}, 'kwargs': {}, 'call_shape': 'public-call'},
            'receipt_sha256': 'a' * 64, 'result': None}


def member(path, payload, encoding='utf8', path_last=False):
    binding = {'path': path, 'bytes': len(payload), 'sha256': M.digest(payload)}
    text = payload.decode() if encoding == 'utf8' else base64.b64encode(payload).decode()
    content = {'utf8': text} if encoding == 'utf8' else ({'encoding': 'base64', 'data': text} if encoding == 'data' else {'base64': text})
    return {**content, **binding} if path_last else {**binding, **content}


class BoundedStream(io.BytesIO):
    def __init__(self, data, maximum):
        super().__init__(data)
        self.maximum = maximum
        self.largest = 0

    def read(self, size=-1):
        if size < 0 or size > self.maximum:
            raise AssertionError('unbounded source read')
        self.largest = max(self.largest, size)
        return super().read(size)


class ObservationInventoryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.events = []

    def tearDown(self):
        self.temp.cleanup()

    def spec(self, data, layout='discover', format='json', **changes):
        path = 'build/own.json.gz' if format.startswith('gzip-') else 'build/own.jsonl' if format == 'jsonl' else 'build/own.json'
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        return {'path': path, 'bytes': len(data), 'sha256': M.digest(data),
                'format': format, 'layout': layout, 'provenance': 'own-public-observation', **changes}

    def run_source(self, spec, chunk=7):
        manifest = {'schema': M.MANIFEST, 'root': str(self.root), 'scope': 'explicit synthetic containers only', 'sources': [spec]}
        return M.run(manifest, self.events.append, chunk)

    def admission(self, archive_member, **changes):
        return {k: archive_member[k] for k in ['path', 'bytes', 'sha256']} | {
            'format': 'jsonl', 'layout': 'records', 'provenance': 'own-public-observation', **changes}

    def test_chunked_public_record_wrappers_and_outcome_before_plan(self):
        data = M.canonical({'schema': 'minera-own-observation-v1', 'records': [row(), row()]})
        report = self.run_source(self.spec(data, layout='records', records_key='records', role='plans'), 1)
        records = [e for e in self.events if e['kind'] == 'record']
        self.assertEqual([e['classification'] for e in records], ['actual_outcome'] * 2)
        self.assertEqual(report['gaps'], 0)
        self.assertFalse(report['admission_complete'])

    def test_discovery_never_decodes_payload_even_when_path_after_data(self):
        forbidden = b'FORBIDDEN_IMPLEMENTATION_SENTINEL\n' * 1000
        envelope = member('reference-runtime/pm4py/implementation.py', forbidden, 'utf8', True)
        data = gzip.compress(M.canonical({'schema': 'minera-own-archive-v1', 'members': [envelope]}))
        with patch.object(M, 'decode_payload', side_effect=AssertionError('forbidden payload decode')):
            report = self.run_source(self.spec(data, format='gzip-json'), 3)
        self.assertEqual(report['gaps'], 0)
        event = next(e for e in self.events if e['kind'] == 'member')
        self.assertFalse(event['admitted'])
        self.assertNotIn('FORBIDDEN', json.dumps(self.events))

    def test_unlisted_payload_not_spooled_or_decoded_in_admitted_envelope(self):
        envelope = member('reference-runtime/pm4py/a.py', b'FORBIDDEN', 'base64')
        with patch.object(M, 'decode_payload', side_effect=AssertionError('decode')):
            report = self.run_source(self.spec(M.canonical([envelope]), layout='envelope', members=[]))
        self.assertEqual(report['gaps'], 0)

    def test_all_recognized_payload_encodings_and_path_order(self):
        for encoding in ['utf8', 'data', 'base64']:
            for path_last in [False, True]:
                with self.subTest(encoding=encoding, path_last=path_last):
                    self.events.clear()
                    payload = M.canonical(row()) + b'\r\n'
                    envelope = member('build/records.jsonl', payload, encoding, path_last)
                    spec = self.spec(M.canonical({'members': [envelope]}), layout='envelope', members_key='members', members=[self.admission(envelope)])
                    report = self.run_source(spec, 1)
                    self.assertEqual(report['gaps'], 0)
                    self.assertEqual(sum(e['kind'] == 'record' for e in self.events), 1)

    def test_admitted_text_preserves_escaped_unicode_crlf_nul_and_backslash(self):
        public = row() | {'value_result': {'literal': 'Ω\n\x00\\\"\U0001f64f'}}
        payload = (json.dumps(public, ensure_ascii=False) + '\r\n').encode()
        envelope = member('build/public.jsonl', payload, path_last=True)
        data = json.dumps([envelope], ensure_ascii=True).encode()
        report = self.run_source(self.spec(data, layout='envelope', members=[self.admission(envelope)]), 2)
        self.assertEqual(report['gaps'], 0)

    def test_nested_unknown_payload_requires_gap_without_decoding(self):
        payload = M.canonical(row())
        envelope = member('build/records.jsonl', payload)
        envelope['utf8'] = {'unknown': {'data': 'FORBIDDEN'}}
        with patch.object(M, 'decode_payload', side_effect=AssertionError('decode')):
            report = self.run_source(self.spec(M.canonical([envelope]), layout='envelope', members=[self.admission(envelope)]))
        self.assertGreater(report['gaps'], 0)
        self.assertNotIn('FORBIDDEN', json.dumps(self.events))

    def test_strict_duplicate_nonfinite_truncation_escape_and_utf8(self):
        bad = [b'{"a":1,"a":2}', b'{"a":1e999}', b'{"a":NaN}', b'{"a":',
               b'{"a":"bad\\x"}', b'{"a":"bad\xff"}', b'{"a":[1,]}',
               b'{"a":01}', b'{"a":true} trailing', b'{"utf8":"x\\u00xz"}']
        for data in bad:
            with self.subTest(data=data):
                self.assertGreater(self.run_source(self.spec(data))['gaps'], 0)

    def test_skipped_nested_objects_still_validate_duplicate_keys(self):
        data = b'{"members":[{"path":"a.py","data":{"x":1,"x":2}}]}'
        self.assertGreater(self.run_source(self.spec(data))['gaps'], 0)

    def test_jsonl_physical_boundaries_and_mixed_metadata_header(self):
        envelope = member('build/public.jsonl', M.canonical(row()) + b'\n')
        header = {'schema': 'minera-own-acceptance-v1', 'kind': 'acceptance'}
        data = M.canonical(header) + b'\r\n' + M.canonical(envelope) + b'\n'
        report = self.run_source(self.spec(data, layout='envelope', format='jsonl',
                    metadata_schemas=[header['schema']], members=[self.admission(envelope)]))
        self.assertEqual(report['gaps'], 0)
        self.assertEqual(sum(e['kind'] == 'metadata_header' for e in self.events), 1)
        invalid = [M.canonical(row()) + b' ' + M.canonical(row()), b'{\n"result":null\n}']
        for data in invalid:
            with self.subTest(data=data):
                self.assertGreater(self.run_source(self.spec(data, layout='records', format='jsonl'))['gaps'], 0)

    def test_corrupt_member_and_container_bindings_block(self):
        envelope = member('build/records.jsonl', M.canonical(row()))
        admitted = self.admission(envelope)
        admitted['sha256'] = '0' * 64
        self.assertGreater(self.run_source(self.spec(M.canonical([envelope]), layout='envelope', members=[admitted]))['gaps'], 0)
        spec = self.spec(M.canonical(row()), layout='records')
        spec['bytes'] += 1
        self.assertGreater(self.run_source(spec)['gaps'], 0)

    def test_excluded_traversal_and_unknown_provenance(self):
        for value in ['../a.json', '/a.json', 'C:/a.json', 'x\\a.json', 'pm4py/a.json', 'x/site-packages/a.json']:
            with self.subTest(value=value), self.assertRaises(ValueError):
                M.safe_path(value)
        spec = self.spec(M.canonical(row()), layout='records', provenance='unknown')
        self.assertGreater(self.run_source(spec)['gaps'], 0)

    def test_unknown_record_and_possible_terminal_are_gaps_not_plans(self):
        for value in [{'module': MODULE, 'function': 'f', 'output': 1}, {'unrecognized': True},
                      {'module': MODULE, 'function': 'f', 'input_after': []},
                      {'module': MODULE, 'function': 'f', 'apply_returned': True},
                      {'module': MODULE, 'function': 'f', 'controls_after': {}}]:
            with self.subTest(value=value):
                report = self.run_source(self.spec(M.canonical(value), layout='records', role='plans'))
                self.assertGreater(report['gaps'], 0)
        for field in ['value_result', 'result_value', 'error', 'result']:
            value = row()
            del value['result']
            value[field] = None
            self.assertEqual(M.classify(value, 'plans')[0], 'actual_outcome')

    def test_duplicate_member_and_missing_admitted_member_are_gaps(self):
        envelope = member('build/records.jsonl', M.canonical(row()))
        self.assertGreater(self.run_source(self.spec(M.canonical([envelope, envelope]), layout='envelope'))['gaps'], 0)
        self.assertGreater(self.run_source(self.spec(b'[]', layout='envelope', members=[self.admission(envelope)]))['gaps'], 0)

    def test_header_cannot_hide_possible_actual_outcomes(self):
        header = {'schema': 'minera-proof-v1', 'module': MODULE, 'function': 'apply', 'result': None}
        report = self.run_source(self.spec(M.canonical(header), format='jsonl'))
        self.assertGreater(report['gaps'], 0)
        self.assertFalse(any(event['kind'] == 'metadata_header' for event in self.events))

    def test_unknown_legacy_envelope_reports_only_key_names(self):
        unknown = {'kind': 'member', 'legacy_path': 'do-not-infer.jsonl',
                   'legacy_payload': 'FORBIDDEN_IMPLEMENTATION', 'sha256': 'opaque'}
        with patch.object(M, 'decode_payload', side_effect=AssertionError('decode')):
            report = self.run_source(self.spec(M.canonical(unknown), format='jsonl'))
        event = next(event for event in self.events if event['kind'] == 'member')
        self.assertEqual(event['envelope_keys'], sorted(unknown))
        self.assertEqual(report['gaps'], 1)
        self.assertNotIn('FORBIDDEN_IMPLEMENTATION', json.dumps(self.events))
        self.assertNotIn('do-not-infer.jsonl', json.dumps(self.events))

    def legacy(self, payload, path_last=False):
        binding = {'kind': 'member', 'name': 'build/public.jsonl', 'sha256': M.digest(payload)}
        encoded = {'data_base64': base64.b64encode(payload).decode()}
        return {**encoded, **binding} if path_last else {**binding, **encoded}

    def legacy_spec(self, envelope, members=None, **changes):
        header = {'schema_version': 1, 'kind': 'manifest', 'summary': {'retained': 1}}
        data = M.canonical(header) + b'\n' + M.canonical(envelope) + b'\n'
        return self.spec(data, format='jsonl', envelope_schema='legacy-name-data-base64-v1',
                         metadata_kind='manifest', metadata_schema_version=1,
                         members=members or [], **changes)

    def legacy_admission(self, payload):
        return {'path': 'build/public.jsonl', 'bytes': len(payload), 'sha256': M.digest(payload),
                'format': 'jsonl', 'layout': 'records', 'provenance': 'own-public-observation'}

    def test_explicit_legacy_discovery_never_decodes_and_reports_absent_original_size(self):
        payload = M.canonical(row()) + b'\r\n'
        envelope = self.legacy(payload, path_last=True)
        with patch.object(M, 'decode_payload', side_effect=AssertionError('decode')):
            report = self.run_source(self.legacy_spec(envelope))
        self.assertEqual(report['gaps'], 0)
        event = next(event for event in self.events if event['kind'] == 'member')
        self.assertEqual(event['path'], 'build/public.jsonl')
        self.assertTrue(event['original_size_unavailable'])
        self.assertNotIn('bytes', event)
        self.assertFalse(event['admitted'])

    def test_legacy_admission_binds_root_size_and_original_sha_for_both_field_orders(self):
        payload = M.canonical(row()) + b'\r\n'
        for path_last in [False, True]:
            with self.subTest(path_last=path_last):
                self.events.clear()
                spec = self.legacy_spec(self.legacy(payload, path_last), [self.legacy_admission(payload)], layout='envelope')
                report = self.run_source(spec, 1)
                self.assertEqual(report['gaps'], 0)
                event = next(event for event in self.events if event['kind'] == 'member_binding')
                self.assertEqual(event['bytes'], len(payload))
                self.assertEqual(event['size_source'], 'reviewed-allowlist')
                self.assertTrue(event['original_size_unavailable'])
                self.assertEqual(sum(event['kind'] == 'record' for event in self.events), 1)

    def test_legacy_missing_root_size_blocks_before_decode(self):
        payload = M.canonical(row())
        admission = self.legacy_admission(payload)
        del admission['bytes']
        with patch.object(M, 'decode_payload', side_effect=AssertionError('decode')):
            report = self.run_source(self.legacy_spec(self.legacy(payload), [admission], layout='envelope'))
        self.assertGreater(report['gaps'], 0)

    def test_legacy_corrupt_original_sha_wrong_kind_and_ambiguous_fields_are_gaps(self):
        payload = M.canonical(row())
        for change in [{'sha256': '0' * 64}, {'kind': 'file'}, {'bytes': len(payload)}, {'path': 'other.jsonl'}]:
            with self.subTest(change=change):
                envelope = self.legacy(payload) | change
                report = self.run_source(self.legacy_spec(envelope, [self.legacy_admission(payload)], layout='envelope'))
                self.assertGreater(report['gaps'], 0)

    def test_legacy_header_requires_explicit_exact_kind_version_and_no_entry_evidence(self):
        for header in [{'kind': 'manifest', 'schema_version': 2}, {'kind': 'file', 'schema_version': 1},
                       {'kind': 'manifest', 'schema_version': True}, {'kind': 'manifest', 'schema_version': 1, 'apply_returned': True}]:
            with self.subTest(header=header):
                spec = self.spec(M.canonical(header), format='jsonl', envelope_schema='legacy-name-data-base64-v1',
                                 metadata_kind='manifest', metadata_schema_version=1)
                self.assertGreater(self.run_source(spec)['gaps'], 0)
        undeclared = self.spec(b'{"kind":"manifest","schema_version":1}', format='jsonl', envelope_schema='legacy-name-data-base64-v1')
        self.assertGreater(self.run_source(undeclared)['gaps'], 0)

    def test_legacy_default_never_automatically_maps_or_admits(self):
        payload = M.canonical(row())
        spec = self.spec(M.canonical(self.legacy(payload)), format='jsonl')
        with patch.object(M, 'decode_payload', side_effect=AssertionError('decode')):
            report = self.run_source(spec)
        self.assertGreater(report['gaps'], 0)
        event = next(event for event in self.events if event['kind'] == 'member')
        self.assertNotIn('path', event)
        self.assertEqual(event['payload_fields'], {})

    def test_member_extras_cannot_hide_call_or_entry_evidence(self):
        payload = M.canonical(row())
        for extras in [{'module': MODULE, 'function': 'f', 'result': None},
                       {'apply_returned': True}, {'unexpected_extra': 'opaque'}]:
            with self.subTest(legacy=extras):
                spec = self.legacy_spec(self.legacy(payload) | extras, [self.legacy_admission(payload)], layout='envelope')
                with patch.object(M, 'decode_payload', side_effect=AssertionError('decode')):
                    self.assertGreater(self.run_source(spec)['gaps'], 0)
        envelope = member('build/public.jsonl', payload) | {'module': MODULE, 'function': 'f', 'result': None, 'apply_returned': True}
        with patch.object(M, 'decode_payload', side_effect=AssertionError('decode')):
            self.assertGreater(self.run_source(self.spec(M.canonical([envelope]), layout='envelope', members=[self.admission(envelope)]))['gaps'], 0)

    def chunk(self, indexed=False, index=0):
        if indexed:
            return {'path': 'public/full.json', 'chunk': index, 'chunks': 2,
                    'bytes': M.CHUNK_SIZE if index == 0 else 7, 'sha256': 'a' * 64,
                    'file_sha256': 'b' * 64, 'encoding': 'base64', 'data': 'FORBIDDEN_OPAQUE_PAYLOAD'}
        return {'member': 'public/full.json', 'offset': index * M.CHUNK_SIZE,
                'bytes': M.CHUNK_SIZE + 7, 'sha256': 'b' * 64, 'data': 'FORBIDDEN_OPAQUE_PAYLOAD'}

    def chunk_spec(self, rows, indexed=False, **changes):
        return self.spec(b'\n'.join(M.canonical(item) for item in rows), format='jsonl',
                         envelope_schema='indexed-base64-chunks-v1' if indexed else 'offset-full-member-base64-chunks-v1',
                         **changes)

    def test_chunk_metadata_keeps_distinct_size_hash_semantics_and_paths_repeat(self):
        for indexed in [False, True]:
            with self.subTest(indexed=indexed):
                self.events.clear()
                spec = self.chunk_spec([self.chunk(indexed), self.chunk(indexed, 1)], indexed)
                with patch.object(M, 'decode_payload', side_effect=AssertionError('decode')), patch.object(M.tempfile, 'SpooledTemporaryFile', side_effect=AssertionError('spool')):
                    report = self.run_source(spec, 1)
                self.assertEqual(report['gaps'], 0)
                chunks = [event for event in self.events if event['kind'] == 'chunk']
                self.assertEqual(len(chunks), 2)
                self.assertFalse(any(event['payload_hash_verified'] or event['whole_member_complete'] for event in chunks))
                self.assertNotIn('member', report['counts'])
                if indexed:
                    self.assertEqual(chunks[1]['declared_chunk_bytes'], 7)
                    self.assertEqual(chunks[1]['declared_chunk_sha256'], 'a' * 64)
                    self.assertEqual(chunks[1]['declared_full_sha256'], 'b' * 64)
                    self.assertNotIn('declared_full_bytes', chunks[1])
                else:
                    self.assertEqual(chunks[1]['declared_full_bytes'], M.CHUNK_SIZE + 7)
                    self.assertNotIn('declared_chunk_bytes', chunks[1])
                self.assertNotIn('FORBIDDEN_OPAQUE_PAYLOAD', json.dumps(self.events))

    def test_chunk_duplicates_missing_fields_types_and_entry_fields_are_gaps(self):
        for indexed in [False, True]:
            value = self.chunk(indexed)
            changes = [{'bytes': True}, {'sha256': 'bad'}, {'apply_returned': True}, {'data': {'nested': 'unknown'}}]
            changes += [{'chunk': True}, {'chunks': 0}, {'encoding': 'utf8'}] if indexed else [{'offset': True}, {'offset': -1}]
            for change in changes:
                with self.subTest(indexed=indexed, change=change):
                    self.assertGreater(self.run_source(self.chunk_spec([value | change], indexed))['gaps'], 0)
            missing = dict(value)
            del missing['sha256']
            self.assertGreater(self.run_source(self.chunk_spec([missing], indexed))['gaps'], 0)
            self.assertGreater(self.run_source(self.chunk_spec([value, value], indexed))['gaps'], 0)

    def test_chunk_schemas_reject_non_discovery_and_default_full_member_admission(self):
        for indexed in [False, True]:
            with self.subTest(indexed=indexed):
                with patch.object(M, 'decode_payload', side_effect=AssertionError('decode')):
                    self.assertGreater(self.run_source(self.chunk_spec([self.chunk(indexed)], indexed, layout='envelope'))['gaps'], 0)
        undeclared = self.chunk_spec([self.chunk(True)], True)
        del undeclared['envelope_schema']
        self.assertGreater(self.run_source(undeclared)['gaps'], 0)

    def test_direct_chunk_parse_rejects_admission_before_parser_or_payload_access(self):
        for indexed in [False, True]:
            value = self.chunk(indexed)
            data_first = {'data': value['data'], **{key: item for key, item in value.items() if key != 'data'}}
            raw = json.dumps(data_first).encode()
            schema = 'indexed-base64-chunks-v1' if indexed else 'offset-full-member-base64-chunks-v1'
            for changes in [{'layout': 'envelope'}, {'layout': 'records'},
                            {'layout': 'discover', 'members': [{'path': 'public/full.json'}]}]:
                with self.subTest(indexed=indexed, changes=changes):
                    inventory = M.Inventory(self.events.append)
                    spec = {'format': 'json', 'envelope_schema': schema, **changes}
                    with patch.object(M, 'Parser', side_effect=AssertionError('parser created')), patch.object(M.tempfile, 'SpooledTemporaryFile', side_effect=AssertionError('spool')), patch.object(M, 'decode_payload', side_effect=AssertionError('decode')):
                        with self.assertRaisesRegex(ValueError, 'metadata discovery only'):
                            inventory.parse(io.BytesIO(raw), spec)

    def test_chunk_cache_hit_preserves_digest_key(self):
        owned = Path(__file__).resolve().parents[2] / 'build/parity-closures/receipt-index-development'
        owned.mkdir(parents=True, exist_ok=True)
        temporary = tempfile.TemporaryDirectory(prefix='inventory-chunk-cache-test-', dir=owned)
        self.addCleanup(temporary.cleanup)
        cache = Path(temporary.name) / 'cache'
        for indexed in [False, True]:
            with self.subTest(indexed=indexed):
                spec = self.chunk_spec([self.chunk(indexed)], indexed)
                manifest = {'schema': M.MANIFEST, 'root': str(self.root), 'scope': 'chunk cache test', 'sources': [spec]}
                first = M.run(manifest, self.events.append, 5, cache_dir=cache)
                self.assertEqual(first['gaps'], 0)
                self.events.clear()
                with patch.object(M.Inventory, 'parse', side_effect=AssertionError('cache miss')):
                    second = M.run(manifest, self.events.append, 5, cache_dir=cache)
                self.assertEqual(second['gaps'], 0)
                hit = next(event for event in self.events if event['kind'] == 'cache_hit')
                self.assertRegex(hit['cache_key'], r'^[0-9a-f]{64}$')

    def utf8_text_member(self, name='own/public.json', text='FORBIDDEN_TEXT_PAYLOAD'):
        payload = text.encode('utf-8')
        return {'kind': 'member', 'name': name, 'byte_count': len(payload),
                'sha256': M.digest(payload), 'text': text}

    def utf8_text_spec(self, rows, **changes):
        return self.spec(b'\n'.join(M.canonical(item) for item in rows), format='jsonl',
                         envelope_schema='utf8-name-text-members-v1', **changes)

    def test_utf8_name_text_discovery_maps_metadata_without_unescaping_or_spooling(self):
        # Put the payload before its name to exercise the privacy boundary.
        value = self.utf8_text_member(text='FORBIDDEN_TEXT_PAYLOAD Ω\\\r\n\x00')
        ordered = {'text': value['text'], **{k: v for k, v in value.items() if k != 'text'}}
        original_loads = M.json.loads
        def guarded_loads(raw, *args, **kwargs):
            if b'FORBIDDEN_TEXT_PAYLOAD' in (raw.encode() if isinstance(raw, str) else raw):
                raise AssertionError('opaque text JSON-unescaped')
            return original_loads(raw, *args, **kwargs)
        with patch.object(M.json, 'loads', guarded_loads), patch.object(M, 'decode_payload', side_effect=AssertionError('decode')), patch.object(M.tempfile, 'SpooledTemporaryFile', side_effect=AssertionError('spool')):
            report = self.run_source(self.utf8_text_spec([ordered]), 1)
        self.assertEqual(report['gaps'], 0)
        event = next(event for event in self.events if event['kind'] == 'member')
        self.assertEqual((event['path'], event['bytes'], event['sha256']), (value['name'], value['byte_count'], value['sha256']))
        self.assertEqual(event['payload_fields'], {'text': 'string'})
        self.assertFalse(event['admitted'])
        self.assertFalse(event['original_size_unavailable'])
        self.assertFalse(event['payload_hash_verified'])
        self.assertTrue(event['metadata_valid'])
        self.assertNotIn('FORBIDDEN_TEXT_PAYLOAD', json.dumps(self.events))

    def test_utf8_name_text_headers_tail_and_public_summaries_remain_unadmitted(self):
        header = {'kind': 'header', 'schema': 'minera.alpha-plus.process-law.utf8-members-jsonl',
                  'version': 1, 'group': 'own-group', 'members': 1,
                  'summary': {'result': 'FORBIDDEN_SUMMARY_VALUE', 'actual_calls': 0}}
        tail = {'kind': 'tail', 'member_count': 1, 'total_member_bytes': 3, 'inventory_sha256': 'a' * 64}
        report = self.run_source(self.utf8_text_spec([header, self.utf8_text_member(), tail]))
        self.assertEqual(report['counts'], {'header_candidate': 2, 'gap': 2, 'member': 1})
        candidates = [event for event in self.events if event['kind'] == 'header_candidate']
        self.assertEqual([event['declared_metadata']['kind'] for event in candidates], ['header', 'tail'])
        self.assertNotIn('FORBIDDEN_SUMMARY_VALUE', json.dumps(self.events))
        self.assertFalse(report['admission_complete'])

    def test_utf8_name_text_rejects_non_discovery_and_any_admission(self):
        value = self.utf8_text_member()
        for changes in [{'layout': 'envelope'}, {'layout': 'records'},
                        {'members': [{'path': 'own/public.json', 'bytes': value['byte_count'], 'sha256': value['sha256']}]}]:
            with self.subTest(changes=changes), patch.object(M, 'decode_payload', side_effect=AssertionError('decode')), patch.object(M.tempfile, 'SpooledTemporaryFile', side_effect=AssertionError('spool')):
                self.assertGreater(self.run_source(self.utf8_text_spec([value], **changes))['gaps'], 0)
        direct = M.Inventory(self.events.append)
        with self.assertRaisesRegex(ValueError, 'metadata discovery only'):
            direct.parse(io.BytesIO(M.canonical(value)), {'layout': 'envelope', 'format': 'json', 'envelope_schema': 'utf8-name-text-members-v1'})

    def test_utf8_name_text_exact_fields_types_and_call_evidence_are_gaps(self):
        value = self.utf8_text_member()
        changes = [{'byte_count': True}, {'byte_count': -1}, {'byte_count': 1.5},
                   {'sha256': 'unknown'}, {'text': {'nested': 'FORBIDDEN'}},
                   {'extra': 'opaque'}, {'module': MODULE, 'function': 'apply', 'result': None},
                   {'apply_returned': True}, {'kind': 'file'}, {'bytes': value['byte_count']}]
        for change in changes:
            with self.subTest(change=change):
                self.assertGreater(self.run_source(self.utf8_text_spec([value | change]))['gaps'], 0)
        for field in ['name', 'text', 'sha256', 'byte_count']:
            with self.subTest(missing=field):
                missing = dict(value)
                del missing[field]
                self.assertGreater(self.run_source(self.utf8_text_spec([missing]))['gaps'], 0)

    def test_utf8_name_text_duplicate_and_unsafe_paths_rejected_excluded_stays_opaque(self):
        value = self.utf8_text_member()
        self.assertGreater(self.run_source(self.utf8_text_spec([value, value]))['gaps'], 0)
        for path in ['../private.json', '/private.json', 'C:/private.json', 'own\\private.json']:
            with self.subTest(path=path):
                self.assertGreater(self.run_source(self.utf8_text_spec([self.utf8_text_member(path)]))['gaps'], 0)
        self.events.clear()
        with patch.object(M, 'decode_payload', side_effect=AssertionError('decode')), patch.object(M.tempfile, 'SpooledTemporaryFile', side_effect=AssertionError('spool')):
            report = self.run_source(self.utf8_text_spec([self.utf8_text_member('pm4py/implementation.py')]))
        self.assertEqual(report['gaps'], 0)
        event = next(event for event in self.events if event['kind'] == 'member')
        self.assertEqual(event['path_disposition'], 'excluded-path')
        self.assertFalse(event['admitted'])

    def test_utf8_name_text_cache_identity_and_source_hash_validation(self):
        owned = Path(__file__).resolve().parents[2] / 'build/parity-closures/streaming-envelope-development-2'
        owned.mkdir(parents=True, exist_ok=True)
        temporary = tempfile.TemporaryDirectory(prefix='utf8-cache-test-', dir=owned)
        self.addCleanup(temporary.cleanup)
        cache = Path(temporary.name) / 'cache'
        spec = self.utf8_text_spec([self.utf8_text_member()])
        manifest = {'schema': M.MANIFEST, 'root': str(self.root), 'scope': 'UTF8 metadata cache test', 'sources': [spec]}
        self.assertEqual(M.run(manifest, self.events.append, 3, cache_dir=cache)['gaps'], 0)
        self.events.clear()
        with patch.object(M.Inventory, 'parse', side_effect=AssertionError('cache miss')):
            second = M.run(manifest, self.events.append, 3, cache_dir=cache)
        self.assertEqual(second['counts']['cache_hit'], 1)
        self.assertRegex(next(event['cache_key'] for event in self.events if event['kind'] == 'cache_hit'), r'^[0-9a-f]{64}$')
        original_spec = dict(spec)
        del spec['envelope_schema']
        default = M.run(manifest, self.events.append, 3, cache_dir=cache)
        self.assertGreater(default['gaps'], 0)
        self.assertNotIn('cache_hit', default['counts'])
        manifest['sources'] = [original_spec]
        (self.root / original_spec['path']).write_bytes(b'changed')
        with patch.object(M.Inventory, 'parse', side_effect=AssertionError('bad source parsed')):
            self.assertGreater(M.run(manifest, self.events.append, 3, cache_dir=cache)['gaps'], 0)

    def test_sources_require_json_suffix_before_open(self):
        with self.assertRaisesRegex(ValueError, 'JSON/JSONL'):
            M.resolve_source(self.root, 'build/implementation.py')

    def test_metadata_cache_reuse_moved_provenance_and_corruption_reparse(self):
        # Windows' managed redirected Temp permits writes but can reject atomic
        # rename. Exercise publication in the writable workspace used by CLI.
        owned = Path(__file__).resolve().parents[2] / 'build/parity-closures/receipt-index-development'
        owned.mkdir(parents=True, exist_ok=True)
        temporary = tempfile.TemporaryDirectory(prefix='inventory-cache-test-', dir=owned)
        self.addCleanup(temporary.cleanup)
        cache = Path(temporary.name) / 'cache'
        envelope = member('build/own.jsonl', M.canonical(row()))
        data = gzip.compress(M.canonical({'members': [envelope]}))
        spec = self.spec(data, format='gzip-json')
        manifest = {'schema': M.MANIFEST, 'root': str(self.root), 'scope': 'cache test', 'sources': [spec]}
        first = M.run(manifest, self.events.append, 5, cache_dir=cache)
        self.assertEqual(first['gaps'], 0)
        moved = self.root / 'build/moved.json.gz'
        moved.write_bytes(data)
        manifest['sources'] = [{**spec, 'path': 'build/moved.json.gz'}]
        self.events.clear()
        with patch.object(M.gzip, 'GzipFile', side_effect=AssertionError('cached source expanded')):
            second = M.run(manifest, self.events.append, 5, cache_dir=cache)
        self.assertEqual(second['counts']['cache_hit'], 1)
        self.assertTrue(all(event.get('source') == 'build/moved.json.gz' for event in self.events))
        cache_file = next(cache.glob('*.jsonl'))
        cache_file.write_bytes(b'corrupt\n')
        third = M.run(manifest, self.events.append, 5, cache_dir=cache)
        self.assertEqual(third['gaps'], 0)
        self.assertNotIn('cache_hit', third['counts'])

    def test_final_generation_revalidation_detects_early_source_change(self):
        spec = self.spec(M.canonical({'members': []}))
        original = M.Inventory.source
        def mutate(inventory, root, source, cache_dir=None):
            original(inventory, root, source, cache_dir)
            (root / source['path']).write_bytes(b'changed')
        with patch.object(M.Inventory, 'source', mutate):
            report = self.run_source(spec)
        self.assertGreater(report['gaps'], 0)

    def test_bom_and_unknown_wrapper_key(self):
        self.assertEqual(self.run_source(self.spec(b'\xef\xbb\xbf' + M.canonical(row()), layout='records'), 1)['gaps'], 0)
        self.assertGreater(self.run_source(self.spec(b'{"elsewhere":[]}', layout='records', records_key='records'))['gaps'], 0)

    def test_gzip_truncation_and_record_size_limit(self):
        data = gzip.compress(M.canonical({'members': []}))[:-5]
        self.assertGreater(self.run_source(self.spec(data, format='gzip-json'))['gaps'], 0)
        stream = io.BytesIO(M.canonical(row()))
        inventory = M.Inventory(self.events.append, chunk_size=3, max_record_bytes=20)
        with self.assertRaisesRegex(ValueError, 'record exceeds'):
            inventory.parse(stream, {'layout': 'records', 'format': 'json'})

    def test_large_skipped_strings_have_bounded_reads_and_memory(self):
        # Escaped newlines/backslashes exercise the C-regex skip path, not merely
        # an easy base64 string without escapes. Construct before measuring RAM.
        payload = ('FORBIDDEN\\n\\u0041' * 300000).encode()
        data = b'{"members":[{"utf8":"' + payload + b'","path":"pm4py/source.py","bytes":1,"sha256":"opaque"}]}'
        stream = BoundedStream(data, 4096)
        inventory = M.Inventory(self.events.append, chunk_size=4096)
        with patch.object(M, 'decode_payload', side_effect=AssertionError('decode')), patch.object(M.tempfile, 'SpooledTemporaryFile', side_effect=AssertionError('spool')):
            tracemalloc.start()
            try:
                inventory.parse(stream, {'layout': 'discover', 'format': 'json'})
                peak = tracemalloc.get_traced_memory()[1]
            finally:
                tracemalloc.stop()
        self.assertLess(peak, 2 * 1024 * 1024)
        self.assertLessEqual(stream.largest, 4096)
        self.assertEqual(inventory.gaps, 0)


if __name__ == '__main__':
    unittest.main()
