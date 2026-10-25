"""Public PM4Py 2.7.22 token-replay capture over shuffled PNML documents.

Forward token replay walks silent paths breadth-first from each marked place
and stops at the first equally short path it finds, so ties between silent
transitions are decided by the order in which a place's outgoing arcs are
visited. This bank separates the three orders a document carries (place,
transition and arc element order, plus element names) by shuffling each one
independently, and records the public replay of every trace. No native calls.

Two families:
  * tie: one marked place with k parallel silent transitions to the place a
    visible X needs; each silent transition also marks its own witness place.
  * random: small random nets with visible and silent transitions, random
    traces (including labels missing from the model).

All nets are replayed in one fresh interpreter per PYTHONHASHSEED and per
allocation padding (objects created before each import, which moves the model's
object addresses); only rows equal under every run are kept, the rest are
counted as unstable. A replay
over 2 seconds or 3 GiB is recorded as unresolved. POSIX only.

    python -I -B scripts/capture-token-replay-arc-order-bank.py \
        --output docs/token-replay-arc-order-bank.json.gz
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
HASH_SEEDS = (0, 1, 2, 3)
# Objects allocated before each import, which shifts the model's object
# addresses and so the iteration order of identity-hashed arc sets.
PADDINGS = (0, 7, 33)


def pnml(places, transitions, arcs, initial, final):
    out = ['<?xml version="1.0" encoding="UTF-8"?>\n<pnml><net id="net" '
           'type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel">'
           '<name><text>net</text></name><page id="page">']
    for place in places:
        marking = (f'<initialMarking><text>{initial[place]}</text></initialMarking>'
                   if place in initial else '')
        out.append(f'<place id="{place}"><name><text>{place}</text></name>{marking}</place>')
    for name, label in transitions:
        if label is None:
            out.append(f'<transition id="{name}"><name><text>{name}</text></name>'
                       f'<toolspecific tool="ProM" version="6.4" activity="$invisible$" '
                       f'localNodeID="{name}"/></transition>')
        else:
            out.append(f'<transition id="{name}"><name><text>{label}</text></name></transition>')
    for index, (source, target) in enumerate(arcs):
        out.append(f'<arc id="arc{index}" source="{source}" target="{target}"/>')
    out.append('</page><finalmarkings><marking>')
    for place, tokens in final.items():
        out.append(f'<place idref="{place}"><text>{tokens}</text></place>')
    out.append('</marking></finalmarkings></net></pnml>\n')
    return ''.join(out)


def silent_names(rng, count):
    pool = ['h%02d' % i for i in range(40)] + ['tau', 'z', 'a1', 'm', 'q9', 'skip', 'hid_8', 'hid_9']
    return rng.sample(pool, count)


def tie_case(rng):
    k = rng.randint(2, 5)
    names = silent_names(rng, k)
    places = ['p0', 'T', 'end'] + ['w_' + n for n in names]
    transitions = [(n, None) for n in names] + [('X', 'X')]
    arcs = []
    for n in names:
        arcs += [('p0', n), (n, 'T'), (n, 'w_' + n)]
    arcs += [('T', 'X'), ('X', 'end')]
    if rng.random() < 0.5:
        # A second layer: the tie is decided again at an intermediate place.
        arcs = [arc for arc in arcs if arc[1] != 'T' or arc[0] == names[0]]
        places.append('mid')
        arcs = [(s, 'mid' if t == 'T' else t) for s, t in arcs]
        second = silent_names(rng, 2)
        second = [s + '_b' for s in second]
        for s in second:
            transitions.append((s, None))
            arcs += [('mid', s), (s, 'T'), (s, 'w_' + s)]
            places.append('w_' + s)
        for n in names[1:]:
            arcs.append((n, 'mid'))
    rng.shuffle(places)
    rng.shuffle(transitions)
    rng.shuffle(arcs)
    traces = [['X'], ['X', 'X'], []]
    return pnml(places, transitions, arcs, {'p0': 1}, {'end': 1}), traces


def random_case(rng):
    # Places are ranked; silent transitions only move tokens to higher ranks,
    # so silent routing cannot cycle. Visible transitions may loop back.
    labels = ['A', 'B', 'C', 'D', 'E', 'F'][:rng.randint(3, 6)]
    ranked = ['src'] + ['p%d' % i for i in range(rng.randint(3, 7))] + ['sink']
    silent = silent_names(rng, rng.randint(2, 6))
    transitions = [(label.lower() + '_t', label) for label in labels] + [(n, None) for n in silent]
    arcs = set()
    for name, label in transitions:
        rank = rng.randrange(0, len(ranked) - 1)
        inputs = {ranked[rank]}
        if rng.random() < 0.25 and rank > 0:
            inputs.add(ranked[rng.randrange(0, rank)])
        later = ranked[rank + 1:]
        targets = later if label is None else ranked[1:]
        outputs = set(rng.sample(targets, min(len(targets), rng.choice((1, 1, 2)))))
        arcs.update((place, name) for place in inputs)
        arcs.update((name, place) for place in outputs)
    arcs = sorted(arcs)
    places = list(ranked)
    rng.shuffle(places)
    rng.shuffle(transitions)
    rng.shuffle(arcs)
    traces = []
    for _ in range(rng.randint(3, 6)):
        pool = labels + ['UNKNOWN'] if rng.random() < 0.1 else labels
        traces.append([rng.choice(pool) for _ in range(rng.randint(0, 6))])
    return pnml(places, transitions, arcs, {'src': 1}, {'sink': 1}), traces


def generate(count):
    rng = random.Random(BANK_SEED)
    cases = []
    for index in range(count):
        family = 'tie' if index % 2 == 0 else 'random'
        document, traces = tie_case(rng) if family == 'tie' else random_case(rng)
        cases.append({'name': f'{family}_{index:04d}', 'family': family,
                      'pnml': document, 'traces': traces})
    return cases


CHILD = r'''
import json, sys, tempfile, os, signal, resource
resource.setrlimit(resource.RLIMIT_AS, (3 << 30, 3 << 30))
class Timeout(Exception):
    pass
def alarm(*_):
    raise Timeout()
signal.signal(signal.SIGALRM, alarm)
from pm4py.objects.petri_net.importer import importer as pnml_importer
from pm4py.algo.conformance.tokenreplay import algorithm as token_replay
from pm4py.objects.log.obj import EventLog, Trace, Event
request = json.load(sys.stdin)
cases, padding = request['cases'], request['padding']
out = []
for case in cases:
    handle, path = tempfile.mkstemp(suffix='.pnml')
    with os.fdopen(handle, 'w', encoding='utf-8') as stream:
        stream.write(case['pnml'])
    try:
        pad = [object() for _ in range(padding)]
        net, im, fm = pnml_importer.apply(path)
        rows = []
        for trace in case['traces']:
            log = EventLog([Trace([Event({'concept:name': a}) for a in trace],
                                  attributes={'concept:name': '0'})])
            try:
                signal.alarm(2)
                try:
                    r = token_replay.apply(log, net, im, fm)[0]
                finally:
                    signal.alarm(0)
                rows.append({'activated': [t.name for t in r['activated_transitions']],
                             'missing': r['missing_tokens'], 'consumed': r['consumed_tokens'],
                             'remaining': r['remaining_tokens'], 'produced': r['produced_tokens'],
                             'fit': bool(r['trace_is_fit'])})
            except Timeout:
                rows.append({'unresolved': 'timeout'})
            except MemoryError:
                rows.append({'unresolved': 'memory'})
            except Exception as error:
                rows.append({'error': type(error).__name__})
        out.append(rows)
    except Exception as error:
        out.append({'import_error': type(error).__name__})
    finally:
        os.unlink(path)
print(json.dumps(out))
'''


def capture(cases, seed, padding):
    env = dict(os.environ, PYTHONHASHSEED=str(seed))
    request = json.dumps({'cases': cases, 'padding': padding})
    result = subprocess.run([sys.executable, '-I', '-B', '-c', CHILD], input=request,
                            capture_output=True, text=True, env=env, check=True)
    return json.loads(result.stdout.strip().splitlines()[-1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--count', type=int, default=600)
    args = parser.parse_args()
    import pm4py
    cases = generate(args.count)
    runs = [capture(cases, seed, padding) for seed in HASH_SEEDS for padding in PADDINGS]
    unstable = 0
    unresolved = 0
    for index, case in enumerate(cases):
        observed = [run[index] for run in runs]
        if isinstance(observed[0], dict):
            case['import_error'] = observed[0]['import_error']
            continue
        rows = []
        for row_index in range(len(case['traces'])):
            values = [run[row_index] for run in observed]
            if any('unresolved' in value for value in values):
                rows.append({'unresolved': True})
                unresolved += 1
            elif all(value == values[0] for value in values):
                rows.append(values[0])
            else:
                rows.append({'unstable': True})
                unstable += 1
        case['replay'] = rows
    bank = {'reference': f'pm4py {pm4py.__version__}', 'bank_seed': BANK_SEED,
            'hash_seeds': list(HASH_SEEDS), 'paddings': list(PADDINGS), 'unstable_rows': unstable, 'unresolved_rows': unresolved, 'cases': cases}
    data = json.dumps(bank, indent=0, sort_keys=True).encode('utf-8')
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0,
                                                      compresslevel=9, filename='') as stream:
        stream.write(data)
    rows = sum(len(case.get('replay', [])) for case in cases)
    print(f'{len(cases)} nets, {rows} rows, {unstable} unstable, {unresolved} unresolved')


if __name__ == '__main__':
    main()
