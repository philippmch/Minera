import sys, types, json, hashlib, copy
B = sys.argv[1]
src = open(B + '/pandas-timestamp-full861-admission-prepared-1/map-native.py', 'rb').read()
m = types.ModuleType('t'); exec(compile(src, 'map-native.py', 'exec'), m.__dict__)
def s(v): return {'type': 'str', 'value': v}
def table(tag):
    return {'columns': {'type': 'list', 'values': [s('c')]}, 'rows': [{'values': [s(tag)]}], 'attrs': {'type': 'dict', 'items': []},
            'dtypes': ['object'], 'index': {'kind': 'RangeIndex', 'dtype': 'int64', 'names': {'type': 'list', 'values': [{'type': 'NoneType', 'value': None}]},
            'labels': {'type': 'list', 'values': [{'type': 'int', 'value': 0}]}}, 'categorical_dtypes': [None]}
def mkrow(i, n, err, case):
    inp = table('in%d' % n); args = {'type': 'list', 'values': [s('a'), s('b')]}; kw = {'type': 'dict', 'items': []}
    row = {'index': i, 'case': {'name': case}, 'module': m.MODULE, 'method': 'apply_events', 'input_before': inp, 'args_before': args,
           'kwargs_before': kw, 'call_shape': m.SHAPE, 'warnings': [], 'input_after': copy.deepcopy(inp), 'args_after': copy.deepcopy(args),
           'kwargs_after': copy.deepcopy(kw), 'input_unchanged': True, 'arguments_unchanged': True, 'returned_input_identity': False}
    if err: row['error'] = {'type': 'x.ValueError', 'message': 'm', 'args': []}
    else: row['result'] = table('out%d' % n)
    row['receipt_sha256'] = m.sha(m.encode(m.receipt(row)))
    return row
def bank(rows):
    intents = [{'index': r['index'], 'receipt_sha256': r['receipt_sha256'], 'case': r['case']['name'], 'module': r['module'], 'method': r['method']} for r in rows]
    audit = {'receipts': [{'receipt_sha256': r['receipt_sha256'], 'case': r['case']['name']} for r in rows], 'removed_duplicates': [], 'removed_prior_overlaps': []}
    plans = [{'index': r['index'], 'receipt_sha256': r['receipt_sha256'], 'receipt': m.receipt(r), 'case': r['case']['name']} for r in rows]
    jl = lambda xs: ('\n'.join(json.dumps(x) for x in xs) + '\n').encode()
    return {'records.jsonl': jl(rows), 'call-intents.jsonl': jl(intents), 'dedup-audit.json': json.dumps(audit).encode(), 'call-plan.jsonl': jl(plans)}
def build(mut=None):
    a = [mkrow(i + 1, i, i >= 302, 'c%d' % i) for i in range(500)]
    b = [mkrow(i + 1, 500 + i, True, 'd%d' % i) for i in range(361)]
    if mut: mut(a, b)
    o = {}
    for name, rows in zip(m.BANKS, (a, b)):
        for k, v in bank(rows).items(): o[name + '/' + k] = v
    return o
def run(label, mut=None, post=None):
    o = build(mut)
    if post: post(o)
    try: rows, c = m.identity_rows(o); print(label, 'PASS', c['outcomes'], c['calls'])
    except Exception as e: print(label, 'REJECT', type(e).__name__, e)
run('baseline')
def dup(a, b):
    b[0] = mkrow(1, 0, False, 'c0')  # same canonical receipt as a[0], in second bank
run('cross-bank duplicate', dup)
def mutate(a, b): a[3]['input_after']['rows'][0]['values'][0]['value'] = 'changed'
run('mutation input_after only (flag still True)', mutate)
def warn(a, b): a[3]['warnings'] = ['W']
run('warning', warn)
def flag(a, b): a[4]['arguments_unchanged'] = False
run('mutation flag', flag)
def gap(o): o[m.BANKS[0] + '/call-intents.jsonl'] = b'\n'.join(o[m.BANKS[0] + '/call-intents.jsonl'].splitlines()[:-1]) + b'\n'
run('alignment gap (missing intent)', post=gap)
def both(a, b): a[5]['error'] = {'type': 'x', 'message': 'm', 'args': []}
run('result+error', both)
def removed(o):
    k = m.BANKS[1] + '/dedup-audit.json'; d = json.loads(o[k]); d['removed_duplicates'] = ['x']; o[k] = json.dumps(d).encode()
run('audit removed duplicates', post=removed)
def tamper(a, b): a[6]['receipt_sha256'] = '0' * 64
run('stale receipt sha', tamper)
def order(a, b): a[7], a[8] = a[8], a[7]
run('swapped index order', order)
def crlf(o):
    k = m.BANKS[0] + '/records.jsonl'; o[k] = o[k].replace(b'\n', b'\r\n')
run('CRLF records (splitlines tolerates)', post=crlf)
def blank(o):
    k = m.BANKS[0] + '/records.jsonl'; o[k] = o[k] + b'\n'
run('trailing blank line in records', post=blank)
# reconstruction determinism and prelude
o = build(); rows, c = m.identity_rows(o)
out1, mp, sid = m.reconstruct(rows, 'PRELUDE')
out2, _, _ = m.reconstruct(rows, 'PRELUDE')
print('reconstruct deterministic', out1 == out2, 'snapshots', mp['snapshots'], 'starts with prelude', out1.startswith(b'PRELUDE\nMINERA_NOINLINE EventTable snapshot_0('))
