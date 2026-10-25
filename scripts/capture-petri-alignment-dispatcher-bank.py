"""Public PM4Py 2.7.22 Petri alignment log dispatcher capture; no native calls.

The 40 frozen nets of docs/reference-2.7.22-conformance-parity-nets.json.gz
(alpha, heuristics, ILP and duplicate-label nets) get seeded logs with
repeated variants, unknown labels and empty traces. Each log is aligned
through the public `algo.conformance.alignments.petri_net.algorithm.apply` on
the whole EventLog and on the equivalent DataFrame (case ids in shuffled,
unsorted order), under the four enum variants and the default, with default
costs or the frozen model/sync cost functions, a nondefault activity key, or
ENABLE_BEST_WORST_COST disabled. Each run is a fresh interpreter per
PYTHONHASHSEED; a call that runs past the time limit binds nothing. Per
trace the bank keeps None or the distinct observed (cost, bwc, fitness)
tuples, and the error name of a raising call.

    python -I -B scripts/capture-petri-alignment-dispatcher-bank.py \
        --output docs/petri-alignment-dispatcher-bank.json.gz
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
HASH_SEEDS = (0, 1, 2, 3)
NETS = Path(__file__).resolve().parent.parent / 'docs/reference-2.7.22-conformance-parity-nets.json.gz'
VARIANTS = ('default', 'VERSION_STATE_EQUATION_A_STAR', 'VERSION_DIJKSTRA_NO_HEURISTICS',
            'VERSION_DIJKSTRA_LESS_MEMORY', 'VERSION_DISCOUNTED_A_STAR')
MODES = ('plain', 'costed', 'activity_key', 'no_bwc')


def generate():
    rng = random.Random(BANK_SEED)
    corpus = json.loads(gzip.decompress(NETS.read_bytes()))['cases']
    cases = []
    for net in corpus:
        words = [list(word) if isinstance(word, str) else list(word) for word in net['traces']]
        labels = sorted({a for word in words for a in word}) or ['a']
        for mode in MODES:
            traces = []
            for _ in range(rng.randint(3, 8)):
                if traces and rng.random() < 0.3:
                    traces.append(list(rng.choice(traces)))
                elif rng.random() < 0.6 and words:
                    traces.append(list(rng.choice(words)))
                else:
                    alphabet = labels + ['zz']
                    traces.append([rng.choice(alphabet) for _ in range(rng.randint(0, 5))])
            case_ids = [f'c{rng.randint(0, 99)}' for _ in traces]
            while len(set(case_ids)) < len(case_ids):
                case_ids = [f'c{rng.randint(0, 99)}' for _ in traces]
            case = {'name': f'{net["name"]}-{mode}', 'pnml': net['model_pnml'], 'mode': mode,
                    'traces': traces, 'case_ids': case_ids}
            if mode == 'costed':
                case['model_costs'] = net['costs']['model']
                case['sync_costs'] = net['costs']['sync']
            cases.append(case)
    return cases


CHILD = r'''
import json, os, signal, sys, tempfile, warnings
warnings.filterwarnings('ignore')
import pandas as pd
from pm4py.objects.petri_net.importer import importer as pnml_importer
from pm4py.algo.conformance.alignments.petri_net import algorithm as pn
from pm4py.objects.log.obj import EventLog, Trace, Event
P = pn.Parameters
LIMIT = int(sys.argv[1])
class Timeout(BaseException):
    pass
def alarm(*_):
    raise Timeout()
signal.signal(signal.SIGALRM, alarm)
def rows(results):
    out = []
    for r in results:
        if r is None:
            out.append(None)
            continue
        out.append([r['cost'], r.get('bwc'), r.get('fitness'), 'bwc' in r, 'fitness' in r])
    return out
cases = json.load(sys.stdin)
out = []
directory = tempfile.mkdtemp()
for case in cases:
    path = os.path.join(directory, 'net.pnml')
    with open(path, 'w', encoding='utf-8') as stream:
        stream.write(case['pnml'])
    net, im, fm = pnml_importer.apply(path)
    by_name = {t.name: t for t in net.transitions}
    key = 'act' if case['mode'] == 'activity_key' else 'concept:name'
    log = EventLog([Trace([Event({'concept:name': 'noise', key: a} if key != 'concept:name'
                                 else {'concept:name': a}) for a in trace],
                          attributes={'concept:name': cid})
                    for trace, cid in zip(case['traces'], case['case_ids'])])
    frame_rows = []
    stamp = pd.Timestamp('2020-01-01')
    for trace, cid in zip(case['traces'], case['case_ids']):
        for a in trace:
            row = {'case:concept:name': cid, 'concept:name': a,
                   'time:timestamp': stamp + pd.Timedelta(seconds=len(frame_rows))}
            if key != 'concept:name':
                row['concept:name'] = 'noise'
                row[key] = a
            frame_rows.append(row)
    columns = ['case:concept:name', 'concept:name', 'time:timestamp'] + ([key] if key != 'concept:name' else [])
    frame = pd.DataFrame(frame_rows, columns=columns)
    record = {}
    for variant in case['variants']:
        for form, data in (('log', log), ('frame', frame)):
            parameters = {}
            if key != 'concept:name':
                parameters[P.ACTIVITY_KEY] = key
            if case['mode'] == 'no_bwc':
                parameters[P.ENABLE_BEST_WORST_COST] = False
            if case['mode'] == 'costed':
                parameters[P.PARAM_MODEL_COST_FUNCTION] = {by_name[n]: c for n, c in case['model_costs'].items()}
                parameters[P.PARAM_SYNC_COST_FUNCTION] = {by_name[n]: c for n, c in case['sync_costs'].items()}
            kwargs = {} if variant == 'default' else {'variant': getattr(pn.Variants, variant)}
            signal.alarm(LIMIT)
            try:
                value = {'rows': rows(pn.apply(data, net, im, fm, parameters=parameters, **kwargs))}
            except Timeout:
                value = {'timeout': True}
            except Exception as error:
                value = {'error': type(error).__name__}
            finally:
                signal.alarm(0)
            record[f'{variant}/{form}'] = value
    out.append(record)
print(json.dumps(out))
'''


def capture(cases, seed, limit):
    env = dict(os.environ, PYTHONHASHSEED=str(seed))
    result = subprocess.run([sys.executable, '-I', '-B', '-c', CHILD, str(limit)],
                            input=json.dumps(cases), capture_output=True, text=True, env=env,
                            check=True)
    return json.loads(result.stdout.strip().splitlines()[-1])


def merge(values):
    """Per call: error/timeout if any run raised or timed out the same way,
    otherwise per row None or the distinct observed tuples."""
    if any('timeout' in value for value in values):
        return {'timeout': True}
    errors = {value.get('error') for value in values}
    if errors != {None}:
        return {'error': sorted(e or 'none' for e in errors)}
    lengths = {len(value['rows']) for value in values}
    if len(lengths) != 1:
        return {'unstable': True}
    rows = []
    for index in range(lengths.pop()):
        seen = []
        for value in values:
            row = value['rows'][index]
            if row not in seen:
                seen.append(row)
        if None in seen and len(seen) > 1:
            return {'unstable': True}
        rows.append(None if seen == [None] else seen)
    return {'rows': rows}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--limit', type=int, default=20)
    args = parser.parse_args()
    import pm4py
    cases = generate()
    for case in cases:
        case['variants'] = list(VARIANTS)
    with concurrent.futures.ThreadPoolExecutor(len(HASH_SEEDS)) as pool:
        runs = list(pool.map(lambda seed: capture(cases, seed, args.limit), HASH_SEEDS))
    summary = {}
    for index, case in enumerate(cases):
        del case['variants']
        case['calls'] = {}
        for call in runs[0][index]:
            merged = merge([run[index][call] for run in runs])
            case['calls'][call] = merged
            kind = next(iter(merged))
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
