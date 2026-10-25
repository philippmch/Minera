"""Public PM4Py 2.7.22 object-centric Petri net and SAW net capture; no native
calls.

Seeded OCELs (as in docs/ocel-graphs-bank.json.gz: two to four object types,
objects without events, shared and repeated objects, timestamp ties, event
tables not in time order, events without objects in a quarter of the logs) go
through the public `algo.discovery.ocel.ocpn.algorithm.apply` (defaults, the
IMd inductive variant, double-arc threshold 0.5, token-based-replay
diagnostics, custom activity/object-type selector columns, the WO_ANNOTATION
selector) and the internal `ocpn.variants.wo_annotation.apply` and the public
`algo.discovery.ocel.saw_nets.algorithm.apply` (defaults and custom
selectors). OCPN results are recorded through their OCDFG fields, double-arc
map, per-type Petri nets (names, labels, arcs, markings) and replay
diagnostics; SAW results through per-type and merged nets with every arc's
weight distribution. Sets and dicts are recorded tagged and sorted. Every case
runs in fresh interpreters under four PYTHONHASHSEED values and keeps every
distinct outcome.

    python -I -B scripts/capture-ocel-nets-bank.py \
        --output docs/ocel-nets-bank.json.gz
"""
import argparse
import concurrent.futures
import gzip
import json
import math
import os
from pathlib import Path
import random
import subprocess
import sys

BANK_SEED = 20261026
HASH_SEEDS = (0, 1, 2, 3)
TYPES = ('order', 'item', 'pack', 'cust')
OCPN_CONFIGS = {
    'default': {},
    'imd': {'inductive_miner_variant': 'imd'},
    'double_arc_low': {'double_arc_threshold': 0.5},
    'tbr': {'diagnostics_with_token_based_replay': True},
    'selectors': {'param:event:activity': 'label', 'param:object:type': 'kind'},
    # The WO_ANNOTATION selector of the dispatcher, and the internal
    # wo_annotation variant module called directly.
    'wo_annotation': {},
    'module_wo_annotation': {},
}
SAW_CONFIGS = {
    'default': {},
    'selectors': {'param:event:activity': 'label', 'param:object:type': 'kind'},
}


def ocel(rng, types, activities):
    objects = []
    for type_name in types:
        for n in range(rng.randint(1, 4)):
            objects.append([f'{type_name[0]}{n}', type_name])
    events = []
    time = 0
    # Events without objects only in a quarter of the logs.
    sizes = (0, 1, 1, 2, 2, 3, 4) if rng.random() < 0.25 else (1, 1, 2, 2, 3, 4)
    for n in range(rng.randint(3, 20)):
        time += rng.choice((0, 0, 1, 2, 5))
        related = rng.sample([o[0] for o in objects], min(len(objects), rng.choice(sizes)))
        events.append([f'e{n:02d}', rng.choice(activities), time, related])
    if rng.random() < 0.15:
        rng.shuffle(events)
    return {'objects': objects, 'events': events}


def generate():
    rng = random.Random(BANK_SEED)
    cases = []
    for index in range(120):
        types = list(TYPES[:rng.randint(2, 4)])
        activities = [chr(ord('a') + n) for n in range(rng.randint(2, 6))]
        cases.append({'name': f'ocpn-{index:03d}', 'real': ocel(rng, types, activities)})
    return cases


def number(value):
    value = float(value)
    return value if math.isfinite(value) else repr(value)


def ordered(value):
    """Sets and dicts tagged and in canonical order; tuples as lists."""
    if isinstance(value, dict):
        return {'dict': sorted([ordered(k), ordered(v)] for k, v in value.items())}
    if isinstance(value, (set, frozenset)):
        return {'set': sorted(ordered(v) for v in value)}
    if isinstance(value, (list, tuple)):
        return [ordered(v) for v in value]
    if isinstance(value, float):
        return number(value)
    if hasattr(value, 'item'):
        return ordered(value.item())
    return value


def name(node):
    return node.name


def petri(net, im=None, fm=None, weights=False):
    # The merged SAW net copies every per-type net, so node names repeat; a
    # repeated name gets a '#k' suffix to keep the nodes apart.
    names, seen = {}, {}
    for node in sorted(list(net.places) + list(net.transitions), key=lambda node: node.name):
        k = seen[node.name] = seen.get(node.name, -1) + 1
        names[id(node)] = node.name if k == 0 else f'{node.name}#{k}'
    label = lambda node: names[id(node)]
    out = {'places': sorted(label(p) for p in net.places),
           'transitions': sorted([label(t), t.label] for t in net.transitions),
           'arcs': sorted([label(a.source), label(a.target)] +
                          ([sorted([int(k), int(v)] for k, v in a.weight.items())] if weights else [a.weight])
                          for a in net.arcs)}
    if im is not None:
        out['initial'] = sorted([label(p), c] for p, c in im.items())
        out['final'] = sorted([label(p), c] for p, c in fm.items())
    return out


def ocpn_value(result):
    out = {k: ordered(v) for k, v in result.items() if k not in ('petri_nets', 'tbr_results')}
    out['petri_nets'] = {ot: petri(*net) for ot, net in result['petri_nets'].items()}
    out['tbr_results'] = {ot: {'places': sorted([name(p), v['m'], v['r'], v['c'], v['p']] for p, v in places.items()),
                               'transitions': sorted([name(t), int(n)] for t, n in fired.items())}
                          for ot, (places, fired) in result['tbr_results'].items()}
    return out


def saw_value(result):
    return {'ot_saw_nets': {ot: petri(net, weights=True) for ot, net in result['ot_saw_nets'].items()},
            'multi_saw_net': petri(result['multi_saw_net'], weights=True)}


def run(cases):
    import contextlib
    import io
    import warnings
    warnings.filterwarnings('ignore')
    import pandas as pd
    from pm4py.objects.ocel.obj import OCEL
    from pm4py.algo.discovery.ocel.ocpn import algorithm as ocpn
    from pm4py.algo.discovery.ocel.saw_nets import algorithm as saw
    from pm4py.algo.discovery.ocel.ocpn.variants import wo_annotation
    calls = {'wo_annotation': lambda log, parameters: ocpn.apply(log, variant=ocpn.Variants.WO_ANNOTATION,
                                                                 parameters=parameters),
             'module_wo_annotation': wo_annotation.apply}
    origin = pd.Timestamp('2024-01-01')

    def build(spec, labelled=False):
        events = pd.DataFrame({'ocel:eid': [e[0] for e in spec['events']],
                               'ocel:activity': [e[1] for e in spec['events']],
                               'ocel:timestamp': [origin + pd.Timedelta(minutes=e[2]) for e in spec['events']]})
        objects = pd.DataFrame({'ocel:oid': [o[0] for o in spec['objects']],
                                'ocel:type': [o[1] for o in spec['objects']]})
        types = dict(spec['objects'])
        rows = [{'ocel:eid': e[0], 'ocel:activity': e[1], 'ocel:timestamp': origin + pd.Timedelta(minutes=e[2]),
                 'ocel:oid': o, 'ocel:type': types[o], 'ocel:qualifier': None}
                for e in spec['events'] for o in e[3]]
        relations = pd.DataFrame(rows, columns=['ocel:eid', 'ocel:activity', 'ocel:timestamp', 'ocel:oid',
                                                'ocel:type', 'ocel:qualifier'])
        if labelled:
            # Custom selector columns: the activity and type spelled upper-case
            # (merged activities where two labels share one spelling).
            events['label'] = events['ocel:activity'].str.upper().replace({'B': 'A'})
            objects['kind'] = objects['ocel:type'].str.upper()
            relations['label'] = relations['ocel:activity'].str.upper().replace({'B': 'A'})
            relations['kind'] = relations['ocel:type'].str.upper()
        return OCEL(events=events, objects=objects, relations=relations)

    out = []
    for case in cases:
        record = {}
        for family, function, configs, value in (('ocpn', ocpn.apply, OCPN_CONFIGS, ocpn_value),
                                                 ('saw', saw.apply, SAW_CONFIGS, saw_value)):
            for config, parameters in configs.items():
                try:
                    with contextlib.redirect_stderr(io.StringIO()):
                        result = calls.get(config, function)(build(case['real'], labelled=config == 'selectors'),
                                                             parameters=dict(parameters))
                    record[f'{family}/{config}'] = {'value': value(result)}
                except Exception as error:
                    record[f'{family}/{config}'] = {'error': type(error).__name__}
        out.append(record)
    return out


def capture(seed):
    env = dict(os.environ, PYTHONHASHSEED=str(seed))
    result = subprocess.run([sys.executable, '-I', '-B', os.path.abspath(__file__), '--child'],
                            capture_output=True, text=True, env=env)
    if result.returncode:
        raise SystemExit(result.stderr[-3000:])
    return json.loads(result.stdout.strip().splitlines()[-1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path)
    parser.add_argument('--child', action='store_true')
    args = parser.parse_args()
    if args.child:
        print(json.dumps(run(generate())))
        return
    import pm4py
    cases = generate()
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        runs = list(pool.map(capture, HASH_SEEDS))
    summary = {}
    for index, case in enumerate(cases):
        case['calls'] = {}
        for call in runs[0][index]:
            seen = []
            for run_result in runs:
                if run_result[index][call] not in seen:
                    seen.append(run_result[index][call])
            merged = seen[0] if len(seen) == 1 else {'observed': seen}
            case['calls'][call] = merged
            kind = 'observed' if len(seen) > 1 else 'error' if 'error' in merged else 'value'
            summary[kind] = summary.get(kind, 0) + 1
    bank = {'reference': f'pm4py {pm4py.__version__}', 'bank_seed': BANK_SEED, 'hash_seeds': list(HASH_SEEDS),
            'ocpn_configs': OCPN_CONFIGS, 'saw_configs': SAW_CONFIGS, 'cases': cases}
    data = json.dumps(bank, indent=0, sort_keys=True).encode('utf-8')
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0,
                                                      compresslevel=9, filename='') as stream:
        stream.write(data)
    print(f'{len(cases)} cases, calls by outcome {summary}')


if __name__ == '__main__':
    main()
