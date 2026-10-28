#!/usr/bin/env python3
"""Generate src/backend/tests/recheck_61ebeee0_cases.inc from the saved public
reference outputs of the 61ebeee0 recheck.

Usage: generate-recheck-61ebeee0-fixture.py <discovery-conformance dir> <ocel-organizational dir> <output .inc>
"""
import sys
DISCOVERY, ORGANIZATIONAL, OUTPUT = sys.argv[1:4]

# --- discovery/conformance tables ---
sys.argv = [sys.argv[0], DISCOVERY, OUTPUT]
import json, pathlib, sys
S = pathlib.Path(sys.argv[1]); out = pathlib.Path(sys.argv[2])
def q(s): return json.dumps(s, ensure_ascii=False)
def num(x):
    if isinstance(x, bool): return 'true' if x else 'false'
    if isinstance(x, int): return str(x)
    return repr(float(x))
def err(x): return isinstance(x, dict) and ('error' in x and bool(x['error']) or 'exception' in x)
L = ['// Generated from the saved public reference outputs of the 61ebeee0 recheck', '// (discovery-conformance). Do not edit by hand.', '']
# Selector matrix.
L.append('const SelectorCase kSelectorCases[] = {')
for r in json.loads((S/'fresh-selector-reference.json').read_text()):
    ref = r['reference']; m = r['mode']
    if err(ref): e = 'error'
    elif m == 'precision': e = num(ref)
    elif m == 'fitness':
        if 'perc_fit_traces' in ref: e = ' '.join(num(ref[k]) for k in ('perc_fit_traces','average_trace_fitness','log_fitness'))
        else: e = ' '.join(num(ref[k]) for k in ('percFitTraces','averageFitness','log_fitness'))
    elif m == 'usage_reference':
        tc = ref['transition_counts']
        e = ' '.join(f'{t}:{tc[t][0]},{tc[t][1]}' for t in sorted(tc)) + f' arcs={ref["arc_count"]}'
    else: e = ','.join(ref['labels'])
    L.append(f'  {{{q(r["case"])}, {q(m)}, {q(r["variant"])}, {q(r["key"])}, {q(r["projection"])}, {q(e)}}},')
L.append('};'); L.append('')
# Replay matrix (forward and backwards), the eight shared per-trace fields.
L.append('const ReplayCase kReplayCases[] = {')
seen = set()
for f in ('fresh-replay-reference.json', 'fresh-backwards-reference.json'):
    for r in json.loads((S/f).read_text()):
        cmd = r['command']; log = pathlib.Path(cmd[1]).name; model = pathlib.Path(r['model']).name
        key = (log, model, r['variant'])
        if key in seen: continue
        seen.add(key)
        ref = r['reference']
        if err(ref): e = 'error'
        else:
            rows = []
            for t in ref:
                rows.append(' '.join([num(t['trace_is_fit']), num(t['trace_fitness']), num(t['produced_tokens']), num(t['consumed_tokens']),
                                      num(t['missing_tokens']), num(t['remaining_tokens']),
                                      '[' + ','.join(t['activated_transitions']) + ']', '[' + ','.join(t['transitions_with_problems']) + ']']))
            e = ' | '.join(rows)
        L.append(f'  {{{q(log)}, {q(model)}, {q(r["variant"])}, {q(e)}}},')
L.append('};'); L.append('')
# Extended marking equation and Woflan.
L.append('const AnalysisCase kAnalysisCases[] = {')
for r in json.loads((S/'fresh-analysis-reference.json').read_text()):
    ref = r['reference']; model = pathlib.Path(r['model']).name
    if err(ref): e = 'error'
    elif ref['value'] is None: e = 'none'
    else: e = num(ref['value'])
    trace = '|'.join(r.get('trace', [])); splits = '|'.join(str(s) for s in r.get('splits', []))
    L.append(f'  {{{q(r["family"])}, {q(model)}, {q(trace)}, {q(splits)}, {r.get("max_k", 0)}, {q(e)}}},')
L.append('};'); L.append('')
# Physical-arc usage records, sorted by kind and element.
L.append('const UsageCase kPhysicalUsageCases[] = {')
for r in json.loads((S/'fresh-physical-reference.json').read_text()):
    model = pathlib.Path(r['model']).name
    rows = sorted(' '.join([x['kind'], x['element'], num(x['log1_occurrences']), num(x['log2_occurrences']), num(x['total_occurrences']), x['label'], num(x['direction']), x['color']]) for x in r['reference'])
    L.append(f'  {{"physical-first.xes", "physical-second.xes", {q(model)}, {q(" | ".join(rows))}}},')
L.append('};')
out.write_text('\n'.join(L) + '\n')

# --- organizational repros (appended) ---
sys.argv = [sys.argv[0], ORGANIZATIONAL, OUTPUT]
import json, math, pathlib, sys
S = pathlib.Path(sys.argv[1]); out = pathlib.Path(sys.argv[2])
def q(s): return json.dumps(s, ensure_ascii=False)
def num(x):
    if isinstance(x, bool): return 'true' if x else 'false'
    if isinstance(x, int): return str(x)
    return repr(float(x))
def attr(a):
    key, value, kind = a
    if kind == 'int': k = 'i' if value < 2**63 else 'u'; text = str(value)
    elif kind == 'float': k = 'f'; text = 'nan' if isinstance(value, float) and math.isnan(value) else repr(float(value))
    elif kind == 'bool': k = 'b'; text = 'true' if value else 'false'
    elif kind == 'date': k = 'd'; text = value
    elif kind == 'string': k = 's'; text = value
    elif kind == 'null': k = 'n'; text = ''
    elif kind == 'na': k = 'a'; text = ''
    else: raise SystemExit('kind ' + kind)
    return f'{{{q(key)}, {q(text)}, \'{k}\'}}'
L = ['', 'const OrgCase kOrgCases[] = {']
for item in json.loads((S/'remaining-repros.json').read_text()):
    c = item['case']; ref = item['comparison']['reference']
    traces = []
    for t in c['log']:
        events = []
        for e in t['events']:
            assert e['start'] is None
            events.append(f'{{{q(e["a"])}, {q(e["r"])}, {q(e["t"])}, {q(e["life"])}, {{{", ".join(attr(a) for a in e["attrs"])}}}}}')
        traces.append(f'{{{q(t["id"])}, {{{", ".join(attr(a) for a in t["attrs"])}}}, {{{", ".join(events)}}}}}')
    params = ', '.join(f'{{{q(k)}, {q(v)}}}' for k, v in c['params'].items())
    if not ref['ok']: e = 'error'
    elif c['family'] == 'profile': e = num(ref['value'])
    else: e = ' | '.join(sorted(f'{s} {t} {a} {n} [' + ','.join(num(p) for p in perf) + ']' for s, t, a, n, perf in ref['value']))
    L.append(f'  {{{q(c["id"])}, {q(c["family"])}, {{{", ".join(traces)}}}, {{{params}}}, {"true" if c.get("performance") else "false"}, '
             f'{q(c.get("metric", ""))}, {q(c.get("activity", ""))}, {q(c.get("resource", ""))}, {q(c.get("resource2", ""))}, '
             f'{q(c.get("t1", ""))}, {q(c.get("t2", ""))}, {q(e)}}},')
L.append('};')
with out.open('a') as f: f.write('\n'.join(L) + '\n')
