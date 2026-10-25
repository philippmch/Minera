"""Capture the public bank for algo.simulation.playout.oc_causal_net (EXTENSIVE).

Seeded object-centric causal nets go through the public
pm4py.objects.oc_causal_net.creation.factory.create_oc_causal_net: dependency
arcs over one or two object types (START_<type> and END_<type> included) with
one or two marker groups per activity side, cardinalities 0..2 or unbounded
and shared or unique marker keys; nets the factory rejects are skipped. (Nets
converted from mined OCPNs name their activities differently under every
hash seed, so they are left out.)

Each net is played out with the public extensive apply under one to three
objects per type and MAX_BINDINGS_PER_ACTIVITY 1 and 2: RETURN_SEQUENCES
(sorted canonical binding sequences), the default OCEL and
OBJECTS_UNIQUE_PER_SEQUENCE (trace-order independent content), in fresh
interpreters under four hash seeds, which must agree.

The bank is a gzip text file of tagged lines read by
minera_oc_causal_net_playout_parity_tests:

  C name
  N json   the marker groups {activity: {"img": [...], "omg": [...]}} with
           markers [activity, type, [min, max or -1], key]
  B json   the objects {type: [ids]}
  R json   {config: output}
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


def random_net(r):
    types = ['o', 'i'][:r.randint(1, 2)]
    acts = ['a', 'b', 'c'][:r.randint(1, 3)]
    arcs = set()
    for t in types:
        part = [a for a in acts if r.random() < 0.7] or [acts[0]]
        for a in part:
            if r.random() < 0.6:
                arcs.add((f'START_{t}', a, t))
            if r.random() < 0.6:
                arcs.add((a, f'END_{t}', t))
            for b in part:
                if r.random() < 0.35:
                    arcs.add((a, b, t))
        if r.random() < 0.15:
            arcs.add((f'START_{t}', f'END_{t}', t))
        if not any(x == f'START_{t}' for x, y, u in arcs):
            arcs.add((f'START_{t}', r.choice(part), t))
        if not any(y == f'END_{t}' for x, y, u in arcs):
            arcs.add((r.choice(part), f'END_{t}', t))
    activities = sorted({x for arc in arcs for x in arc[:2]} | {f'START_{t}' for t in types} |
                        {f'END_{t}' for t in types})

    def marker(rel, t):
        low = r.choice([0, 1, 1, 1, 2])
        high = r.choice([-1, -1, 1, 2])
        if high != -1 and high < low:
            high = low
        return [rel, t, [low, high], r.choice([0, 0, 0, 1, 2])]

    groups = {a: {'img': [], 'omg': []} for a in activities}
    for a in activities:
        for side, mine in (('omg', sorted((y, t) for x, y, t in arcs if x == a)),
                           ('img', sorted((x, t) for x, y, t in arcs if y == a))):
            for _ in range(r.randint(1, 2) if mine else 0):
                chosen = [m for m in mine if r.random() < 0.7] or [r.choice(mine)]
                groups[a][side].append([marker(rel, t) for rel, t in chosen])
    objects = {t: [f'{t}{n}' for n in range(r.randint(1, 3 if len(types) == 1 else 2))] for t in types}
    return groups, objects


def generate(count):
    r = random.Random(BANK_SEED)
    cases = []
    for index in range(count):
        groups, objects = random_net(r)
        cases.append({'name': f'random{index}', 'groups': groups, 'objects': objects})
    return cases


CHILD = r'''
import json, signal, sys, warnings
warnings.filterwarnings('ignore')
from pm4py.objects.oc_causal_net.creation import factory
from pm4py.algo.simulation.playout.oc_causal_net.variants import extensive
P = extensive.Parameters
LIMIT = int(sys.argv[1])
class Timeout(BaseException):
    pass
def alarm(*_):
    raise Timeout()
signal.signal(signal.SIGALRM, alarm)
def canon(side, ia, io):
    if side is None:
        return None
    return sorted([ia[k], io[t], sorted(objs)] for k, v in side for t, objs in v if objs)
def content(log, unique):
    if 'ocel:eid' not in log.events.columns:
        return {'events': []}
    related = {}
    for eid, oid in zip(log.relations['ocel:eid'], log.relations['ocel:oid']):
        related.setdefault(eid, []).append(oid)
    if unique:
        traces = {}
        for eid, activity in zip(log.events['ocel:eid'], log.events['ocel:activity']):
            objects = related.get(eid, [])
            suffix = objects[0].rsplit('_', 1)[1] if objects else None
            traces.setdefault(suffix, []).append([activity, sorted(o.rsplit('_', 1)[0] for o in objects)])
        return {'traces': sorted(traces.values(), key=json.dumps), 'ids': list(log.events['ocel:eid']),
                'objects': len(log.objects)}
    return {'events': sorted([a, sorted(related.get(e, []))] for e, a in zip(log.events['ocel:eid'], log.events['ocel:activity'])),
            'ids': list(log.events['ocel:eid']), 'timestamps': [str(t) for t in log.events['ocel:timestamp']],
            'objects': sorted([o, t] for o, t in zip(log.objects['ocel:oid'], log.objects['ocel:type']))}
out = []
for case in json.load(sys.stdin):
    try:
        groups, objects = case['groups'], case['objects']
        net = factory.create_oc_causal_net(json.loads(json.dumps(groups)))
    except Exception as error:
        out.append({'skip': [type(error).__name__, str(error)]})
        continue
    record = {}
    for bindings in (1, 2):
        for config, extra in (('sequences', {P.RETURN_SEQUENCES: True}), ('ocel', {}),
                              ('unique', {P.OBJECTS_UNIQUE_PER_SEQUENCE: True})):
            parameters = dict(extra)
            parameters[P.MAX_BINDINGS_PER_ACTIVITY] = bindings
            signal.alarm(LIMIT)
            try:
                result = extensive.apply(net, {t: set(v) for t, v in objects.items()}, parameters=parameters)
                if config == 'sequences':
                    sequences, ia, io = result
                    value = {'sequences': sorted([[ia[b[0]], canon(b[1], ia, io), canon(b[2], ia, io)] for b in s]
                                                 for s in sequences)}
                else:
                    value = content(result, config == 'unique')
            except Timeout:
                value = {'timeout': True}
            except Exception as error:
                value = {'error': [type(error).__name__, str(error)]}
            finally:
                signal.alarm(0)
            record[f'{config}{bindings}'] = value
    out.append({'groups': groups, 'objects': objects, 'record': record})
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
    parser.add_argument('--count', type=int, default=800)
    parser.add_argument('--limit', type=int, default=5)
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/oc-causal-net-playout-bank.txt.gz')
    args = parser.parse_args()
    cases = generate(args.count)
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        runs = list(pool.map(lambda seed: capture(cases, seed, args.limit), HASH_SEEDS))
    text, summary = [], {}
    for index, case in enumerate(cases):
        first = runs[0][index]
        if 'skip' in first:
            summary['skipped'] = summary.get('skipped', 0) + 1
            continue
        if any(json.dumps(run[index]['groups'], sort_keys=True) != json.dumps(first['groups'], sort_keys=True)
               for run in runs):
            summary['net varies'] = summary.get('net varies', 0) + 1
            continue
        record = first['record']
        for config in list(record):
            seen = [run[index]['record'][config] for run in runs]
            if any('timeout' in value for value in seen):
                record[config] = {'timeout': True}
            elif any(value != seen[0] for value in seen[1:]):
                record[config] = {'varies': True}
            key = config + ':' + next(iter(record[config]))
            summary[key] = summary.get(key, 0) + 1
        text += [f'C {case["name"]}', 'N ' + json.dumps(first['groups'], separators=(',', ':'), sort_keys=True),
                 'B ' + json.dumps(first['objects'], separators=(',', ':'), sort_keys=True),
                 'R ' + json.dumps(record, separators=(',', ':'), sort_keys=True)]
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0, filename='') as stream:
        stream.write(('\n'.join(text) + '\n').encode())
    print(json.dumps(dict(sorted(summary.items()))))


if __name__ == '__main__':
    main()
