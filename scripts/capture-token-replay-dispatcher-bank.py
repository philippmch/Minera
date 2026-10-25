"""Public PM4Py 2.7.22 token replay dispatcher capture; no native calls.

The 40 frozen nets of docs/reference-2.7.22-conformance-parity-nets.json.gz
get seeded logs (repeated variants, unknown labels, empty traces). Each log is
replayed through the public `algo.conformance.tokenreplay.algorithm.apply`
under both variants (and the default) on the EventLog and on the equivalent
DataFrame, whose rows interleave the cases with increasing timestamps under
unsorted case ids. For the DataFrame the public `get_diagnostics_dataframe`
records are kept as well. Rows are recorded by PNML names; each run is a
fresh interpreter per PYTHONHASHSEED and a call whose output varies keeps
every distinct one.

    python -I -B scripts/capture-token-replay-dispatcher-bank.py \
        --output docs/token-replay-dispatcher-bank.json.gz
"""
import argparse
import concurrent.futures
import gzip
import json
import os
from pathlib import Path
import random
import subprocess
import sys

BANK_SEED = 20261025
HASH_SEEDS = tuple(range(16))
NETS = Path(__file__).resolve().parent.parent / 'docs/reference-2.7.22-conformance-parity-nets.json.gz'
VARIANTS = ('default', 'TOKEN_REPLAY', 'BACKWARDS')


def generate():
    rng = random.Random(BANK_SEED)
    corpus = json.loads(gzip.decompress(NETS.read_bytes()))['cases']
    cases = []
    for net in corpus:
        words = [list(word) for word in net['traces']]
        labels = sorted({a for word in words for a in word}) or ['a']
        for round_index in range(5):
            traces = []
            for _ in range(rng.randint(2, 8)):
                if traces and rng.random() < 0.25:
                    traces.append(list(rng.choice(traces)))
                elif rng.random() < 0.6 and words:
                    traces.append(list(rng.choice(words)))
                else:
                    traces.append([rng.choice(labels + ['zz']) for _ in range(rng.randint(0, 5))])
            case_ids = rng.sample([f'c{n}' for n in range(100)], len(traces))
            # Interleave the cases' events, keeping each case's own order.
            pending = [[index, 0] for index, trace in enumerate(traces) if trace]
            rows = []
            while pending:
                slot = rng.randrange(len(pending))
                index, position = pending[slot]
                rows.append([case_ids[index], traces[index][position]])
                pending[slot][1] += 1
                if pending[slot][1] == len(traces[index]):
                    pending.pop(slot)
            cases.append({'name': f'{net["name"]}-{round_index}', 'pnml': net['model_pnml'],
                          'traces': traces, 'case_ids': case_ids, 'frame_rows': rows})
    return cases


CHILD = r'''
import json, os, sys, tempfile, warnings
warnings.filterwarnings('ignore')
import pandas as pd
from pm4py.objects.petri_net.importer import importer as pnml_importer
from pm4py.algo.conformance.tokenreplay import algorithm as tb
from pm4py.objects.log.obj import EventLog, Trace, Event
def name(node):
    return node if isinstance(node, str) else node.name
def row(r):
    # Backwards rows carry fewer keys; absent fields are recorded as absent.
    out = {'fit': r['trace_is_fit'], 'fitness': r['trace_fitness'],
           'counts': [r['missing_tokens'], r['consumed_tokens'], r['remaining_tokens'], r['produced_tokens']],
           'keys': sorted(r)}
    if 'activated_transitions' in r:
        out['activated'] = [name(t) for t in r['activated_transitions']]
    if 'transitions_with_problems' in r:
        out['problems'] = [name(t) for t in r['transitions_with_problems']]
    if 'enabled_transitions_in_marking' in r:
        out['enabled'] = sorted(name(t) for t in r['enabled_transitions_in_marking'])
    if 'reached_marking' in r:
        out['reached'] = sorted([name(p), int(c)] for p, c in r['reached_marking'].items())
    return out
def records(frame):
    return {'columns': list(frame.columns),
            'rows': [[value.item() if hasattr(value, 'item') else value for value in record]
                     for record in frame.itertuples(index=False)]}
cases = json.load(sys.stdin)
out = []
directory = tempfile.mkdtemp()
stamp = pd.Timestamp('2020-01-01')
for case in cases:
    path = os.path.join(directory, 'net.pnml')
    with open(path, 'w', encoding='utf-8') as stream:
        stream.write(case['pnml'])
    net, im, fm = pnml_importer.apply(path)
    log = EventLog([Trace([Event({'concept:name': a}) for a in trace], attributes={'concept:name': cid})
                    for trace, cid in zip(case['traces'], case['case_ids'])])
    frame = pd.DataFrame([{'case:concept:name': cid, 'concept:name': a,
                           'time:timestamp': stamp + pd.Timedelta(seconds=index)}
                          for index, (cid, a) in enumerate(case['frame_rows'])],
                         columns=['case:concept:name', 'concept:name', 'time:timestamp'])
    record = {}
    for variant in ('default', 'TOKEN_REPLAY', 'BACKWARDS'):
        kwargs = {} if variant == 'default' else {'variant': getattr(tb.Variants, variant)}
        for form, data in (('log', log), ('frame', frame)):
            try:
                result = tb.apply(data, net, im, fm, **kwargs)
                value = {'rows': [row(r) for r in result]}
                if form == 'frame':
                    try:
                        value['diagnostics'] = records(tb.get_diagnostics_dataframe(data, result, **kwargs))
                    except Exception as error:
                        value['diagnostics'] = {'error': type(error).__name__}
            except Exception as error:
                value = {'error': type(error).__name__}
            record[f'{variant}/{form}'] = value
    out.append(record)
print(json.dumps(out))
'''


def capture(cases, seed):
    env = dict(os.environ, PYTHONHASHSEED=str(seed))
    result = subprocess.run([sys.executable, '-I', '-B', '-c', CHILD], input=json.dumps(cases),
                            capture_output=True, text=True, env=env)
    if result.returncode:
        raise SystemExit(result.stderr[-3000:])
    return json.loads(result.stdout.strip().splitlines()[-1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    import pm4py
    cases = generate()
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        runs = list(pool.map(lambda seed: capture(cases, seed), HASH_SEEDS))
    summary = {}
    for index, case in enumerate(cases):
        case['calls'] = {}
        for call in runs[0][index]:
            seen = []
            for run in runs:
                if run[index][call] not in seen:
                    seen.append(run[index][call])
            merged = seen[0] if len(seen) == 1 else {'observed': seen}
            case['calls'][call] = merged
            kind = next(iter(merged)) if 'observed' in merged or 'error' in merged else 'rows'
            summary[kind] = summary.get(kind, 0) + 1
    bank = {'reference': f'pm4py {pm4py.__version__}', 'bank_seed': BANK_SEED,
            'hash_seeds': list(HASH_SEEDS), 'cases': cases}
    data = json.dumps(bank, indent=0, sort_keys=True).encode('utf-8')
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0,
                                                      compresslevel=9, filename='') as stream:
        stream.write(data)
    print(f'{len(cases)} cases, calls by outcome {summary}')


if __name__ == '__main__':
    main()
