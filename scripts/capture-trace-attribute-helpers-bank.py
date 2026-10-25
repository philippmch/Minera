"""Capture the public bank for the helper modules of
algo.clustering.trace_attribute_driven (util.filter_subsets,
variants.act_dist_calc, variants.suc_dist_calc, variants.sim_calc,
variants.logslice_dist, leven_dist.leven_dist_calc, dfg.dfg_dist,
linkage_method.linkage_avg, merge_log.merge_log).

Seeded public calls on seeded event logs (labels a..e, repeated variants),
each run in fresh interpreters under two hash seeds, which must agree (for
act_sim_med, whose median relies on numpy's unstable argsort, every value
seen is kept).

The bank is a gzip text file of lines read by
minera_trace_attribute_helpers_parity_tests: a tag, then fields separated by
" | ". A trace is its labels written together ('-' for the empty trace), a
log or variant list its traces joined by ',' ('.' when empty), a list of
logs those joined by ';'. A trace with an attribute is trace@value ('~' when
missing; its concept:name is c<position>). Floats are Python reprs, errors
'! Type message'. See the test for each tag's fields.
"""
import argparse
import gzip
import json
import os
from pathlib import Path
import random
import subprocess
import sys

BANK_SEED = 20261025
HASH_SEEDS = (0, 1)
ROOT = Path(__file__).resolve().parent.parent


def word(r, alphabet, low, high):
    return ''.join(r.choice(alphabet) for _ in range(r.randint(low, high)))


def log(r, empty=0.0, letters='abcde', variants=6, repeat=5, length=4):
    alphabet = letters[:r.randint(1, len(letters))]
    vs = [word(r, alphabet, 0 if r.random() < empty else 1, length) for _ in range(r.randint(1, variants))]
    out = [v for v in vs for _ in range(r.randint(1, repeat))]
    r.shuffle(out)
    return out


def attributed(r, traces, values='xyzw'):
    return [[t, r.choice(values) if r.random() < 0.9 else None] for t in traces]


def generate(count):
    r = random.Random(BANK_SEED)
    cases = []
    percents = [0, 0.1, 0.3, 0.5, 0.6, 0.8, 1.0]
    units = [0.05, 0.1, 0.15, 0.2, 0.25, 0.3, 0.33, 0.5, 0.7, 1.0, 1.5]
    for _ in range(count):
        t = log(r, 0.05)
        cases.append({'f': 'ata', 'log': attributed(r, t), 'values': r.sample('xyzw', r.randint(0, 3)),
                      'key': r.choice(['grp', 'concept:name', 'missing']), 'positive': r.random() < 0.6})
        cases.append({'f': 's2v', 'log': log(r, 0.05), 'th': r.randint(0, 6), 'num': r.randint(0, 7)})
        cases.append({'f': 's2d', 'log': log(r, 0.05), 'th': r.randint(0, 6), 'num': r.randint(0, 7)})
        cases.append({'f': 's2n', 'log': log(r, 0.05), 'num': r.randint(0, 7)})
        cases.append({'f': r.choice(['sp0', 'sp1', 'sp2']), 'log': log(r, 0.05),
                      'p': r.choice(percents + [round(r.random(), 3)])})
        cases.append({'f': 'lpa', 'log': log(r, 0.05), 'unit': r.choice(units + [round(r.random(), 3)])})
        cases.append({'f': 'lp', 'log': log(r, 0.05), 'unit': r.choice(units + [round(r.random(), 3)])})
        t = log(r, 0.05)
        cases.append({'f': 'avf', 'log': t, 'admitted': r.sample(sorted(set(t)), r.randint(0, len(set(t)))),
                      'positive': r.random() < 0.6})
        cases.append({'f': 'ova', 'variant': word(r, 'abcd', 0, 6)})
        cases.append({'f': 'ovs', 'variant': word(r, 'abc', 0, 6), 'binarize': r.random() < 0.5})
        t1, t2 = log(r), log(r)
        if r.random() < 0.15:
            t2 = list(t1)
        v = []
        for t in (t1, t2):
            vl = sorted(set(t))
            r.shuffle(vl)
            v.append(vl[:r.randint(0, len(vl))] if r.random() < 0.5 else None)
        f = r.choice(['asim', 'amed', 'adual', 'ssim', 'sdual', 'dist'])
        cases.append({'f': f, 'log1': t1, 'log2': t2, 'v1': v[0], 'v2': v[1], 'v1th': r.randint(0, 3),
                      'v1num': r.randint(0, 4), 'v2th': r.randint(0, 3), 'v2num': r.randint(0, 4),
                      'th': r.choice([0, 0, 1, 2, 3]), 'num': r.choice([0, 1, 2, 5]), 'alpha': r.choice([0, 0.3, 0.5, 1])})
        t1, t2 = log(r), log(r)
        if r.random() < 0.1:
            t2 = list(t1)
        cases.append({'f': r.choice(['asp', 'aspa', 'aspas', 'ssp', 'sspa', 'lev', 'leva']), 'log1': t1, 'log2': t2,
                      'p1': r.choice([1.0, 0.5, 0.8, 0.3, 0.0]), 'p2': r.choice([1.0, 0.5, 0.8, 0.3])})
        dfg = {}
        for _ in range(r.randint(0, 7)):
            dfg[word(r, 'abcd', 2, 2)] = r.randint(1, 5)
        cases.append({'f': 'osuc', 'dfg': list(dfg.items()), 'p': r.choice([0, 0.3, 0.5, 0.7, 1.0, 1.4, round(r.random(), 2)])})
        size = r.randint(0, 6)
        cases.append({'f': 'ipc', 'x': [r.randint(0, 5) for _ in range(size)], 'y': [r.randint(0, 5) for _ in range(size)]})
        cases.append({'f': 'l2s', 'log': attributed(r, log(r, 0.05), '123'), 'value': r.choice('1234')})
        t1 = log(r, 0.05)
        cases.append({'f': r.choice(['sda', 'sds']), 'log1': t1, 'log2': log(r, 0.05) if r.random() < 0.85 else list(t1),
                      'unit': r.choice([0.1, 0.2, 0.25, 0.3, 0.5, 0.7, 1.0, 1.5, round(r.random(), 3)])})
        l1, l2 = [], []
        for _ in range(r.randint(0, 6)):
            l1.append(word(r, 'abc', 0, 3))
        for _ in range(r.randint(0, 4)):
            l2.append(word(r, 'abc', 0, 3))
        cases.append({'f': 'lpp', 'list1': l1, 'list2': l2})
        both = (log(r, 0.1, 'abc', 4, 3, 3), log(r, 0.1, 'abc', 4, 3, 3))
        if r.random() < 0.08:
            both = (['', ''] if r.random() < 0.5 else ['a'], [''])
        cases.append({'f': r.choice(['dfa', 'dfs', 'dfg', 'dfm']), 'log1': both[0], 'log2': both[1],
                      'alpha': r.choice([0, 0.3, 0.5, 1])})
        n = r.randint(1, 7)
        cases.append({'f': r.choice(['lka', 'lkd', 'lkm', 'lkl']), 'logs': [log(r, 0, 'abcd', 4, 4, 4) for _ in range(n)],
                      'dist': [r.choice([round(r.random(), 2), r.random(), 0.5, 0.0, 1.0]) for _ in range(n * (n - 1) // 2)],
                      'alpha': r.choice([0, 0.3, 0.5, 1]), 'percent': r.choice([1.0, 0.5, 0.8, 0.3])})
        cases.append({'f': 'um', 'logs': [attributed(r, log(r, 0.05)) for _ in range(r.randint(0, 3))]})
        n = r.randint(2, 8)
        cases.append({'f': r.choice(['an', 'lt', 'cl']), 'dist': [r.choice([r.random(), round(r.random(), 1)]) for _ in range(n * (n - 1) // 2)],
                      'method': r.choice(['average', 'single', 'complete']), 'maxclust': r.randint(0, n + 1),
                      'names': [r.choice(['x', 'y', 'z', 'w', 'q', '10', '9', 'B']) + str(i) for i in range(n)],
                      'log': attributed(r, log(r, 0.05, repeat=3), [f'x{i}' for i in range(n)])})
        cases.append({'f': r.choice(['l2sm', 'ls']), 'log': attributed(r, log(r, 0.05)),
                      'values': r.sample('xyzw', r.randint(0, 3)), 'key': r.choice(['grp', 'concept:name', 'missing'])})
    for _ in range(4):
        cases.append({'f': 'ad'})
        cases.append({'f': 'ml', 'path': r.choice(['/tmp/', 'logs/', '']), 'cate': r.choice(['c', 'AMOUNT', '']),
                      'iter': r.randint(0, 3)})
    return cases


CHILD = r'''
import contextlib, datetime, io, json, sys, warnings
warnings.filterwarnings('ignore')
import numpy as np, pandas as pd
from scipy.cluster.hierarchy import linkage, to_tree
from scipy.spatial.distance import squareform
from pm4py.objects.log.obj import Event, EventLog, Trace
from pm4py.algo.clustering.trace_attribute_driven.util import filter_subsets as fs
from pm4py.algo.clustering.trace_attribute_driven.variants import act_dist_calc as ad, suc_dist_calc as sd
from pm4py.algo.clustering.trace_attribute_driven.variants import sim_calc as sc, logslice_dist as ld
from pm4py.algo.clustering.trace_attribute_driven.leven_dist import leven_dist_calc as lv
from pm4py.algo.clustering.trace_attribute_driven.dfg import dfg_dist as dd
from pm4py.algo.clustering.trace_attribute_driven.linkage_method import linkage_avg as la
from pm4py.algo.clustering.trace_attribute_driven.merge_log import merge_log as ml
origin = datetime.datetime(2024, 1, 1, tzinfo=datetime.timezone.utc)
def mk(traces, key='grp'):
    log, minute = EventLog(), 0
    for n, item in enumerate(traces):
        word, value = item if isinstance(item, list) else (item, None)
        t = Trace()
        t.attributes['concept:name'] = f'c{n}'
        if value is not None:
            t.attributes[key] = value
        for a in word:
            t.append(Event({'concept:name': a, 'time:timestamp': origin + datetime.timedelta(minutes=minute)}))
            minute += 1
        log.append(t)
    return log
def words(log): return [''.join(e['concept:name'] for e in t) for t in log]
def names(log): return [t.attributes['concept:name'] for t in log]
def word(v): return ''.join(v)
def records(df): return [[word(d['variant']), int(d['count'])] for d in df.to_dict('records')]
def num(x):
    x = float(x)
    return 'nan' if x != x else repr(x)
def varlist(case, k, th, n, log):
    v = case[k]
    return [tuple(x) for x in v] if v is not None else fs.sublog2varlist(log, case[th], case[n])
def run(c):
    f = c['f']
    if f == 'ata':
        params = {'positive': c['positive']}
        if c['key'] != 'concept:name':
            params['pm4py:param:attribute_key'] = c['key']
        return names(fs.apply_trace_attributes(mk(c['log']), c['values'], parameters=params))
    if f == 's2v': return [word(v) for v in fs.sublog2varlist(mk(c['log']), c['th'], c['num'])]
    if f == 's2d': return records(fs.sublog2df(mk(c['log']), c['th'], c['num']))
    if f == 's2n': return records(fs.sublog2df_num(mk(c['log']), c['num']))
    if f in ('sp0', 'sp1', 'sp2'):
        fn = {'sp0': fs.sublog_percent, 'sp1': fs.sublog_percent2actlist, 'sp2': fs.sublog_percent2varlist}[f]
        df, vl = fn(mk(c['log']), c['p'])
        return [records(df), [word(v) for v in vl]]
    if f == 'lpa':
        sl, counts = fs.logslice_percent_act(mk(c['log']), c['unit'])
        return [[[word(v) for v in s] for s in sl], [int(x) for x in counts]]
    if f == 'lp':
        sl, counts = fs.logslice_percent(mk(c['log']), c['unit'])
        return [[words(s) for s in sl], [int(x) for x in counts]]
    if f == 'avf':
        return words(fs.apply_variants_filter(mk(c['log']), [tuple(v) for v in c['admitted']], parameters={'positive': c['positive']}))
    if f == 'ad': return fs.act_dist([], [], mk([]), mk([]), 0)
    if f == 'ova': return [[d['var'], int(d['freq'])] for d in ad.occu_var_act(tuple(c['variant'])).to_dict('records')]
    if f == 'ovs':
        return [[d['direct_suc'], int(d['freq'])] for d in sd.occu_var_suc(tuple(c['variant']), parameters={'binarize': c['binarize']}).to_dict('records')]
    if f in ('asim', 'amed', 'adual', 'ssim', 'sdual', 'dist'):
        l1, l2 = mk(c['log1']), mk(c['log2'])
        v1 = varlist(c, 'v1', 'v1th', 'v1num', l1)
        v2 = varlist(c, 'v2', 'v2th', 'v2num', l2)
        out = [[word(v) for v in v1], [word(v) for v in v2]]
        with contextlib.redirect_stdout(io.StringIO()):
            try:
                if f == 'dist':
                    value = sc.dist_calc(v1, v2, l1, l2, c['th'], c['num'], c['alpha'])
                else:
                    fn = {'asim': ad.act_sim, 'amed': ad.act_sim_med, 'adual': ad.act_sim_dual, 'ssim': sd.suc_sim, 'sdual': sd.suc_sim_dual}[f]
                    value = fn(v1, v2, l1, l2, c['th'], c['num'])
                out.append(num(value))
            except Exception as e:
                out.append(['!', type(e).__name__, str(e)])
        return out
    if f in ('asp', 'aspa', 'aspas', 'ssp', 'sspa', 'lev', 'leva'):
        l1, l2 = mk(c['log1']), mk(c['log2'])
        with contextlib.redirect_stdout(io.StringIO()):
            if f == 'aspas':
                actset = pd.DataFrame({'var': ['a', 'b']})
                return num(ad.act_sim_percent_avg_actset(l1, l2, c['p1'], c['p2'], actset))
            fn = {'asp': ad.act_sim_percent, 'aspa': ad.act_sim_percent_avg, 'ssp': sd.suc_sim_percent, 'sspa': sd.suc_sim_percent_avg,
                  'lev': lv.leven_dist, 'leva': lv.leven_dist_avg}[f]
            return num(fn(l1, l2, c['p1'], c['p2']))
    if f == 'osuc':
        dfg = {(k[0], k[1]): v for k, v in c['dfg']}
        return [[d['suc'][0] + d['suc'][1], int(d['freq'])] for d in sd.occu_suc(dfg, c['p']).to_dict('records')]
    if f == 'ipc':
        return [num(v) for v in sc.inner_prod_calc(pd.DataFrame({'freq_x': c['x'], 'freq_y': c['y']}))]
    if f == 'l2s': return names(ld.log2sublog(mk(c['log'], 'AMOUNT_REQ'), c['value']))
    if f in ('sda', 'sds'):
        fn = ld.slice_dist_act if f == 'sda' else ld.slice_dist_suc
        with contextlib.redirect_stdout(io.StringIO()):
            return num(fn(mk(c['log1']), mk(c['log2']), c['unit']))
    if f == 'lpp': return list(lv.leven_preprocess(c['list1'], c['list2']))
    if f in ('dfa', 'dfs', 'dfg', 'dfm'):
        l1, l2 = mk(c['log1']), mk(c['log2'])
        if f == 'dfa': return num(dd.dfg_dist_calc_act(l1, l2))
        if f == 'dfs': return num(dd.dfg_dist_calc_suc(l1, l2))
        if f == 'dfg': return [num(v) for v in dd.dfg_dist_calc(l1, l2)]
        return num(dd.dfg_dist_calc_minkowski(l1, l2, c['alpha']))
    if f in ('lka', 'lkd', 'lkm', 'lkl'):
        fn = {'lka': la.linkage_avg, 'lkd': la.linkage_dfg_update, 'lkm': la.linkage_DMM_update, 'lkl': la.linkage_DMM_update_leven}[f]
        n = len(c['logs'])
        matrix = squareform(np.array(c['dist'])) if n > 1 else np.zeros((1, 1))
        with contextlib.redirect_stdout(io.StringIO()):
            z = fn([mk(t) for t in c['logs']], matrix, c['alpha'], c['percent'])
        return [[num(x) for x in row] for row in np.asarray(z).tolist()]
    if f == 'um':
        logs = [mk(t) for t in c['logs']]
        return [[word(e['concept:name'] for e in t), t.attributes['concept:name'], t.attributes.get('grp')] for t in ml.update_merge(logs)]
    if f in ('an', 'lt', 'cl'):
        z = linkage(np.array(c['dist']), c['method'])
        zl = [[num(x) for x in row] for row in z.tolist()]
        if f == 'cl':
            sublogs, groups = ml.clusteredlog(z, c['maxclust'], [f'x{i}' for i in range(len(c['names']))], mk(c['log']), 'DMM', 'grp')
            return [zl, [names(s) for s in sublogs], [list(g) for g in groups]]
        parent = {'name': 'root', 'children': []}
        result = ml.add_node(to_tree(z), parent)
        assert result is None
        tree = parent['children'][0]
        if f == 'an': return [zl, tree]
        leaves = ml.label_tree(tree, {i: n for i, n in enumerate(c['names'])})
        return [zl, leaves, tree]
    if f in ('l2sm', 'ls'):
        lg = mk(c['log'])
        if f == 'l2sm': return names(ml.log2sublog(lg, c['values'][0] if c['values'] else 'x', c['key']))
        return names(ml.logslice(lg, c['values'], c['key']))
    if f == 'ml': return ml.merge_log(c['path'], c['cate'], c['iter'])
    raise ValueError(f)
out = []
for case in json.load(sys.stdin):
    try:
        out.append(run(case))
    except Exception as e:
        out.append(['!', type(e).__name__, str(e)])
print(json.dumps(out))
'''


def capture(cases, seed):
    env = dict(os.environ, PYTHONHASHSEED=str(seed))
    result = subprocess.run([sys.executable, '-I', '-B', '-c', CHILD], input=json.dumps(cases),
                            capture_output=True, text=True, env=env)
    if result.returncode:
        raise SystemExit(result.stderr[-3000:])
    return json.loads(result.stdout.strip().splitlines()[-1])


def is_error(value):
    return isinstance(value, list) and len(value) == 3 and value[0] == '!'


def error_text(value):
    return f'! {value[1]} {value[2]}'


def traces(ts):
    return ','.join(t or '-' for t in ts) or '.'


def attr_traces(ts):
    return ','.join(f'{t or "-"}@{"~" if v is None else v}' for t, v in ts) or '.'


def tokens(xs):
    return ' '.join(str(x) for x in xs) or '.'


def pairs(xs):
    return ' '.join(f'{k or "-"}={v}' for k, v in xs) or '.'


def tree_text(node):
    head = node['name'] if 'name' in node else str(node['node_id'])
    return '(' + ' '.join([head] + [tree_text(c) for c in node['children']]) + ')'


def z_text(z):
    return ';'.join(' '.join(row) for row in z) or '.'


def line(c, value):
    f = c['f']
    if is_error(value) and f not in ('asim', 'adual', 'ssim', 'sdual', 'dist'):
        out = error_text(value)
    elif f == 'ata':
        out = tokens(value)
    elif f == 's2v':
        out = traces(value)
    elif f in ('s2d', 's2n'):
        out = pairs(value)
    elif f in ('sp0', 'sp1', 'sp2'):
        out = f'{pairs(value[0])} | {traces(value[1])}'
    elif f == 'lpa':
        out = f'{";".join(traces(s) for s in value[0]) or "."} | {tokens(value[1])}'
    elif f == 'lp':
        out = f'{";".join(traces(s) for s in value[0]) or "."} | {tokens(value[1])}'
    elif f == 'avf':
        out = traces(value)
    elif f in ('ova', 'ovs', 'osuc'):
        out = pairs(value)
    elif f in ('asim', 'amed', 'adual', 'ssim', 'sdual', 'dist'):
        if is_error(value):
            raise SystemExit(f'unexpected error {value} for {c}')
        result = value[2] if not is_error(value[2]) else error_text(value[2])
        out = f'{traces(value[0])} | {traces(value[1])} | {result}'
    elif f in ('ipc', 'dfg'):
        out = tokens(value)
    elif f == 'lpp':
        out = f'{value[0] or "-"} {value[1] or "-"}'
    elif f in ('lka', 'lkd', 'lkm', 'lkl'):
        out = z_text(value)
    elif f == 'um':
        out = ','.join(f'{t or "-"}@{"~" if v is None else v}@{name}' for t, name, v in value) or '.'
    elif f == 'an':
        out = f'{z_text(value[0])} | {tree_text(value[1])}'
    elif f == 'lt':
        out = f'{z_text(value[0])} | {tokens(value[1])} | {tree_text(value[2])}'
    elif f == 'cl':
        out = f'{z_text(value[0])} | {";".join(tokens(s) for s in value[1]) or "."} | {";".join(tokens(g) for g in value[2]) or "."}'
    elif f in ('l2s', 'l2sm', 'ls'):
        out = tokens(value)
    else:
        out = value if isinstance(value, str) else json.dumps(value)
    args = {
        'ata': lambda: f'{attr_traces(c["log"])} | {tokens(c["values"])} | {c["key"]} | {int(c["positive"])}',
        's2v': lambda: f'{traces(c["log"])} | {c["th"]} {c["num"]}',
        's2d': lambda: f'{traces(c["log"])} | {c["th"]} {c["num"]}',
        's2n': lambda: f'{traces(c["log"])} | {c["num"]}',
        'lpa': lambda: f'{traces(c["log"])} | {c["unit"]!r}',
        'lp': lambda: f'{traces(c["log"])} | {c["unit"]!r}',
        'avf': lambda: f'{traces(c["log"])} | {traces(c["admitted"])} | {int(c["positive"])}',
        'ad': lambda: '.',
        'ova': lambda: c['variant'] or '-',
        'ovs': lambda: f'{c["variant"] or "-"} | {int(c["binarize"])}',
        'osuc': lambda: f'{pairs(c["dfg"])} | {c["p"]!r}',
        'ipc': lambda: f'{tokens(c["x"])} | {tokens(c["y"])}',
        'l2s': lambda: f'{attr_traces(c["log"])} | {c["value"]}',
        'lpp': lambda: f'{traces(c["list1"])} | {traces(c["list2"])}',
        'lka': lambda: linkage_args(c), 'lkd': lambda: linkage_args(c), 'lkm': lambda: linkage_args(c), 'lkl': lambda: linkage_args(c),
        'um': lambda: ';'.join(attr_traces(t) for t in c['logs']) or '.',
        'an': lambda: '.',
        'lt': lambda: tokens(c['names']),
        'cl': lambda: f'{c["maxclust"]} | {attr_traces(c["log"])}',
        'l2sm': lambda: f'{attr_traces(c["log"])} | {c["values"][0] if c["values"] else "x"} | {c["key"]}',
        'ls': lambda: f'{attr_traces(c["log"])} | {tokens(c["values"])} | {c["key"]}',
        'ml': lambda: f'{c["path"] or "-"} {c["cate"] or "-"} {c["iter"]}',
    }
    if f in ('sp0', 'sp1', 'sp2'):
        head = f'{traces(c["log"])} | {c["p"]!r}'
    elif f in ('asim', 'amed', 'adual', 'ssim', 'sdual', 'dist'):
        head = f'{traces(c["log1"])} | {traces(c["log2"])} | {c["th"]} {c["num"]} {c["alpha"]!r}'
    elif f in ('asp', 'aspa', 'aspas', 'ssp', 'sspa', 'lev', 'leva'):
        head = f'{traces(c["log1"])} | {traces(c["log2"])} | {c["p1"]!r} {c["p2"]!r}'
    elif f in ('sda', 'sds'):
        head = f'{traces(c["log1"])} | {traces(c["log2"])} | {c["unit"]!r}'
    elif f in ('dfa', 'dfs', 'dfg', 'dfm'):
        head = f'{traces(c["log1"])} | {traces(c["log2"])} | {c["alpha"]!r}'
    else:
        head = args[f]()
    return f'{f} {head} | {out}'


def linkage_args(c):
    return f'{";".join(traces(t) for t in c["logs"])} | {tokens(c["dist"]) if c["dist"] else "."} | {c["alpha"]!r} {c["percent"]!r}'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--count', type=int, default=300)
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/trace-attribute-helpers-bank.txt.gz')
    args = parser.parse_args()
    cases = generate(args.count)
    runs = [capture(cases, seed) for seed in HASH_SEEDS]
    lines, summary = [], {}
    for index, case in enumerate(cases):
        seen = [run[index] for run in runs]
        value = seen[0]
        if any(v != value for v in seen[1:]):
            if case['f'] != 'amed':
                raise SystemExit(f'hash seeds disagree on {case}: {seen}')
            value = value[:2] + [' '.join(sorted({error_text(v[2]).replace(' ', '_') if is_error(v[2]) else v[2] for v in seen}))]
        key = case['f'] + (' error' if is_error(value) or (isinstance(value, list) and len(value) == 3 and is_error(value[2])) else '')
        summary[key] = summary.get(key, 0) + 1
        lines.append(line(case, value))
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0, filename='') as stream:
        stream.write(('\n'.join(lines) + '\n').encode())
    print(json.dumps(dict(sorted(summary.items()))))


if __name__ == '__main__':
    main()
