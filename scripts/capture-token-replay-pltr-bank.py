"""Public PM4Py 2.7.22 forward token replay PLTR/RETURN_NAMES capture; no native calls.

The 40 frozen nets of docs/reference-2.7.22-conformance-parity-nets.json.gz
get seeded logs (repeated variants, unknown labels, empty traces and exact
value-equal copies of a trace). Every event carries an `eid` attribute, so
the event contexts of the PLTR maps can be named. Each log is replayed
through the public `algo.conformance.tokenreplay.variants.token_replay.apply`
with ENABLE_PLTR_FITNESS (source traces or grouped variants), with
RETURN_NAMES, and with both. Rows, place fitness (trace key sets and m/r/c/p),
transition fitness and unknown-activity contexts are recorded by PNML names
and trace keys (the trace's concept:name), in fresh interpreters under sixteen
PYTHONHASHSEED values; a call whose output varies keeps every distinct one.

    python -I -B scripts/capture-token-replay-pltr-bank.py \
        --output docs/token-replay-pltr-bank.json.gz
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
CONFIGS = {
    'pltr': {'enable_pltr_fitness': True},
    'pltr_grouped': {'enable_pltr_fitness': True, 'disable_variants': False},
    'names': {'return_names': True},
    'names_pltr': {'return_names': True, 'enable_pltr_fitness': True},
}


def generate():
    rng = random.Random(BANK_SEED)
    corpus = json.loads(gzip.decompress(NETS.read_bytes()))['cases']
    cases = []
    for net in corpus:
        words = [list(word) for word in net['traces']]
        labels = sorted({a for word in words for a in word}) or ['a']
        for round_index in range(10):
            traces = []
            for index in range(rng.randint(3, 8)):
                if traces and rng.random() < 0.2:
                    traces.append(dict(rng.choice(traces)))  # exact copy: same key and eids
                    continue
                if traces and rng.random() < 0.2:
                    events = list(rng.choice(traces)['events'])
                elif rng.random() < 0.6 and words:
                    events = list(rng.choice(words))
                else:
                    events = [rng.choice(labels + ['zz']) for _ in range(rng.randint(0, 5))]
                key = f'k{index}'
                traces.append({'key': key, 'events': events,
                               'eids': [f'{key}.{position}' for position in range(len(events))]})
            cases.append({'name': f'{net["name"]}-{round_index}', 'pnml': net['model_pnml'],
                          'traces': traces})
    return cases


CHILD = r'''
import json, os, signal, sys, tempfile, warnings
warnings.filterwarnings('ignore')
from pm4py.objects.petri_net.importer import importer as pnml_importer
from pm4py.algo.conformance.tokenreplay.variants import token_replay as tr
from pm4py.objects.log.obj import EventLog, Trace, Event
P = tr.Parameters
NAMES = {'enable_pltr_fitness': P.ENABLE_PLTR_FITNESS, 'disable_variants': P.DISABLE_VARIANTS,
         'return_names': P.RETURN_NAMES}
CONFIGS = json.loads(sys.argv[2])
LIMIT = int(sys.argv[1])
class Timeout(BaseException):
    pass
def alarm(*_):
    raise Timeout()
signal.signal(signal.SIGALRM, alarm)
def name(node):
    return node if isinstance(node, str) else node.name
def marking(value):
    if isinstance(value, list):  # PLTR rows hold 'place:count' strings
        return sorted([item.rsplit(':', 1)[0], int(item.rsplit(':', 1)[1])] for item in value)
    return sorted([name(place), int(count)] for place, count in value.items())
def row(r):
    out = {'fit': r['trace_is_fit'], 'fitness': r['trace_fitness'],
           'activated': [name(t) for t in r['activated_transitions']],
           'problems': [name(t) for t in r['transitions_with_problems']],
           'enabled': sorted(name(t) for t in r['enabled_transitions_in_marking']),
           'reached': marking(r['reached_marking']),
           'counts': [r['missing_tokens'], r['consumed_tokens'], r['remaining_tokens'], r['produced_tokens']]}
    if 'activated_transitions_labels' in r:
        out['activated_labels'] = r['activated_transitions_labels']
        out['enabled_pairs'] = sorted([n, l] for n, l in zip(r['enabled_transitions_in_marking'],
                                                              r['enabled_transitions_in_marking_labels']))
    out['keys'] = sorted(r)
    return out
def key(trace):
    # Grouped replay keys activity-only variant traces without attributes.
    if 'concept:name' in trace.attributes:
        return trace.attributes['concept:name']
    return 'variant:' + ','.join(e['concept:name'] for e in trace)
def event_name(event):
    if 'eid' in event:
        return event['eid']
    # An unknown activity at the start of a trace has an empty dict context.
    return event['concept:name'] if 'concept:name' in event else '@@empty'
def contexts(mapping):
    # Transition maps hold event lists; the unknown-activity map holds one
    # event per trace.
    return sorted([key(t), [event_name(events)] if hasattr(events, 'get') else
                   [event_name(e) for e in events]] for t, events in mapping.items())
cases = json.load(sys.stdin)
out = []
directory = tempfile.mkdtemp()
for case in cases:
    path = os.path.join(directory, 'net.pnml')
    with open(path, 'w', encoding='utf-8') as stream:
        stream.write(case['pnml'])
    net, im, fm = pnml_importer.apply(path)
    record = {}
    for config, values in CONFIGS.items():
        log = EventLog([Trace([Event({'concept:name': a, 'eid': e}) for a, e in zip(t['events'], t['eids'])],
                              attributes={'concept:name': t['key']}) for t in case['traces']])
        parameters = {NAMES[k]: v for k, v in values.items()}
        signal.alarm(LIMIT)
        try:
            result = tr.apply(log, net, im, fm, parameters=parameters)
            if isinstance(result, tuple):
                rows, places, transitions, unknown = result
                value = {'rows': [row(r) for r in rows],
                         'places': sorted([name(p), sorted(key(t) for t in v['underfed_traces']),
                                           sorted(key(t) for t in v['overfed_traces']),
                                           [v['m'], v['r'], v['c'], v['p']]] for p, v in places.items()),
                         'transitions': sorted([name(t), contexts(v['underfed_traces']), contexts(v['fit_traces'])]
                                               for t, v in transitions.items()),
                         'unknown': sorted([a, contexts(v)] for a, v in unknown.items())}
            else:
                value = {'rows': [row(r) for r in result]}
        except Timeout:
            value = {'timeout': True}
        except Exception as error:
            if os.environ.get('PLTR_DEBUG'):
                raise
            value = {'error': type(error).__name__}
        finally:
            signal.alarm(0)
        record[config] = value
    out.append(record)
print(json.dumps(out))
'''


def capture(cases, seed, limit):
    env = dict(os.environ, PYTHONHASHSEED=str(seed))
    result = subprocess.run([sys.executable, '-I', '-B', '-c', CHILD, str(limit), json.dumps(CONFIGS)],
                            input=json.dumps(cases), capture_output=True, text=True, env=env)
    if result.returncode:
        raise SystemExit(result.stderr[-3000:])
    return json.loads(result.stdout.strip().splitlines()[-1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--limit', type=int, default=20)
    args = parser.parse_args()
    import pm4py
    cases = generate()
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        runs = list(pool.map(lambda seed: capture(cases, seed, args.limit), HASH_SEEDS))
    summary = {}
    for index, case in enumerate(cases):
        case['calls'] = {}
        for config in CONFIGS:
            seen = []
            for run in runs:
                if run[index][config] not in seen:
                    seen.append(run[index][config])
            if any('timeout' in value for value in seen):
                merged = {'timeout': True}
            elif len(seen) == 1:
                merged = seen[0]
            else:
                merged = {'observed': seen}
            case['calls'][config] = merged
            kind = next(iter(merged))
            summary[kind] = summary.get(kind, 0) + 1
    bank = {'reference': f'pm4py {pm4py.__version__}', 'bank_seed': BANK_SEED,
            'hash_seeds': list(HASH_SEEDS), 'configs': CONFIGS, 'cases': cases}
    data = json.dumps(bank, indent=0, sort_keys=True).encode('utf-8')
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0,
                                                      compresslevel=9, filename='') as stream:
        stream.write(data)
    print(f'{len(cases)} cases, calls by outcome {summary}')


if __name__ == '__main__':
    main()
