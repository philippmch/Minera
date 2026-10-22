"""Index explicitly admitted own public observations; never import the reference.

CLI: python -B scripts/retained_public_receipt_index.py MANIFEST DATABASE REPORT

The v1 manifest declares root, inventory_complete, scope (a human-readable bounded
inventory description), modules, and sources. Each source declares path, bytes,
sha256, provenance='own-public-observation', format ('json', 'jsonl',
'gzip-json', 'gzip-jsonl', 'gzip-envelope-json', 'gzip-envelope-jsonl'), adapter
('receipt-v1', 'pandas-before-v1', 'polars-receipt-v1', or 'digest-plan-v1'),
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
Currently the closed receipt adapters admit only the three versioned LTL
observer schemas listed in SCHEMAS. New observer families require an explicit
adapter/schema addition with operand-form and canonicalization validation.
"""

from __future__ import annotations

import argparse
import base64
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


def adapt(row, spec):
    if not isinstance(row, dict):
        raise ValueError('record must be an object')
    adapter = spec.get('adapter')
    if adapter not in {'receipt-v1', 'pandas-before-v1', 'polars-receipt-v1', 'digest-plan-v1'}:
        raise ValueError('unknown receipt adapter')
    if spec.get('role') not in {'observations', 'plans', 'intents'}:
        raise ValueError('unknown call provenance role')
    if adapter == 'digest-plan-v1':
        if spec.get('receipt_schema') not in {value[0] for value in SCHEMAS.values()}:
            raise ValueError('unknown original receipt schema')
    elif spec.get('receipt_schema') != SCHEMAS[adapter][0]:
        raise ValueError('unknown original receipt schema for adapter')
    if adapter == 'digest-plan-v1' and any(key in row for key in ['result', 'error', 'value_result', 'receipt']):
        raise ValueError('digest-only plan contains full receipt or outcome; explicit full adapter required')
    if adapter == 'digest-plan-v1' or (spec.get('role') == 'intents' and 'receipt' not in row):
        if adapter == 'digest-plan-v1' and spec.get('role') != 'plans':
            raise ValueError('digest-plan adapter requires planned-only provenance')
        digest = row.get('receipt_sha256')
        if not re.fullmatch('[0-9a-f]{64}', str(digest)):
            raise ValueError('invalid digest-only receipt binding')
        return (row.get('module'), row.get('function', row.get('method')), None,
                digest, 'digest_only_plan' if adapter == 'digest-plan-v1' else 'entered_without_outcome')
    if adapter in {'receipt-v1', 'polars-receipt-v1'}:
        receipt = row.get('receipt')
    elif adapter == 'pandas-before-v1':
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
                    self.gap(key, member, ordinal, row, str(error))
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
