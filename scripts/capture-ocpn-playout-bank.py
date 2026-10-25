"""Capture the public bank for algo.simulation.playout.ocpn (EXTENSIVE).

Seeded object-centric event logs (two or three object types, events relating
one to four objects) are mined with the public discover_oc_petri_net,
converted with pm4py.objects.ocpn.factory.create and played out with the
public algorithm.apply (EXTENSIVE) under MAX_BINDINGS_PER_ACTIVITY 1 and 2,
with RETURN_TRACES, with EXISTS_TRACE and with the default OCEL result
(plain and with OBJECTS_UNIQUE_PER_TRACE, whose per-trace object suffixes
follow the trace order and are folded back into per-trace event lists).
Runs happen in fresh
interpreters under four hash seeds; the trace order follows set iteration,
so traces are recorded as a sorted list of [label or null, sorted objects]
sequences, which must agree across seeds. The OCEL results are recorded by
their trace-order independent content: the multiset of (activity, objects)
events, the object table and the event identifiers and timestamps.

The bank is a gzip text file of tagged lines read by
minera_ocpn_playout_parity_tests:

  C name
  O id type                 an object of the input log
  E id activity minute ids  an event of the input log ("-" for no objects)
  R json                    {config: observed output} for the case
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
ROOT = Path(__file__).resolve().parent.parent
TYPES = ('order', 'item', 'box')
ACTIVITIES = ('a', 'b', 'c', 'd', 'e')


def ocel(rng):
    types = TYPES[:rng.randint(2, 3)]
    objects = [[f'{t[0]}{n}', t] for t in types for n in range(rng.randint(1, 3))]
    events, time = [], 0
    for n in range(rng.randint(2, 9)):
        time += rng.choice((0, 1, 2))
        related = rng.sample([o[0] for o in objects], min(len(objects), rng.choice((1, 1, 2, 2, 3, 4))))
        events.append([f'e{n:02d}', rng.choice(ACTIVITIES[:rng.randint(2, 5)]), time, related])
    return {'objects': objects, 'events': events}


def generate(count):
    rng = random.Random(BANK_SEED)
    return [dict(ocel(rng), name=f'case{index}') for index in range(count)]


CHILD = r'''
import json, signal, sys, warnings
warnings.filterwarnings('ignore')
import pandas as pd
import pm4py
from pm4py.objects.ocel.obj import OCEL
from pm4py.objects.ocpn import factory
from pm4py.algo.simulation.playout.ocpn import algorithm
from pm4py.algo.simulation.playout.ocpn.variants import extensive
P = extensive.Parameters
LIMIT = int(sys.argv[1])
class Timeout(BaseException):
    pass
def alarm(*_):
    raise Timeout()
signal.signal(signal.SIGALRM, alarm)
def build(case):
    types = dict(case['objects'])
    stamp = lambda m: pd.Timestamp('2024-01-01') + pd.Timedelta(minutes=m)
    events = pd.DataFrame({'ocel:eid': [e[0] for e in case['events']], 'ocel:activity': [e[1] for e in case['events']],
                           'ocel:timestamp': [stamp(e[2]) for e in case['events']]})
    rows = [(e[0], o, types[o], e[1], stamp(e[2])) for e in case['events'] for o in e[3]]
    relations = pd.DataFrame(rows, columns=['ocel:eid', 'ocel:oid', 'ocel:type', 'ocel:activity', 'ocel:timestamp'])
    objects = pd.DataFrame(case['objects'], columns=['ocel:oid', 'ocel:type'])
    return OCEL(events=events, objects=objects, relations=relations)
def ocel_content(log):
    if 'ocel:eid' not in log.events.columns:
        return {'events': [], 'event_columns': list(log.events.columns), 'objects': list(log.objects.columns)}
    related = {}
    for eid, oid in zip(log.relations['ocel:eid'], log.relations['ocel:oid']):
        related.setdefault(eid, []).append(oid)
    events = sorted([a, sorted(related.get(e, []))] for e, a in zip(log.events['ocel:eid'], log.events['ocel:activity']))
    return {'events': events,
            'ids': list(log.events['ocel:eid']),
            'timestamps': [str(t) for t in log.events['ocel:timestamp']],
            'objects': sorted([o, t] for o, t in zip(log.objects['ocel:oid'], log.objects['ocel:type'])),
            'relation_columns': list(log.relations.columns)}
def unique_content(log):
    # Objects are renamed "<id>_<trace>" with the trace's (set-order) index:
    # group the events by that suffix and drop it.
    if 'ocel:eid' not in log.events.columns:
        return ocel_content(log)
    related, traces = {}, {}
    for eid, oid in zip(log.relations['ocel:eid'], log.relations['ocel:oid']):
        related.setdefault(eid, []).append(oid)
    for eid, activity in zip(log.events['ocel:eid'], log.events['ocel:activity']):
        objects = related.get(eid, [])
        suffix = objects[0].rsplit('_', 1)[1] if objects else None
        traces.setdefault(suffix, []).append([activity, sorted(o.rsplit('_', 1)[0] for o in objects)])
    return {'traces': sorted(traces.values(), key=json.dumps), 'ids': list(log.events['ocel:eid']),
            'objects': len(log.objects)}
CONFIGS = {}
for bindings in (1, 2):
    CONFIGS[f'traces{bindings}'] = {P.MAX_BINDINGS_PER_ACTIVITY: bindings, P.RETURN_TRACES: True}
    CONFIGS[f'exists{bindings}'] = {P.MAX_BINDINGS_PER_ACTIVITY: bindings, P.EXISTS_TRACE: True}
    CONFIGS[f'ocel{bindings}'] = {P.MAX_BINDINGS_PER_ACTIVITY: bindings}
    CONFIGS[f'unique{bindings}'] = {P.MAX_BINDINGS_PER_ACTIVITY: bindings, P.OBJECTS_UNIQUE_PER_TRACE: True}
out = []
for case in json.load(sys.stdin):
    record = {}
    try:
        net = factory.create(pm4py.discover_oc_petri_net(build(case)))
    except Exception as error:
        out.append({'error': [type(error).__name__, str(error)]})
        continue
    for config, parameters in CONFIGS.items():
        signal.alarm(LIMIT)
        try:
            result = algorithm.apply(net, net.initial_marking, net.final_marking, parameters=dict(parameters))
            if isinstance(result, tuple):
                traces, transitions, objects = result
                value = {'traces': sorted(([[transitions[t].label, sorted(o)] for t, o in trace] for trace in traces),
                                          key=json.dumps),
                         'objects': sorted([o, t] for o, t in objects.items())}
            elif isinstance(result, bool):
                value = {'exists': result}
            elif config.startswith('unique'):
                value = unique_content(result)
            else:
                value = ocel_content(result)
        except Timeout:
            value = {'timeout': True}
        except Exception as error:
            value = {'error': [type(error).__name__, str(error)]}
        finally:
            signal.alarm(0)
        record[config] = value
    out.append(record)
print(json.dumps(out))
'''


def capture(cases, seed, limit):
    env = dict(os.environ, PYTHONHASHSEED=str(seed))
    result = subprocess.run([sys.executable, '-I', '-B', '-c', CHILD, str(limit)],
                            input=json.dumps(cases), capture_output=True, text=True, env=env)
    if result.returncode:
        raise SystemExit(result.stderr[-3000:])
    return json.loads(result.stdout.strip().splitlines()[-1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--count', type=int, default=200)
    parser.add_argument('--limit', type=int, default=10)
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/ocpn-playout-bank.txt.gz')
    args = parser.parse_args()
    cases = generate(args.count)
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        runs = list(pool.map(lambda seed: capture(cases, seed, args.limit), HASH_SEEDS))
    text, summary = [], {}
    for index, case in enumerate(cases):
        record = runs[0][index]
        if 'error' not in record:
            for config in list(record):
                seen = [run[index][config] for run in runs]
                if any('timeout' in value for value in seen):
                    record[config] = {'timeout': True}
                elif any(value != seen[0] for value in seen[1:]):
                    record[config] = {'varies': True}
        for config, value in record.items():
            if isinstance(value, dict):
                key = config + ':' + next(iter(value))
                summary[key] = summary.get(key, 0) + 1
        text.append(f'C {case["name"]}')
        text += [f'O {oid} {otype}' for oid, otype in case['objects']]
        text += [f'E {eid} {activity} {minute} ' + (' '.join(related) or '-')
                 for eid, activity, minute, related in case['events']]
        text.append('R ' + json.dumps(record, separators=(',', ':'), sort_keys=True))
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0, filename='') as stream:
        stream.write(('\n'.join(text) + '\n').encode())
    print(json.dumps(dict(sorted(summary.items()))))


if __name__ == '__main__':
    main()
