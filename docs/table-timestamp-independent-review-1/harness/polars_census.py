import sys, types, json, copy
B = sys.argv[1]
src = open(B + '/polars-timestamp-full732-admission-prepared-1/map-native.py', 'rb').read()
m = types.ModuleType('t'); exec(compile(src, 'map-native.py', 'exec'), m.__dict__)
def s(v): return {'type': 'str', 'value': v}
def frame(tag): return {'columns': ['c'], 'dtypes': ['String'], 'rows': [{'values': [s(tag)]}]}
def mkrow(i, n, kind, case):
    inp = frame('in%d' % n); kw = {'type': 'dict', 'items': [[s('dt1'), s('a')], [s('dt2'), s('b')]]}
    receipt = {'module': m.MODULE, 'function': 'apply_events', 'call_shape': m.SHAPE, 'input_kind': 'LazyFrame', 'input': inp, 'kwargs': kw, 'category_observables': []}
    row = {'index': i, 'case': {'name': case, 'function': 'apply_events'}, 'module': m.MODULE, 'receipt': receipt, 'receipt_sha256': m.fingerprint(receipt),
           'input_before': copy.deepcopy(inp), 'input_categories_before': [], 'kwargs_before': copy.deepcopy(kw), 'kwargs_after': copy.deepcopy(kw),
           'input_after': copy.deepcopy(inp), 'input_unchanged': True, 'kwargs_unchanged': True, 'warnings': [], 'apply_returned': kind != 'apply'}
    if kind == 'frame': row.update(result=frame('out%d' % n), result_categories=[], result_kind='frame', returned_input_identity=False)
    else: row['error'] = {'type': 'polars.exceptions.X', 'phase': kind, 'message': 'm'}
    return row
def bank(rows):
    jl = lambda xs: ('\n'.join(json.dumps(x) for x in xs) + '\n').encode()
    return {'records.jsonl': jl(rows), 'call-intents.jsonl': jl([{'index': r['index'], 'receipt_sha256': r['receipt_sha256'], 'case': r['case']['name'], 'function': r['case']['function']} for r in rows]),
            'dedup-audit.json': json.dumps({'receipts': [{'receipt_sha256': r['receipt_sha256'], 'case': r['case']['name']} for r in rows], 'duplicates': [], 'prior_overlaps': []}).encode()}
def build(mut=None):
    kinds = ['frame'] * 229 + ['apply'] * 427 + ['collect'] * 76
    rows = [mkrow(0, n, k, 'c%d' % n) for n, k in enumerate(kinds)]
    banks = [rows[:300], rows[300:600], rows[600:]]
    for b in banks:
        for i, r in enumerate(b, 1): r['index'] = i
    if mut: mut(banks)
    o = {}
    for name, rs in zip(m.BANKS, banks):
        for k, v in bank(rs).items(): o[name + '/' + k] = v
    return o
def run(label, mut=None):
    try: rows, c = m.identity_rows(build(mut)); print(label, 'PASS', c['outcomes'], 'mutations=', c['mutations'], 'warnings=', c['algorithm_warnings'])
    except Exception as e: print(label, 'REJECT', type(e).__name__, e)
run('baseline')
def mut_in(b): b[0][3]['input_after']['rows'][0]['values'][0]['value'] = 'changed'
run('input_after differs, input_unchanged flag True', mut_in)
def warn(b): b[0][3]['warnings'] = ['UserWarning']
run('one warning', warn)
def dup(b):
    r = copy.deepcopy(b[0][0]); r['index'] = 1; b[1][0] = r
run('cross-bank duplicate', dup)
def kwmut(b): b[0][4]['kwargs_after']['items'][0][1]['value'] = 'z'
run('kwargs_after differs', kwmut)
def reorder(b):
    r = b[0][5]; r['kwargs_before']['items'].reverse(); r['kwargs_after'] = copy.deepcopy(r['kwargs_before']); r['receipt']['kwargs'] = copy.deepcopy(r['kwargs_before']); r['receipt_sha256'] = m.fingerprint(r['receipt'])
run('reordered keywords dt2,dt1 (accepted and recorded)', reorder)
def badenum(b):
    r = b[0][6]; r['kwargs_before']['items'].append([s('parameters'), {'type': 'dict', 'items': [[{'type': 'enum', 'value': s('k')}, s('v')]]}])
    r['kwargs_after'] = copy.deepcopy(r['kwargs_before']); r['receipt']['kwargs'] = copy.deepcopy(r['kwargs_before']); r['receipt_sha256'] = m.fingerprint(r['receipt'])
run('enum key lacking module/name (shape unchecked)', badenum)
