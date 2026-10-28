#!/usr/bin/env python3
"""Build the case tables of src/backend/tests/test_recheck_c8ee22d.cpp.

Usage: generate-recheck-c8ee22d-fixture.py EVIDENCE OUT_DIR
           [--filters CASES RESULTS]... [--batches CASES RESULTS]...

EVIDENCE is the reviewer's minera-recheck-c8ee22d directory (saved inputs and
public PM4Py outputs of the recheck of c8ee22d, 2026-10-28). Extra case sets
come from scripts/capture-recheck-c8ee22d-reference.py. Writes
filter-cases.tsv.gz and org-cases.tsv.gz: one case per line, the input in the
test's token format, a tab, the expected status and a tab, the expected value
(compact JSON, list members sorted where the reference returns sets) or error.
"""
import gzip
import json
import sys
from pathlib import Path


def dumps(value):
    return json.dumps(value, ensure_ascii=False, separators=(',', ':'))


def quoted(text):
    return '"' + text.replace('\\', '\\\\').replace('"', '\\"') + '"'


# Python 3.13 spells a failed membership test "is not iterable", 3.14 (the
# reviewer's runtime) "is not a container or iterable".
def message(text):
    if text.startswith('argument of type') and text.endswith('is not iterable'):
        return text[:-len('is not iterable')] + 'is not a container or iterable'
    return text


def filter_line(case, result):
    parts = [case['id'], case['op'], quoted(case['lo']), quoted(case['hi'])] + [quoted(k) for k in case['keys']]
    parts.append(str(len(case['rows'])))
    for row in case['rows']:
        parts += [quoted(x) for x in row]
    if result['status'] == 'OK':
        expected = dumps(result['result'])
    else:
        expected = message(result['error'])
    return ' '.join(parts) + '\t' + result['status'] + '\t' + expected


def token(value, date=False):
    if isinstance(value, dict):
        kind = value['t']
        if kind == 'date': return 'D' + value['v']
        if kind == 'na': return 'A'
        if kind == 'float': return 'F' + value['v']
        if kind == 'int': return 'I' + value['v']
    if value is None: return 'X'
    if isinstance(value, bool): return 'B' + ('1' if value else '0')
    if isinstance(value, int): return 'I' + str(value)
    if isinstance(value, float): return 'F' + repr(value)
    return ('T' if date else 'S') + value


def canonical_org(kind, value):
    if kind == 'network':
        edges = sorted(dumps([s, t, a, n, [float(x) for x in perf]]) for s, t, a, n, perf in value)
        return '[' + ','.join(edges) + ']'
    groups = []
    for activity, resource, count, batches in value:
        rendered = []
        for kind_index, start, end, events in batches:
            items = sorted(dumps([float(e[0]), float(e[1]), e[2], e[3]]) for e in events)
            rendered.append('[' + ','.join([dumps(kind_index), dumps(float(start)), dumps(float(end)),
                                            '[' + ','.join(items) + ']']) + ']')
        rendered.sort()
        groups.append('[' + ','.join([dumps(activity), dumps(resource), dumps(count), '[' + ','.join(rendered) + ']']) + ']')
    return '[' + ','.join(groups) + ']'


def org_line(case, reference):
    if case['kind'] == 'network':
        head = ['network', quoted(case['id']), case['role'], quoted(case['key']), '1' if case.get('projection') else '0',
                str(len(case['values']))] + [quoted(token(v)) for v in case['values']]
    else:
        cells = [quoted(token(v, i in (3, 4))) for row in case['rows'] for i, v in enumerate(row)]
        head = ['batches', quoted(case['id']), str(case['minimum']), '1' if case['event_id'] else '0',
                case.get('id_dtype', 'object'), str(len(case['rows']))] + cells
    if reference['ok']:
        tail = ['OK', canonical_org(case['kind'], reference['value'])]
    else:
        tail = [reference.get('error_type', 'ERROR'), reference['error']]
    return ' '.join(head) + '\t' + '\t'.join(tail)


def main():
    evidence, out_dir = Path(sys.argv[1]), Path(sys.argv[2])
    extra_filters, extra_batches = [], []
    args = sys.argv[3:]
    while args:
        flag, cases, results, args = args[0], args[1], args[2], args[3:]
        (extra_filters if flag == '--filters' else extra_batches).append((cases, results))

    filter_lines = []
    for name in ['activity-neighbors', 'key-neighbors']:
        root = evidence / 'filtering-statistics'
        cases = json.loads((root / f'{name}-cases.json').read_text())
        saved = {c['case']: c['oracle'] for c in json.loads((root / f'{name}-comparisons.json').read_text())}
        filter_lines += [filter_line(c, saved[c['id']]) for c in cases]
    for cases_path, results_path in extra_filters:
        results = {r['id']: r for r in map(json.loads, Path(results_path).read_text().splitlines())}
        filter_lines += [filter_line(c, results[c['id']]) for c in json.loads(Path(cases_path).read_text())]

    org_lines = []
    for name in ['extended', 'boundary-extra']:
        root = evidence / 'ocel-organizational'
        cases = json.loads((root / f'{name}-cases.json').read_text())
        saved = {}
        for record in json.loads((root / f'{name}-comparison.json').read_text()):
            saved[record.get('id') or record['input']['id']] = record['reference']
        org_lines += [org_line(c, saved[c['id']]) for c in cases]
    for cases_path, results_path in extra_batches:
        results = {r['id']: r for r in map(json.loads, Path(results_path).read_text().splitlines())}
        org_lines += [org_line(c, results[c['id']]) for c in json.loads(Path(cases_path).read_text())]

    out_dir.mkdir(parents=True, exist_ok=True)
    for name, lines in [('filter-cases.tsv.gz', filter_lines), ('org-cases.tsv.gz', org_lines)]:
        with gzip.GzipFile(out_dir / name, 'wb', mtime=0) as handle:
            handle.write(('\n'.join(lines) + '\n').encode())
    print(len(filter_lines), 'filter cases,', len(org_lines), 'organizational cases')


if __name__ == '__main__':
    main()
