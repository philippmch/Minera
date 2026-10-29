#!/usr/bin/env python3
"""Build the case tables of src/backend/tests/test_recheck_7270a1f.cpp.

Usage: generate-recheck-7270a1f-fixture.py OUT_DIR
           [--filters CASES RESULTS]... [--pnml CASES RESULTS]...

Case sets and their public PM4Py 2.7.22 results come from
scripts/capture-recheck-7270a1f-reference.py. Writes:

  filter-cases.tsv.gz  filter cases (consecutive activity/case grouping, then
                       the typed-column cases) in the token format of
                       test_recheck_d5b33ef.cpp: the input, a tab, the
                       expected status, a tab and the expected snapshot or
                       error
  pnml-cases.tsv.gz    one PNML model per line: id, the model (hex), the
                       expected import without the stochastic map and the
                       expected import with it. An import is "ERR <message>",
                       "POLICY" or "OK <labels>" (plus " <records>" with the
                       map): labels are [transition id, label or null] sorted
                       by id; records are null without a map, else one
                       [id, distribution, timing, p1, p2, p3, weight,
                       priority] per transition sorted by id, numbers as
                       Python repr text

POLICY marks the declared native policy: a native stochastic map holds finite
numbers, finite non-negative activation weights and int priorities, where
PM4Py keeps a parameter text it does not convert (DETERMINISTIC, NORMAL,
UNIFORM), an infinite or NaN value, an 'UNDEFINED' exponential rate, any
float weight and any int priority.
"""
import gzip
import json
import math
import sys
from pathlib import Path


def dumps(value):
    return json.dumps(value, ensure_ascii=False, separators=(',', ':'))


def write_gz(path, lines):
    with gzip.GzipFile(path, 'wb', mtime=0) as handle:
        handle.write(('\n'.join(lines) + '\n').encode())


def read_jsonl(path):
    return [json.loads(line) for line in Path(path).read_text().splitlines() if line.strip()]


# ---------------------------------------------------------------- PNML

NAMES = {'IMMEDIATE': 'immediate', 'DETERMINISTIC': 'deterministic', 'NORMAL': 'normal', 'UNIFORM': 'uniform',
         'LOGNORMAL': 'lognormal', 'GAMMA': 'gamma', 'EXPONENTIAL': 'exponential'}
DEFAULT = ['exponential', 'timed', 1.0, 0.0, 0.0, 1.0, 0]


class Policy(Exception):
    pass


def number(text):
    try:
        value = float(text)
    except (TypeError, ValueError):
        raise Policy()
    if not math.isfinite(value): raise Policy()
    return value


# The native record of one defined PM4Py random variable, from the public
# getters: its type, parameter text, weight and priority.
def native_record(rv):
    kind, text = rv['type'], rv['params']
    if kind == 'IMMEDIATE': values = [0.0, 0.0, 0.0]
    elif kind in ('DETERMINISTIC', 'EXPONENTIAL'):
        if text == 'UNDEFINED': raise Policy()
        values = [number(text), 0.0, 0.0]
    elif kind in ('NORMAL', 'UNIFORM'):
        first, second = text.split(';')
        values = [number(first), number(second), 0.0]
    else:
        values = [number(part) for part in text.split(';')]
    weight = 1.0 if rv['weight'] is None else float(rv['weight'])
    priority = 0 if rv['priority'] is None else int(rv['priority'])
    if not math.isfinite(weight) or weight < 0 or not -2 ** 31 <= priority < 2 ** 31: raise Policy()
    return [NAMES[kind], 'immediate' if kind == 'IMMEDIATE' else 'timed'] + values + [weight, priority]


def render(value):
    if isinstance(value, float): return repr(value)
    return dumps(value)


def pnml_expected(result, stochastic):
    if not result['ok']: return 'ERR ' + result['error']
    text = 'OK ' + dumps(result['labels'])
    if not stochastic: return text
    smap = result['smap']
    try:
        defined = {tid: native_record(rv) for tid, rv in smap.items() if rv['type'] in NAMES}
    except Policy:
        return 'POLICY'
    if not defined: return text + ' null'
    records = []
    for tid, _ in result['labels']:
        record = [tid] + defined.get(tid, DEFAULT)
        records.append('[' + ','.join(render(x) for x in record) + ']')
    return text + ' [' + ','.join(records) + ']'


def main():
    out_dir = Path(sys.argv[1])
    extra = {'--filters': [], '--pnml': []}
    args = sys.argv[2:]
    while args:
        flag = args.pop(0)
        extra[flag].append(args[:2])
        args = args[2:]
    out_dir.mkdir(parents=True, exist_ok=True)

    sys.path.insert(0, str(Path(__file__).parent))
    d5 = __import__('generate-recheck-d5b33ef-fixture')
    filter_lines = []
    for cases_path, results_path in extra['--filters']:
        results = {r['id']: r for r in read_jsonl(results_path)}
        filter_lines += [d5.filter_line(c, results[c['id']]) for c in json.loads(Path(cases_path).read_text())]
    write_gz(out_dir / 'filter-cases.tsv.gz', filter_lines)

    pnml_lines = []
    for cases_path, results_path in extra['--pnml']:
        results = {r['id']: r for r in read_jsonl(results_path)}
        for case in json.loads(Path(cases_path).read_text()):
            result = results[case['id']]
            pnml_lines.append('\t'.join([case['id'], case['model'].encode().hex(),
                                         pnml_expected(result['plain'], False),
                                         pnml_expected(result['stochastic'], True)]))
    write_gz(out_dir / 'pnml-cases.tsv.gz', pnml_lines)
    print(len(filter_lines), 'filter cases,', len(pnml_lines), 'PNML models')


if __name__ == '__main__':
    main()
