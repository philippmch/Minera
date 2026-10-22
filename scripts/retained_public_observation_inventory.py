"""Streaming, public-only inventory of explicitly selected observation containers.

Usage: python -B scripts/retained_public_observation_inventory.py MANIFEST REPORT
       --events EVENTS.jsonl [--cache CACHE_DIRECTORY] [--chunk-size 65536]

Manifest schema: minera-retained-public-observation-inventory-manifest-v1.
Fields: root, scope, sources. A source has path, bytes, sha256, format
(json/jsonl/gzip-json/gzip-jsonl), layout (discover/records/envelope), and
provenance='own-public-observation'. Discovery may use provenance='own-container'
and inventories metadata only. Record wrappers explicitly name records_key;
envelope wrappers explicitly name members_key. Envelopes admit only members
listed in members, each with its own path/bytes/sha256, provenance, format and
layout. Nested envelopes require a new root-reviewed admission, never recursion.
The explicit envelope_schema='legacy-name-data-base64-v1' maps the known own
producer's name/data_base64 fields and requires kind='member'. Its original
envelopes omit bytes: admission still requires exact bytes from the reviewed
member allowlist and verifies the original envelope SHA. Its no-path header is
admitted only by metadata_kind='manifest', metadata_schema_version=1.
The explicit offset-full-member-base64-chunks-v1 and indexed-base64-chunks-v1
schemas are discovery-only. Chunk events preserve full-member versus chunk
size/hash declarations without decoding, verification, assembly or completion.
Optional series_identity scopes duplicate chunk declarations across containers;
without it, identities are scoped only to the individual container.

Discovery validates JSON structure and inventories metadata without unescaping
or decoding archive payload strings. An unlisted payload remains opaque even
when its path appears after its data. Listed payloads are spooled until metadata
is known, bound to both envelope and manifest hashes, then parsed incrementally.
No reference import, algorithm call, source-tree discovery, or parity assertion
is performed. A report covers only the explicitly bound containers/members;
unknown schemas, encodings and records remain gaps. Keyword absence proves none
of these inventories complete. Cache reuse requires full content hashes plus
the exact parser/config identity, never timestamps alone.
"""

from __future__ import annotations

import argparse
import base64
import binascii
import codecs
import contextlib
import gzip
import hashlib
import io
import json
import math
from pathlib import Path, PurePosixPath
import re
import tempfile
import os

VERSION = 'minera-retained-public-observation-inventory-v1'
MANIFEST = 'minera-retained-public-observation-inventory-manifest-v1'
PARSER_SHA256 = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
EXCLUDED = {'pm4py', 'site-packages', 'reference-runtime',
            'heuristic-subsecond-reference-runtime', 'node_modules', '__pycache__',
            '.git', '.aws'}
PAYLOAD_KEYS = {'utf8', 'data', 'base64'}
CHUNK_SCHEMAS = {'offset-full-member-base64-chunks-v1', 'indexed-base64-chunks-v1'}
CHUNK_SIZE = 8 * 1024 * 1024
POSSIBLE_CALL_KEYS = {'receipt', 'receipt_sha256', 'function', 'method', 'module',
                      'result', 'error', 'value_result', 'result_value', 'output',
                      'return', 'returned', 'exception', 'input_after', 'args_after',
                      'kwargs_after', 'controls_after', 'apply_returned', 'entered'}
SPECIAL = re.compile(rb'["\\\x00-\x1f]')
STRING_RUN = re.compile(rb'(?:[^"\\\x00-\x1f]+|\\(?:["\\/bfnrt]|u[0-9a-fA-F]{4}))*')
NUMBER = re.compile(rb'-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?\Z')


def canonical(value):
    return json.dumps(value, ensure_ascii=True, allow_nan=False, sort_keys=True,
                      separators=(',', ':')).encode('ascii')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def hash_file(path):
    hasher = hashlib.sha256()
    size = 0
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            size += len(chunk)
            hasher.update(chunk)
    return size, hasher.hexdigest()


def require_binding(actual, spec):
    if type(spec.get('bytes')) is not int or actual[0] != spec['bytes']:
        raise ValueError('size binding mismatch')
    if not re.fullmatch('[0-9a-f]{64}', str(spec.get('sha256', ''))) or actual[1] != spec['sha256']:
        raise ValueError('SHA-256 binding mismatch')


def require_declared_binding(spec):
    if type(spec.get('bytes')) is not int or spec['bytes'] < 0:
        raise ValueError('reviewed allowlist requires exact nonnegative bytes')
    if not re.fullmatch('[0-9a-f]{64}', str(spec.get('sha256', ''))):
        raise ValueError('reviewed allowlist requires exact SHA-256')


def safe_path(value, public_json=False):
    if not isinstance(value, str) or not value or ':' in value or '\\' in value:
        raise ValueError('non-relative POSIX path')
    path = PurePosixPath(value)
    if path.is_absolute() or any(part in {'', '.', '..'} for part in value.split('/')):
        raise ValueError('absolute or traversing path')
    if any(part.casefold() in EXCLUDED for part in path.parts):
        raise ValueError('excluded implementation/dependency/private path')
    if public_json and not value.casefold().endswith(('.json', '.jsonl')):
        raise ValueError('only JSON/JSONL public members may be admitted')
    return value


def resolve_source(root, value):
    safe_path(value)
    if not value.casefold().endswith(('.json', '.jsonl', '.json.gz', '.jsonl.gz')):
        raise ValueError('source must be a JSON/JSONL container')
    path = (root / value).resolve()
    path.relative_to(root)
    if any(part.casefold() in EXCLUDED for part in path.parts):
        raise ValueError('resolved path is excluded')
    return path


class Reader:
    """Bounded binary lookahead; gzip buffers are never expanded as a whole."""
    def __init__(self, stream, chunk_size=65536, limit=64 * 1024 * 1024):
        self.stream = stream
        self.chunk_size = chunk_size
        self.buffer = b''
        self.pos = 0
        self.offset = 0
        self.line = 1
        self.capture = None
        self.limit = limit
        self.captured = 0

    def fill(self):
        if self.pos == len(self.buffer):
            self.buffer = self.stream.read(self.chunk_size)
            self.pos = 0
        return bool(self.buffer)

    def peek(self):
        return self.buffer[self.pos] if self.fill() else None

    def take(self, count=1):
        if count < 0 or count > len(self.buffer) - self.pos:
            raise ValueError('invalid buffered read')
        result = self.buffer[self.pos:self.pos + count]
        self.pos += count
        self.offset += count
        self.line += result.count(b'\n')
        if self.capture is not None:
            self.captured += count
            if self.captured > self.limit:
                raise ValueError('individual record exceeds configured byte limit')
            self.capture.update(result)
        return result

    def byte(self):
        if not self.fill():
            raise ValueError('truncated JSON')
        return self.take()[0]

    def whitespace(self, horizontal=False):
        permitted = b' \t' if horizontal else b' \t\r\n'
        while self.peek() is not None and self.peek() in permitted:
            self.take()

    def expect(self, value):
        if self.byte() != value:
            raise ValueError('unexpected JSON token at byte ' + str(self.offset))


class Parser:
    def __init__(self, stream, chunk_size=65536, max_record_bytes=64 * 1024 * 1024):
        self.r = Reader(stream, chunk_size, max_record_bytes)
        self.max_record_bytes = max_record_bytes

    def string(self, sink=None, materialize=False):
        """Validate a string lexically; opaque strings are NEVER JSON-unescaped.

        UTF-8 decoding here validates the outer JSON transport only. Escapes and
        payload encodings are left untouched unless materialize is explicit.
        """
        raw = io.BytesIO() if materialize else None
        targets = [target for target in [sink, raw] if target is not None]
        total = 0
        def write(data):
            nonlocal total
            total += len(data)
            if materialize and total > self.max_record_bytes:
                raise ValueError('JSON scalar exceeds configured byte limit')
            for target in targets:
                target.write(data)
        self.r.expect(34)
        write(b'"')
        validator = codecs.getincrementaldecoder('utf-8')('strict')
        while True:
            if not self.r.fill():
                raise ValueError('unterminated JSON string')
            remaining = self.r.buffer[self.r.pos:]
            end = STRING_RUN.match(remaining).end()
            if end:
                chunk = self.r.take(end)
                validator.decode(chunk, final=False)
                write(chunk)
            if end == len(remaining):
                continue
            special = self.r.byte()
            validator.decode(b'', final=True)
            validator.reset()
            write(bytes([special]))
            if special == 34:
                break
            if special < 32:
                raise ValueError('unescaped control character in JSON string')
            escaped = self.r.byte()
            write(bytes([escaped]))
            if escaped == 117:
                for _ in range(4):
                    char = self.r.byte()
                    if char not in b'0123456789abcdefABCDEF':
                        raise ValueError('invalid Unicode escape')
                    write(bytes([char]))
            elif escaped not in b'"\\/bfnrt':
                raise ValueError('invalid JSON escape')
        return json.loads(raw.getvalue()) if raw is not None else None

    def value(self, materialize=True, depth=0):
        if depth > 128:
            raise ValueError('JSON nesting exceeds supported depth')
        self.r.whitespace()
        token = self.r.peek()
        if token == 34:
            return self.string(materialize=materialize)
        if token in {123, 91}:
            is_object = token == 123
            result = {} if is_object else []
            keys = set()
            self.r.byte()
            self.r.whitespace()
            close = 125 if is_object else 93
            if self.r.peek() == close:
                self.r.byte()
                return result if materialize else None
            while True:
                key = None
                if is_object:
                    if self.r.peek() != 34:
                        raise ValueError('object key must be a string')
                    key = self.string(materialize=True)
                    if key in keys:
                        raise ValueError('duplicate JSON object key')
                    keys.add(key)
                    self.r.whitespace()
                    self.r.expect(58)
                value = self.value(materialize, depth + 1)
                if materialize:
                    if is_object:
                        result[key] = value
                    else:
                        result.append(value)
                self.r.whitespace()
                sep = self.r.byte()
                if sep == close:
                    break
                if sep != 44:
                    raise ValueError('missing JSON separator')
                self.r.whitespace()
            return result if materialize else None
        raw = bytearray()
        while self.r.peek() is not None and self.r.peek() not in b' \t\r\n,]}':
            raw.extend(self.r.take())
            if len(raw) > 4096:
                raise ValueError('oversized JSON numeric/literal token')
        token = bytes(raw)
        literals = {b'true': True, b'false': False, b'null': None}
        if token in literals:
            return literals[token]
        if not NUMBER.fullmatch(token):
            raise ValueError('invalid JSON scalar')
        value = json.loads(token)
        if isinstance(value, float) and not math.isfinite(value):
            raise ValueError('nonfinite JSON number')
        return value if materialize else None

    def array(self, consume):
        self.r.expect(91)
        self.r.whitespace()
        count = 0
        if self.r.peek() == 93:
            self.r.byte()
            return count
        while True:
            consume(self, count + 1)
            count += 1
            self.r.whitespace()
            sep = self.r.byte()
            if sep == 93:
                return count
            if sep != 44:
                raise ValueError('missing array separator')
            self.r.whitespace()

    def finish(self):
        self.r.whitespace()
        if self.r.peek() is not None:
            raise ValueError('trailing JSON content')


def classify(row, role='observations'):
    """Structural taxonomy only: does not certify full receipt reconstruction."""
    if not isinstance(row, dict):
        return 'unknown', None, 'public record is not an object'
    receipt = row.get('receipt')
    module = receipt.get('module') if isinstance(receipt, dict) else row.get('module')
    outer = row.get('module')
    if outer is not None and module is not None and outer != module:
        return 'unknown', None, 'conflicting public module identities'
    function = (receipt.get('function', receipt.get('method')) if isinstance(receipt, dict)
                else row.get('function', row.get('method')))
    qualified = isinstance(module, str) and module.startswith('pm4py.') and isinstance(function, str) and bool(function)
    if any(key in row for key in ['result', 'error', 'value_result', 'result_value']):
        if qualified:
            return 'actual_outcome', module, None
        return 'unknown', module, 'terminal outcome lacks qualified call identity'
    if any(key in row for key in ['output', 'return', 'returned', 'exception',
                                  'input_after', 'args_after', 'kwargs_after',
                                  'controls_after', 'apply_returned']):
        return 'unknown', module, 'possible terminal/entry evidence requires schema adapter'
    if role == 'intents' or row.get('entered') is True:
        if row.get('receipt_sha256') or qualified:
            return 'entered_without_outcome', module, 'entered outcome requires offline resolution'
        return 'unknown', module, 'entry evidence lacks receipt binding'
    if role == 'plans':
        if row.get('receipt_sha256') or qualified:
            return 'planned_only', module, None
        return 'unknown', module, 'plan lacks qualified identity or receipt binding'
    if (role == 'metadata' and isinstance(row.get('schema'), str) and row['schema'].startswith('minera-')) or row.get('kind') == 'metadata' or (isinstance(row.get('schema'), str) and
            row['schema'].startswith('minera-') and any(key in row for key in ['public_surface', 'signatures', 'parameters', 'availability'])):
        return 'metadata_only', module, None
    return 'unknown', module, 'unrecognized public record schema'


def decoded_string_chunks(raw, chunk_size):
    """Incrementally JSON-unescape ONLY an explicitly admitted payload token."""
    parser = Parser(raw, chunk_size)
    parser.r.expect(34)
    decoder = codecs.getincrementaldecoder('utf-8')('strict')
    pending_surrogate = None
    def character(value):
        nonlocal pending_surrogate
        code = ord(value)
        if pending_surrogate is not None:
            if not 0xdc00 <= code <= 0xdfff:
                raise ValueError('unpaired payload surrogate')
            code = 0x10000 + ((pending_surrogate - 0xd800) << 10) + code - 0xdc00
            pending_surrogate = None
            return chr(code).encode('utf-8')
        if 0xd800 <= code <= 0xdbff:
            pending_surrogate = code
            return b''
        if 0xdc00 <= code <= 0xdfff:
            raise ValueError('unpaired payload surrogate')
        return value.encode('utf-8')
    while True:
        if not parser.r.fill():
            raise ValueError('truncated spooled payload string')
        remaining = parser.r.buffer[parser.r.pos:]
        match = SPECIAL.search(remaining)
        count = match.start() if match else len(remaining)
        if count:
            text = decoder.decode(parser.r.take(count), final=False)
            if text:
                if pending_surrogate is not None:
                    raise ValueError('unpaired payload surrogate')
                yield text.encode('utf-8')
        if not match:
            continue
        special = parser.r.byte()
        tail = decoder.decode(b'', final=True)
        if tail:
            yield tail.encode('utf-8')
        decoder.reset()
        if special == 34:
            if pending_surrogate is not None:
                raise ValueError('unpaired payload surrogate')
            break
        if special != 92:
            raise ValueError('invalid spooled payload control character')
        escape = parser.r.byte()
        if escape == 117:
            value = chr(int(bytes(parser.r.byte() for _ in range(4)), 16))
        else:
            value = {34: '"', 92: '\\', 47: '/', 98: '\b', 102: '\f', 110: '\n', 114: '\r', 116: '\t'}[escape]
        encoded = character(value)
        if encoded:
            yield encoded
    parser.finish()


def decode_payload(raw, encoding, target, chunk_size):
    hasher = hashlib.sha256()
    size = 0
    pending = b''
    ended = False
    for chunk in decoded_string_chunks(raw, chunk_size):
        if encoding == 'base64':
            pending += chunk
            length = len(pending) // 4 * 4
            block, pending = pending[:length], pending[length:]
            if not block:
                continue
            if ended:
                raise ValueError('base64 data follows padding')
            try:
                chunk = base64.b64decode(block, validate=True)
            except binascii.Error as error:
                raise ValueError('invalid base64 payload') from error
            ended = b'=' in block
        hasher.update(chunk)
        size += len(chunk)
        target.write(chunk)
    if pending:
        raise ValueError('incomplete base64 quartet')
    return size, hasher.hexdigest()


class Inventory:
    def __init__(self, emit, chunk_size=65536, max_record_bytes=64 * 1024 * 1024):
        if chunk_size < 1:
            raise ValueError('chunk size must be positive')
        self.emit = emit
        self.chunk_size = chunk_size
        self.max_record_bytes = max_record_bytes
        self.counts = {}
        self.gaps = 0
        self.context = {}
        self.seen_members = {}
        self.seen_chunks = set()
        self.chunk_headers = {}

    def event(self, kind, **fields):
        self.counts[kind] = self.counts.get(kind, 0) + 1
        self.emit({'kind': kind, **self.context, **fields})

    def gap(self, reason, **fields):
        self.gaps += 1
        self.event('gap', reason=reason, **fields)

    def record(self, parser, ordinal, spec):
        parser.r.capture = hashlib.sha256()
        parser.r.captured = 0
        try:
            row = parser.value()
            raw_hash = parser.r.capture.hexdigest()
        finally:
            parser.r.capture = None
        classification, module, reason = classify(row, spec.get('role', 'observations'))
        self.event('record', ordinal=ordinal, classification=classification,
                   module=module, record_sha256=raw_hash, bytes=parser.r.captured,
                   receipt_sha256=row.get('receipt_sha256') if isinstance(row, dict) else None)
        if reason:
            self.gap(reason, ordinal=ordinal, module=module)

    def chunk_metadata(self, ordinal, spec, metadata, keys, payload_types):
        """Declared chunk metadata only: neither payload verification nor assembly."""
        schema = spec['envelope_schema']
        offset_form = schema == 'offset-full-member-base64-chunks-v1'
        expected = ({'member', 'offset', 'bytes', 'sha256', 'data'} if offset_form else
                    {'path', 'chunk', 'chunks', 'bytes', 'sha256', 'file_sha256', 'encoding', 'data'})
        name = metadata.get('path')
        reasons = []
        if keys != expected:
            reasons.append('chunk envelope fields do not match explicit producer schema')
        if payload_types != {'data': 'string'}:
            reasons.append('chunk payload must remain an opaque base64 string')
        disposition = 'unknown'
        if isinstance(name, str):
            try:
                safe_path(name)
                disposition = 'safe-relative-path'
            except ValueError as error:
                disposition = 'excluded-path' if 'excluded' in str(error) else 'unsafe-path'
                if disposition == 'unsafe-path':
                    reasons.append('unsafe chunk member path')
        else:
            reasons.append('chunk member path absent')
        size = metadata.get('bytes')
        if type(size) is not int or size < 0:
            reasons.append('chunk/full byte declaration must be a nonnegative integer')
        if not re.fullmatch('[0-9a-f]{64}', str(metadata.get('sha256', ''))):
            reasons.append('chunk/full SHA-256 declaration absent')
        declared = {}
        if offset_form:
            offset = metadata.get('offset')
            identity = offset
            if type(offset) is not int or offset < 0 or offset % CHUNK_SIZE != 0:
                reasons.append('offset must be a nonnegative producer chunk boundary')
            elif type(size) is int and (offset > size or (size > 0 and offset == size)):
                reasons.append('offset outside declared full member bytes')
            declared = {'offset': offset, 'declared_full_bytes': size,
                        'declared_full_sha256': metadata.get('sha256')}
            header = (size, metadata.get('sha256'))
        else:
            index, count = metadata.get('chunk'), metadata.get('chunks')
            identity = index
            if type(index) is not int or type(count) is not int or count < 1 or index < 0 or index >= count:
                reasons.append('chunk index/count must be exact valid integers')
            if metadata.get('encoding') != 'base64':
                reasons.append('indexed chunk encoding must be base64')
            if not re.fullmatch('[0-9a-f]{64}', str(metadata.get('file_sha256', ''))):
                reasons.append('indexed full-member SHA-256 declaration absent')
            if type(size) is int and (size > CHUNK_SIZE or size == 0 and (index != 0 or count != 1)):
                reasons.append('indexed chunk byte count outside producer domain')
            if type(index) is int and type(count) is int and index < count - 1 and size != CHUNK_SIZE:
                reasons.append('nonfinal indexed chunk must declare exactly 8 MiB')
            declared = {'chunk': index, 'chunks': count, 'declared_chunk_bytes': size,
                        'declared_chunk_sha256': metadata.get('sha256'),
                        'declared_full_sha256': metadata.get('file_sha256')}
            header = (count, metadata.get('file_sha256'))
        series = spec.get('series_identity')
        scope = canonical(series).decode() if series is not None else self.context.get('source')
        if isinstance(name, str) and type(identity) is int:
            key = (schema, scope, name, identity)
            if key in self.seen_chunks:
                reasons.append('duplicate chunk identity within declared series/container')
            self.seen_chunks.add(key)
            member_key = (schema, scope, name)
            if member_key in self.chunk_headers and self.chunk_headers[member_key] != header:
                reasons.append('inconsistent whole-member chunk metadata within series/container')
            self.chunk_headers[member_key] = header
        self.event('chunk', ordinal=ordinal, path=name, envelope_schema=schema,
                   envelope_keys=sorted(keys), payload_fields=payload_types,
                   path_disposition=disposition, series_identity=series,
                   identity_scope='declared-series' if series is not None else 'container-only',
                   metadata_valid=not reasons, payload_hash_verified=False,
                   whole_member_complete=False, **declared)
        for reason in reasons:
            self.gap(reason, ordinal=ordinal, member=name)

    def envelope(self, parser, ordinal, spec, discovery=False):
        if parser.r.peek() != 123:
            parser.value(False)
            self.gap('archive envelope member is not an object', ordinal=ordinal)
            return
        parser.r.byte()
        metadata = {}
        keys = set()
        payloads = {}
        payload_types = {}
        legacy = spec.get('envelope_schema') == 'legacy-name-data-base64-v1'
        chunk_schema = spec.get('envelope_schema') in CHUNK_SCHEMAS
        payload_keys = PAYLOAD_KEYS | ({'data_base64'} if legacy else set())
        admitted = {member['path']: member for member in spec.get('members', [])}
        try:
            parser.r.whitespace()
            if parser.r.peek() != 125:
                while True:
                    key = parser.string(materialize=True)
                    if key in keys:
                        raise ValueError('duplicate envelope key')
                    keys.add(key)
                    parser.r.whitespace()
                    parser.r.expect(58)
                    parser.r.whitespace()
                    if key in payload_keys:
                        payload_types[key] = 'string' if parser.r.peek() == 34 else 'nested-json'
                        preserve = not discovery and ('path' not in metadata or metadata.get('path') in admitted)
                        if parser.r.peek() == 34:
                            raw = tempfile.SpooledTemporaryFile(max_size=65536) if preserve else None
                            if raw is not None:
                                payloads[key] = raw
                            parser.string(sink=raw)
                        else:
                            parser.value(False)
                    elif key in {'path', 'sha256', 'encoding', 'schema', 'kind'} or (legacy and key == 'name') or (chunk_schema and key in {'member', 'file_sha256'}):
                        metadata_key = 'path' if legacy and key == 'name' or chunk_schema and key == 'member' else key
                        if parser.r.peek() == 34:
                            metadata[metadata_key] = parser.string(materialize=True)
                        else:
                            parser.value(False)
                            metadata[metadata_key] = None
                    elif key == 'bytes' or (legacy and key == 'schema_version') or (chunk_schema and key in {'offset', 'chunk', 'chunks'}):
                        if parser.r.peek() in b'-0123456789' if parser.r.peek() is not None else False:
                            metadata[key] = parser.value()
                        else:
                            parser.value(False)
                            metadata[key] = None
                    else:
                        parser.value(False)
                    parser.r.whitespace()
                    sep = parser.r.byte()
                    if sep == 125:
                        break
                    if sep != 44:
                        raise ValueError('missing envelope separator')
                    parser.r.whitespace()
            else:
                parser.r.byte()
            name = metadata.get('path')
            if chunk_schema:
                self.chunk_metadata(ordinal, spec, metadata, keys, payload_types)
                return
            if legacy and name is None and not payload_types:
                valid_header = (metadata.get('kind') == spec.get('metadata_kind') == 'manifest'
                                and type(metadata.get('schema_version')) is int
                                and type(spec.get('metadata_schema_version')) is int
                                and metadata['schema_version'] == spec.get('metadata_schema_version') == 1
                                and not (keys & POSSIBLE_CALL_KEYS))
                if valid_header:
                    self.event('metadata_header', ordinal=ordinal, header_kind='manifest',
                               schema_version=1, envelope_schema=spec['envelope_schema'], envelope_keys=sorted(keys))
                else:
                    self.event('header_candidate', ordinal=ordinal, header_kind=metadata.get('kind'),
                               schema_version=metadata.get('schema_version'), envelope_keys=sorted(keys),
                               possible_call_fields=sorted(keys & POSSIBLE_CALL_KEYS))
                    self.gap('unknown legacy header kind/version or possible entry evidence', ordinal=ordinal)
                return
            if name is None and not payload_types and isinstance(metadata.get('schema'), str):
                if keys & POSSIBLE_CALL_KEYS:
                    self.event('header_candidate', ordinal=ordinal, schema=metadata['schema'],
                               possible_call_fields=sorted(keys & POSSIBLE_CALL_KEYS), envelope_keys=sorted(keys))
                    self.gap('header has possible call evidence; explicit public-record admission required', ordinal=ordinal)
                    return
                if metadata['schema'] in spec.get('metadata_schemas', []):
                    self.event('metadata_header', ordinal=ordinal, **{k: v for k, v in metadata.items() if k != 'kind'}, header_kind=metadata.get('kind'), envelope_keys=sorted(keys))
                    return
                self.event('header_candidate', ordinal=ordinal, schema=metadata['schema'], possible_call_fields=[], envelope_keys=sorted(keys))
                self.gap('unknown no-path header schema requires explicit metadata admission', ordinal=ordinal)
                return
            disposition = 'unknown'
            if isinstance(name, str):
                try:
                    safe_path(name)
                    disposition = 'safe-relative-path'
                except ValueError as error:
                    disposition = 'excluded-path' if 'excluded' in str(error) else 'unsafe-path'
                    if disposition == 'unsafe-path':
                        self.gap('unsafe archive member path', ordinal=ordinal, member=name)
            self.event('member', ordinal=ordinal, **{k: v for k, v in metadata.items() if k != 'kind'}, envelope_kind=metadata.get('kind'), payload_fields=payload_types, path_disposition=disposition,
                       admitted=not discovery and name in admitted, envelope_keys=sorted(keys),
                       envelope_schema=spec.get('envelope_schema'), original_size_unavailable='bytes' not in metadata)
            if not isinstance(name, str):
                self.gap('envelope path absent', ordinal=ordinal)
                return
            seen = self.seen_members.setdefault(self.context.get('source'), set())
            if name in seen:
                self.gap('duplicate archive member path', ordinal=ordinal, member=name)
                return
            seen.add(name)
            if keys & POSSIBLE_CALL_KEYS:
                self.gap('envelope member has unexpected call/entry evidence', ordinal=ordinal,
                         member=name, possible_call_fields=sorted(keys & POSSIBLE_CALL_KEYS))
                return
            if legacy and keys != {'kind', 'name', 'sha256', 'data_base64'}:
                self.gap('legacy envelope fields do not match exact producer schema', ordinal=ordinal, member=name)
                return
            if not legacy and keys & {'chunk', 'chunks', 'file_sha256', 'offset', 'member'}:
                self.gap('chunk envelope requires an explicit metadata-only chunk schema', ordinal=ordinal, member=name)
                return
            if legacy and metadata.get('kind') != 'member':
                self.gap('legacy envelope requires kind=member', ordinal=ordinal, member=name)
                return
            if legacy and (set(payload_types) != {'data_base64'} or 'encoding' in metadata or 'bytes' in metadata or 'path' in keys):
                self.gap('legacy envelope field map is ambiguous or unsupported', ordinal=ordinal, member=name)
                return
            if discovery or name not in admitted:
                return  # payload tokens are never unescaped or base64-decoded
            member = admitted[name]
            require_declared_binding(member)
            safe_path(name, public_json=True)
            if member.get('provenance') != 'own-public-observation':
                raise ValueError('unknown member provenance')
            if member.get('layout') != 'records' or member.get('format') not in {'json', 'jsonl'}:
                raise ValueError('nested or unknown member encoding/layout')
            if legacy:
                if not re.fullmatch('[0-9a-f]{64}', str(metadata.get('sha256', ''))):
                    raise ValueError('original legacy envelope SHA-256 absent')
                payload_key, encoding = 'data_base64', 'base64'
            elif set(payload_types) == {'utf8'} and 'encoding' not in metadata:
                payload_key, encoding = 'utf8', 'utf8'
            elif set(payload_types) == {'base64'} and 'encoding' not in metadata:
                payload_key, encoding = 'base64', 'base64'
            elif set(payload_types) == {'data'} and metadata.get('encoding') == 'base64':
                payload_key, encoding = 'data', 'base64'
            else:
                raise ValueError('unknown or ambiguous payload encoding')
            if payload_types[payload_key] != 'string' or payload_key not in payloads:
                raise ValueError('nested payload JSON requires separate admission')
            raw = payloads[payload_key]
            raw.seek(0)
            with tempfile.TemporaryFile() as decoded:
                binding = decode_payload(raw, encoding, decoded, self.chunk_size)
                if legacy:
                    if not re.fullmatch('[0-9a-f]{64}', str(metadata.get('sha256', ''))) or binding[1] != metadata['sha256']:
                        raise ValueError('original legacy envelope SHA-256 mismatch')
                else:
                    require_binding(binding, metadata)
                require_binding(binding, member)
                self.event('member_binding', member=name, bytes=binding[0], sha256=binding[1],
                           original_size_unavailable=legacy, size_source='reviewed-allowlist' if legacy else 'envelope-and-reviewed-allowlist')
                decoded.seek(0)
                previous = self.context
                self.context = {**previous, 'member': name}
                try:
                    self.parse(decoded, member)
                finally:
                    self.context = previous
        finally:
            for raw in payloads.values():
                raw.close()

    def top_object(self, parser, spec, discovery=False):
        parser.r.expect(123)
        keys = set()
        schema = None
        candidates = {}
        selected = spec.get('members_key') if spec.get('layout') == 'envelope' else spec.get('records_key')
        found = False
        parser.r.whitespace()
        if parser.r.peek() != 125:
            while True:
                key = parser.string(materialize=True)
                if key in keys:
                    raise ValueError('duplicate top-level object key')
                keys.add(key)
                parser.r.whitespace()
                parser.r.expect(58)
                parser.r.whitespace()
                if discovery and key == 'members' and parser.r.peek() == 91:
                    candidates[key] = parser.array(lambda p, n: self.envelope(p, n, spec, True))
                elif key == selected:
                    found = True
                    if parser.r.peek() != 91:
                        raise ValueError('declared wrapper key must be an array')
                    consume = self.envelope if spec.get('layout') == 'envelope' else self.record
                    candidates[key] = parser.array(lambda p, n: consume(p, n, spec))
                elif key in PAYLOAD_KEYS:
                    # Unknown root objects never expose implementation payloads.
                    parser.value(False)
                    candidates[key] = 'opaque-payload'
                elif key == 'schema':
                    if parser.r.peek() == 34:
                        schema = parser.string(materialize=True)
                    else:
                        parser.value(False)
                elif parser.r.peek() == 91:
                    candidates[key] = parser.array(lambda p, n: p.value(False))
                else:
                    parser.value(False)
                parser.r.whitespace()
                sep = parser.r.byte()
                if sep == 125:
                    break
                if sep != 44:
                    raise ValueError('missing top-level separator')
                parser.r.whitespace()
        else:
            parser.r.byte()
        self.event('container_structure', schema=schema, top_level_keys=sorted(keys), array_candidates=candidates)
        if discovery and keys & POSSIBLE_CALL_KEYS:
            self.gap('possible call-containing root object requires explicit record admission', possible_call_fields=sorted(keys & POSSIBLE_CALL_KEYS))
        if not discovery and selected is not None and not found:
            self.gap('declared wrapper key absent')
        if discovery and 'members' not in candidates and not any(isinstance(v, int) for v in candidates.values()):
            self.gap('unknown top-level object structure; no automatic admission')

    def parse(self, stream, spec):
        parser = Parser(stream, self.chunk_size, self.max_record_bytes)
        if parser.r.peek() == 0xef:
            parser.r.expect(0xef)
            parser.r.expect(0xbb)
            parser.r.expect(0xbf)
        layout = spec.get('layout')
        if layout not in {'discover', 'records', 'envelope'}:
            raise ValueError('unknown container layout')
        discovery = layout == 'discover'
        fmt = spec.get('format', '').removeprefix('gzip-')
        if fmt not in {'json', 'jsonl'}:
            raise ValueError('unknown JSON transport format')
        if fmt == 'jsonl':
            ordinal = 0
            while True:
                parser.r.whitespace()
                if parser.r.peek() is None:
                    break
                line = parser.r.line
                ordinal += 1
                if layout == 'records':
                    self.record(parser, ordinal, spec)
                else:
                    self.envelope(parser, ordinal, spec, discovery)
                if parser.r.line != line:
                    raise ValueError('JSONL record crosses a physical line')
                parser.r.whitespace(horizontal=True)
                if parser.r.peek() == 13:
                    parser.r.byte()
                if parser.r.peek() == 10:
                    parser.r.byte()
                elif parser.r.peek() is not None:
                    raise ValueError('JSONL values must be separated by a newline')
            return
        parser.r.whitespace()
        token = parser.r.peek()
        if token == 91:
            if layout == 'records':
                consume = lambda p, n: self.record(p, n, spec)
            else:
                consume = lambda p, n: self.envelope(p, n, spec, discovery)
            count = parser.array(consume)
            self.event('container_structure', root_kind='array', entries=count)
        elif token == 123 and (discovery or spec.get('records_key') or spec.get('members_key')):
            self.top_object(parser, spec, discovery)
        elif token == 123 and layout == 'records':
            self.record(parser, 1, spec)
        elif token == 123 and layout == 'envelope':
            self.envelope(parser, 1, spec)
        else:
            parser.value(False)
            self.gap('unknown top-level JSON structure')
        parser.finish()

    def source(self, root, spec, cache_dir=None):
        self.context = {'source': spec.get('path')}
        self.seen_members[spec.get('path')] = set()
        if spec.get('provenance') not in ({'own-container', 'own-public-observation'} if spec.get('layout') == 'discover' else {'own-public-observation'}):
            raise ValueError('unknown source provenance')
        path = resolve_source(root, spec['path'])
        require_binding(hash_file(path), spec)
        config = {'parser_sha256': PARSER_SHA256, 'max_record_bytes': self.max_record_bytes,
                  'spec': {key: value for key, value in spec.items() if key != 'path'}}
        cache_key = digest(canonical(config))
        cache_path = cache_dir / (cache_key + '.jsonl') if cache_dir else None
        if cache_path and self.replay_cache(cache_path, cache_key, spec):
            return
        previous_emit = self.emit
        with contextlib.ExitStack() as stack:
            temporary = None
            cache_stream = None
            hasher = hashlib.sha256()
            total = count = 0
            if cache_path:
                cache_dir.mkdir(parents=True, exist_ok=True)
                fd, name = tempfile.mkstemp(prefix='inventory-', suffix='.tmp', dir=cache_dir)
                os.close(fd)
                temporary = Path(name)
                cache_stream = stack.enter_context(temporary.open('wb'))
                def combined_emit(row):
                    nonlocal total, count
                    previous_emit(row)
                    data = canonical({key: value for key, value in row.items() if key != 'source'}) + b'\n'
                    cache_stream.write(data)
                    hasher.update(data)
                    total += len(data)
                    count += 1
                self.emit = combined_emit
            try:
                with path.open('rb') as raw:
                    stream = stack.enter_context(gzip.GzipFile(fileobj=raw)) if spec.get('format', '').startswith('gzip-') else raw
                    self.parse(stream, spec)
                missing = {member['path'] for member in spec.get('members', [])} - self.seen_members[spec['path']]
                if missing:
                    self.gap('allowlisted archive members absent', members=sorted(missing))
                require_binding(hash_file(resolve_source(root, spec['path'])), spec)
                if cache_stream:
                    footer = {'cache_footer': {'key': cache_key, 'parser_sha256': PARSER_SHA256,
                              'events_bytes': total, 'events_sha256': hasher.hexdigest(), 'events_count': count}}
                    cache_stream.write(canonical(footer) + b'\n')
                    cache_stream.flush()
            except BaseException:
                if cache_stream:
                    cache_stream.close()
                    temporary.unlink(missing_ok=True)
                raise
            finally:
                self.emit = previous_emit
        if temporary is not None:
            temporary.replace(cache_path)

    def replay_cache(self, path, key, spec):
        if not path.exists():
            return False
        # Verify the complete metadata receipt before emitting any cached event.
        # Payload strings are never present in these cache files.
        hasher = hashlib.sha256()
        total = count = 0
        footer = None
        try:
            with path.open('rb') as stream:
                for raw in stream:
                    parser = Parser(io.BytesIO(raw), self.chunk_size, self.max_record_bytes)
                    event = parser.value()
                    parser.finish()
                    if not isinstance(event, dict):
                        return False
                    if 'cache_footer' in event:
                        if footer is not None:
                            return False
                        footer = event['cache_footer']
                    else:
                        if footer is not None or event.get('kind') not in {'member', 'member_binding', 'chunk', 'record', 'gap', 'container_structure', 'metadata_header', 'header_candidate'}:
                            return False
                        hasher.update(raw)
                        total += len(raw)
                        count += 1
            if footer != {'key': key, 'parser_sha256': PARSER_SHA256,
                          'events_bytes': total, 'events_sha256': hasher.hexdigest(), 'events_count': count}:
                return False
        except (ValueError, TypeError, KeyError, OSError, UnicodeError):
            return False
        with path.open('rb') as stream:
            for raw in stream:
                event = json.loads(raw)
                if 'cache_footer' in event:
                    break
                kind = event.pop('kind')
                if kind == 'gap':
                    self.gaps += 1
                if kind == 'chunk' and event.get('metadata_valid'):
                    schema = event['envelope_schema']
                    series = event.get('series_identity')
                    scope = canonical(series).decode() if series is not None else self.context.get('source')
                    offset_form = schema == 'offset-full-member-base64-chunks-v1'
                    identity = event['offset'] if offset_form else event['chunk']
                    chunk_key = (schema, scope, event['path'], identity)
                    if chunk_key in self.seen_chunks:
                        self.gap('duplicate cached chunk identity within declared series/container', member=event['path'])
                    self.seen_chunks.add(chunk_key)
                    member_key = (schema, scope, event['path'])
                    header = ((event['declared_full_bytes'], event['declared_full_sha256']) if offset_form else
                              (event['chunks'], event['declared_full_sha256']))
                    if member_key in self.chunk_headers and self.chunk_headers[member_key] != header:
                        self.gap('inconsistent cached whole-member chunk metadata', member=event['path'])
                    self.chunk_headers[member_key] = header
                self.event(kind, **event)
        self.event('cache_hit', cache_key=key)
        return True


def run(manifest, emit, chunk_size=65536, max_record_bytes=64 * 1024 * 1024, cache_dir=None):
    if manifest.get('schema') != MANIFEST or not isinstance(manifest.get('scope'), str) or not manifest['scope'].strip():
        raise ValueError('unknown manifest schema or absent bounded scope')
    root = Path(manifest['root']).resolve()
    if any(part.casefold() in EXCLUDED for part in root.parts):
        raise ValueError('excluded inventory root')
    inventory = Inventory(emit, chunk_size, max_record_bytes)
    paths = [source['path'] for source in manifest['sources']]
    if len(set(paths)) != len(paths):
        raise ValueError('duplicate source path')
    for spec in manifest['sources']:
        try:
            members = spec.get('members', [])
            if spec.get('envelope_schema') not in {None, 'legacy-name-data-base64-v1'} | CHUNK_SCHEMAS:
                raise ValueError('unknown explicit envelope schema')
            if spec.get('envelope_schema') in CHUNK_SCHEMAS and (spec.get('layout') != 'discover' or members):
                raise ValueError('chunk schema is metadata discovery only; explicit reassembly is unavailable')
            names = [safe_path(member['path'], public_json=True) for member in members]
            for member in members:
                require_declared_binding(member)
            if len(set(names)) != len(names):
                raise ValueError('duplicate member allowlist path')
            inventory.source(root, spec, Path(cache_dir) if cache_dir else None)
        except (ValueError, TypeError, KeyError, OSError, EOFError, UnicodeError) as error:
            inventory.context = {'source': spec.get('path')}
            inventory.gap(str(error))
    for spec in manifest['sources']:
        try:
            require_binding(hash_file(resolve_source(root, spec['path'])), spec)
        except (ValueError, TypeError, KeyError, OSError) as error:
            inventory.context = {'source': spec.get('path')}
            inventory.gap('final source revalidation: ' + str(error))
    if digest(Path(__file__).read_bytes()) != PARSER_SHA256:
        inventory.context = {}
        inventory.gap('inventory parser changed during generation')
    return {'schema': VERSION, 'scope': manifest['scope'], 'parser_sha256': PARSER_SHA256,
            'manifest_sha256': digest(canonical(manifest)), 'counts': inventory.counts,
            'gaps': inventory.gaps, 'explicit_inventory_parsed': inventory.gaps == 0,
            'admission_complete': False, 'catalogue_audit_complete': False,
            'reference_calls': 0, 'reference_implementation_inspected': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('report', type=Path)
    parser.add_argument('--events', type=Path, required=True)
    parser.add_argument('--cache', type=Path)
    parser.add_argument('--chunk-size', type=int, default=65536)
    parser.add_argument('--max-record-bytes', type=int, default=64 * 1024 * 1024)
    args = parser.parse_args()
    # Manifest values are small, explicit review inputs rather than archives.
    with args.manifest.open('rb') as stream:
        small = Parser(stream, args.chunk_size, args.max_record_bytes)
        manifest = small.value()
        small.finish()
    with args.events.open('wb') as events:
        result = run(manifest, lambda row: events.write(canonical(row) + b'\n'), args.chunk_size, args.max_record_bytes, args.cache)
    result['events'] = {'path': str(args.events), 'bytes': args.events.stat().st_size, 'sha256': hash_file(args.events)[1]}
    result['cache_enabled'] = args.cache is not None
    temporary = args.report.with_suffix(args.report.suffix + '.tmp')
    temporary.write_bytes(canonical(result) + b'\n')
    temporary.replace(args.report)


if __name__ == '__main__':
    main()
