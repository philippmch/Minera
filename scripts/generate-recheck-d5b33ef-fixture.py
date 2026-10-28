#!/usr/bin/env python3
"""Build the case tables of src/backend/tests/test_recheck_d5b33ef.cpp.

Usage: generate-recheck-d5b33ef-fixture.py EVIDENCE OUT_DIR
           [--filters CASES RESULTS]... [--org CASES RESULTS]... [--pnml RESULTS]

EVIDENCE is the reviewer's minera-recheck-d5b33ef directory (saved inputs and
public PM4Py 2.7.22 outputs of the recheck of d5b33ef, 2026-10-28). Extra case
sets and their results come from scripts/capture-recheck-d5b33ef-reference.py
(the reviewer's organizational cases are captured there too, over 40 hash
seeds, so that every result the reference can give is listed). Writes:

  filter-cases.tsv.gz  one case per line: the input in the test's token format
                       (with the case and activity column dtypes), a tab, the
                       expected status, a tab and the expected snapshot or error
  org-cases.tsv.gz     one case per line: the input, then one tab-separated
                       alternative per distinct reference result, "OK <value>"
                       or "ERR <message>" (plus "DECLARED set-layout" for the
                       declared set-order cases); values are canonical
                       compact JSON with set members sorted
  network-cli.tsv      the reviewer's six tied-sort XES logs with their
                       canonical reference edges
  pnml-numeric.tsv     the reviewer's PNML weight fixtures: file, expected
                       "OK <weight repr>", "ERR <message>" or "POLICY"
  pnml-properties.tsv.gz
                       generated PNML models with one stochastic property
                       text each (hex): key, text, model, expected
                       "OK <weight repr> <priority> <invisible>", "ERR <message>"
                       or "POLICY"

POLICY marks the declared native policy: activation weights must be finite
and non-negative and priorities fit a native int, where PM4Py keeps any float
and any Python int.
"""
import gzip
import json
import math
import shutil
import sys
from pathlib import Path


def dumps(value):
    return json.dumps(value, ensure_ascii=False, separators=(',', ':'))


def quoted(text):
    return '"' + text.replace('\\', '\\\\').replace('"', '\\"') + '"'


def write_gz(path, lines):
    with gzip.GzipFile(path, 'wb', mtime=0) as handle:
        handle.write(('\n'.join(lines) + '\n').encode())


# ---------------------------------------------------------------- filters

def filter_line(case, result):
    parts = [case['id'], case['op'], quoted(case['lo']), quoted(case['hi'])] + [quoted(k) for k in case['keys']]
    parts += [case.get('cdtype', 'object'), case.get('adtype', 'object'), str(len(case['rows']))]
    for row in case['rows']:
        parts += [quoted(x) for x in row]
    expected = dumps(result['result']) if result['status'] == 'OK' else result['error']
    return ' '.join(parts) + '\t' + result['status'] + '\t' + expected


# ---------------------------------------------------------------- organizational

# Declared labels: a None case id prints as "nan" and a None event id as
# "None" in native batches, where PM4Py keeps the None object.
def canonical_org(kind, value):
    if kind == 'network':
        return '[' + ','.join(sorted(dumps([s, t, a, n, [float(x) for x in perf]]) for s, t, a, n, perf in value)) + ']'
    groups = []
    for activity, resource, count, batches in value:
        rendered = []
        for kind_index, start, end, events in batches:
            items = sorted(dumps([float(e[0]), float(e[1]), 'nan' if e[2] is None else e[2],
                                  'None' if e[3] is None else e[3]]) for e in events)
            rendered.append('[' + ','.join([dumps(kind_index), dumps(float(start)), dumps(float(end)),
                                            '[' + ','.join(items) + ']']) + ']')
        rendered.sort()
        groups.append('[' + ','.join([dumps(activity), dumps(resource), dumps(count), '[' + ','.join(rendered) + ']']) + ']')
    return '[' + ','.join(groups) + ']'


def alternative(kind, record):
    return 'OK ' + canonical_org(kind, record['value']) if record['ok'] else 'ERR ' + record['error']


# Declared set-order cases: the reference sorts a batch's event set in
# CPython's hash-table order, which native does not model. Where the tuples
# that decide the first failed comparison hold no strings or NaN, that order
# is the same under every hash seed, so 40 seeds list one pair while native
# names another pair of the same inputs (both raise TypeError).
SET_LAYOUT = {'batch4_439_object_object', 'batch4_977_object_object'}


def org_line(case, records):
    head = [case['kind'], quoted(case['id']), case.get('mode', 'default'), str(case.get('minimum', 1)),
            str(case.get('distance', 0)), '1' if case.get('event_id') else '0', case.get('case_dtype', 'object'),
            case.get('id_dtype', 'object'), str(len(case['rows']))] + [quoted(x) for row in case['rows'] for x in row]
    seen = []
    for record in records:
        for item in [record] + record.get('alternatives', []):
            text = alternative(case['kind'], item)
            if text not in seen: seen.append(text)
    if case['id'] in SET_LAYOUT:
        assert all(t.startswith("ERR '<' not supported") for t in seen), case['id']
        seen.append('DECLARED set-layout')
    return ' '.join(head) + '\t' + '\t'.join(seen)


def read_jsonl(path):
    return [json.loads(line) for line in Path(path).read_text().splitlines() if line.strip()]


# ---------------------------------------------------------------- PNML

def property_line(record, model):
    key, text = record['key'], record['text']
    if record['ok']:
        weight, priority = float(record['weight']), int(record['priority'])
        if not math.isfinite(weight) or weight < 0 or not -2 ** 31 <= priority < 2 ** 31:
            expected = 'POLICY'
        else:
            expected = f"OK {record['weight']} {record['priority']} {1 if record['invisible'] else 0}"
    else:
        expected = 'ERR ' + record['error']
    encoded = '-' if text is None else text.encode().hex()
    return '\t'.join([key, encoded, model.encode().hex(), expected])


def main():
    evidence, out_dir = Path(sys.argv[1]), Path(sys.argv[2])
    extra = {'--filters': [], '--org': [], '--pnml': []}
    args = sys.argv[3:]
    while args:
        flag = args.pop(0)
        count = 1 if flag == '--pnml' else 2
        extra[flag].append(args[:count])
        args = args[count:]
    out_dir.mkdir(parents=True, exist_ok=True)

    # N3 and the sort-boundary controls: the reviewer's 182 filter cases
    # (d5-precheck is a subset), then the captured run-key fuzz.
    root = evidence / 'filtering-statistics'
    filter_lines = []
    results = {r['id']: r for r in read_jsonl(root / 'd5-neighbors-oracle-results.jsonl')}
    filter_lines += [filter_line(c, results[c['id']]) for c in json.loads((root / 'd5-neighbors-cases.json').read_text())]
    for cases_path, results_path in extra['--filters']:
        results = {r['id']: r for r in read_jsonl(results_path)}
        filter_lines += [filter_line(c, results[c['id']]) for c in json.loads(Path(cases_path).read_text())]
    write_gz(out_dir / 'filter-cases.tsv.gz', filter_lines)

    # N2 and N4: the reviewer's wider and NaN-order cases with their saved
    # results, joined by every result the 40-seed capture observed; the
    # shared-NaN declaration needs one NaN object in several rows, which
    # the token format cannot express.
    root = evidence / 'ocel-organizational'
    saved = {}
    for name in ['wider-oracle.jsonl', 'nan-case-order-oracle.jsonl']:
        saved.update({r['id']: r for r in read_jsonl(root / name)})
    captured = {}
    for _, results_path in extra['--org']:
        captured.update({r['id']: r for r in read_jsonl(results_path)})
    org_lines = []
    for name in ['wider-cases.json', 'nan-case-order-cases.json']:
        for case in json.loads((root / name).read_text()):
            if case.get('shared_nan'): continue
            if case['kind'] == 'batches':
                case.setdefault('case_dtype', {'case-str': 'str', 'case-float64': 'float64'}.get(case.get('mode'), 'object'))
            records = [saved[case['id']]] + ([captured[case['id']]] if case['id'] in captured else [])
            org_lines.append(org_line(case, records))
    reviewer_ids = set(saved)
    for cases_path, _ in extra['--org']:
        for case in json.loads(Path(cases_path).read_text()):
            if case['id'] in reviewer_ids: continue
            org_lines.append(org_line(case, [captured[case['id']]]))
    write_gz(out_dir / 'org-cases.tsv.gz', org_lines)

    # N2 through the CLI path: the reviewer's six XES logs.
    cli_lines = []
    for record in json.loads((root / 'wider-sort-cli-comparisons.json').read_text()):
        name = 'wider-network-sort-' + record['id'].split('/', 1)[1].replace('/', '-') + '.xes'
        shutil.copyfile(root / name, out_dir / name)
        cli_lines.append(name + '\t' + alternative('network', record['reference']))
    (out_dir / 'network-cli.tsv').write_text('\n'.join(cli_lines) + '\n')

    # N5: the reviewer's PNML weight fixtures, then generated property cases.
    root = evidence / 'discovery-conformance'
    pnml_lines = []
    for record in json.loads((root / 'd5-pnml-numeric-comparisons.json').read_text()):
        name = record['model'].rsplit('/', 1)[1]
        shutil.copyfile(root / name, out_dir / name)
        reference = record['reference']
        if record['status'] == 'qualified_weight_validation':
            expected = 'POLICY'
        elif reference['ok']:
            expected = 'OK ' + repr(float(reference['weights'][0]))
        else:
            expected = 'ERR ' + reference['error']
        pnml_lines.append(name + '\t' + expected)
    (out_dir / 'pnml-numeric.tsv').write_text('\n'.join(pnml_lines) + '\n')

    sys.path.insert(0, str(Path(__file__).parent))
    capture = __import__('capture-recheck-d5b33ef-reference')
    property_lines = []
    for (results_path,) in extra['--pnml']:
        for record in read_jsonl(results_path):
            property_lines.append(property_line(record, capture.pnml_model(record['key'], record['text'])))
    write_gz(out_dir / 'pnml-properties.tsv.gz', property_lines)
    print(len(filter_lines), 'filter cases,', len(org_lines), 'organizational cases,', len(cli_lines),
          'network logs,', len(pnml_lines), 'PNML fixtures,', len(property_lines), 'PNML property cases')


if __name__ == '__main__':
    main()
