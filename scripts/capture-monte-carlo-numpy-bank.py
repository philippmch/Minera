"""Capture public Monte Carlo simulations under seeded numpy draws.

    TZ=UTC python scripts/capture-monte-carlo-numpy-bank.py \\
        --output docs/monte-carlo-numpy-bank.json.gz

Seeded nets (chains, choices, parallel branches, loops, silent and weighted
transitions, shared resources) with explicit stochastic maps over every public
random-variable family go through the public PETRI_SEMAPH_FIFO simulation after
numpy.random.seed. The reference hands its weighted choice the enabled
transitions in Python set order, which follows object identity, so every call
is repeated on freshly built nets and each distinct outcome is kept. Run it
under several PYTHONHASHSEED values (the default runs itself under 0-3).
"""
import argparse
from datetime import datetime, timedelta, timezone
import gzip
import io
import contextlib
import json
import os
from pathlib import Path
import random
import subprocess
import sys
import warnings

warnings.filterwarnings('ignore')

BANK_SEED = 20261035
REPEATS = 6
HASH_SEEDS = [0, 1, 2, 3]


def law(rng):
    # Locations stay above minus one scale, so redrawing negative service
    # times ends quickly.
    kind = rng.choice(['normal', 'normal', 'exponential', 'uniform', 'gamma', 'lognormal',
                       'deterministic', 'constant0', 'normal0'])
    if kind == 'normal':
        sigma = round(rng.uniform(0.1, 3), 2)
        return ['normal', round(rng.uniform(-sigma, 6), 2), sigma]
    if kind == 'normal0':
        return ['normal', round(rng.uniform(0, 6), 2), 0]
    scale = round(rng.uniform(0.5, 4), 2)
    loc = round(rng.uniform(-scale, 2), 2)
    if kind == 'exponential':
        return ['exponential', loc, scale]
    if kind == 'uniform':
        return ['uniform', loc, scale]
    if kind == 'gamma':
        return ['gamma', rng.choice([0.4, 0.8, 1.0, 2.5, 6.0]), loc / 2, scale]
    if kind == 'lognormal':
        return ['lognormal', round(rng.uniform(0.2, 1.2), 2), loc / 2, scale]
    if kind == 'deterministic':
        return ['deterministic', round(rng.uniform(0, 5), 2)]
    return ['constant0']


def topology(rng, shape):
    # (places, transitions [(name, label)], arcs [(source, target)], initial, final)
    labels = iter('ABCDEFGHIJKLMNOP')
    if shape == 'chain':
        n = rng.randint(1, 4)
        places = [f'p{i}' for i in range(n + 1)]
        transitions = [(f't{i}', next(labels)) for i in range(n)]
        arcs = [a for i in range(n) for a in [(places[i], f't{i}'), (f't{i}', places[i + 1])]]
        return places, transitions, arcs, {'p0': 1}, {places[-1]: 1}
    if shape == 'choice':
        n = rng.randint(2, 4)
        transitions = [(f't{i}', next(labels)) for i in range(n)]
        arcs = [a for i in range(n) for a in [('p0', f't{i}'), (f't{i}', 'p1')]]
        return ['p0', 'p1'], transitions, arcs, {'p0': 1}, {'p1': 1}
    if shape == 'two_choices':
        transitions = [('t0', 'A'), ('t1', 'B'), ('t2', 'C'), ('t3', 'D')]
        arcs = [('p0', 't0'), ('p0', 't1'), ('t0', 'p1'), ('t1', 'p1'),
                ('p1', 't2'), ('p1', 't3'), ('t2', 'p2'), ('t3', 'p2')]
        return ['p0', 'p1', 'p2'], transitions, arcs, {'p0': 1}, {'p2': 1}
    if shape == 'parallel':
        transitions = [('t0', None), ('t1', 'A'), ('t2', 'B'), ('t3', 'C')]
        arcs = [('p0', 't0'), ('t0', 'p1'), ('t0', 'p2'), ('p1', 't1'), ('t1', 'p3'),
                ('p2', 't2'), ('t2', 'p4'), ('p3', 't3'), ('p4', 't3'), ('t3', 'p5')]
        return [f'p{i}' for i in range(6)], transitions, arcs, {'p0': 1}, {'p5': 1}
    if shape == 'loop':
        transitions = [('t0', 'A'), ('t1', 'B'), ('t2', None)]
        arcs = [('p0', 't0'), ('t0', 'p1'), ('p1', 't1'), ('t1', 'p2'), ('p1', 't2'), ('t2', 'p0')]
        return ['p0', 'p1', 'p2'], transitions, arcs, {'p0': 1}, {'p2': 1}
    # skip: an optional activity through a silent bypass
    transitions = [('t0', 'A'), ('t1', 'B'), ('t2', None), ('t3', 'C')]
    arcs = [('p0', 't0'), ('t0', 'p1'), ('p1', 't1'), ('t1', 'p2'), ('p1', 't2'), ('t2', 'p2'),
            ('p2', 't3'), ('t3', 'p3')]
    return ['p0', 'p1', 'p2', 'p3'], transitions, arcs, {'p0': 1}, {'p3': 1}


def specs():
    rng = random.Random(BANK_SEED)
    shapes = ['chain', 'choice', 'two_choices', 'parallel', 'loop', 'skip']
    for index in range(150):
        shape = shapes[index % len(shapes)]
        places, transitions, arcs, initial, final = topology(rng, shape)
        weights = {}
        for name, _ in transitions:
            weights[name] = rng.choice([1, 1, 2, 3, 0.5, 5])
        if shape == 'loop':
            weights['t2'] = rng.choice([0.5, 1, 2])
            weights['t1'] = rng.choice([1, 2, 4])
        if index % 17 == 5:
            for name in weights:
                weights[name] = -weights[name]
        if index % 23 == 7:
            for name in weights:
                weights[name] = 0
        yield dict(name=f'mc-{index:03d}', shape=shape, places=places, transitions=transitions,
                   arcs=arcs, initial=initial, final=final,
                   laws={name: law(rng) for name, _ in transitions}, weights=weights,
                   seed=rng.choice([1, 7, 42, 2026]), simulations=rng.randint(1, 8),
                   arrival=rng.choice([0.5, 1, 3, 10, 50]), capacity=rng.choice([1, 1, 1, 2, 3]))
    # Source logs instead of maps: the reference fits weights and service
    # laws from the log (and the arrival ratio, when left out) before
    # simulating.
    for index in range(150, 240):
        shape = shapes[index % len(shapes)]
        places, transitions, arcs, initial, final = topology(rng, shape)
        spec = dict(name=f'mc-{index:03d}', shape=shape, places=places, transitions=transitions,
                    arcs=arcs, initial=initial, final=final,
                    seed=rng.choice([1, 7, 42, 2026]), simulations=rng.randint(1, 8),
                    capacity=rng.choice([1, 1, 2]))
        if index % 3:
            spec['arrival'] = rng.choice([0.5, 1, 3, 10, 50])
        spec['traces'] = [playout(spec, rng) for _ in range(rng.randint(1, 8))]
        yield spec


def playout(spec, rng):
    # A random firing sequence to the final marking, with visible events at
    # integer seconds; durations come from a few per-transition means.
    marking = dict(spec['initial'])
    inputs = {name: [s for s, t in spec['arcs'] if t == name] for name, _ in spec['transitions']}
    outputs = {name: [t for s, t in spec['arcs'] if s == name] for name, _ in spec['transitions']}
    events, second = [], rng.randint(0, 5)
    for _ in range(40):
        if all(marking.get(place, 0) == count for place, count in spec['final'].items()) and \
                sum(marking.values()) == sum(spec['final'].values()):
            break
        enabled = [(name, label) for name, label in spec['transitions']
                   if all(marking.get(place, 0) > 0 for place in inputs[name])]
        name, label = rng.choice(enabled)
        for place in inputs[name]:
            marking[place] -= 1
        for place in outputs[name]:
            marking[place] = marking.get(place, 0) + 1
        if label is not None:
            second += rng.choice([0, 1, 2, 3, 5, 8])
            events.append([label, second])
    return events


def random_variable(spec):
    from pm4py.objects.random_variables.normal.random_variable import Normal
    from pm4py.objects.random_variables.exponential.random_variable import Exponential
    from pm4py.objects.random_variables.uniform.random_variable import Uniform
    from pm4py.objects.random_variables.gamma.random_variable import Gamma
    from pm4py.objects.random_variables.lognormal.random_variable import LogNormal
    from pm4py.objects.random_variables.constant0.random_variable import Constant0
    from pm4py.objects.random_variables.deterministic.random_variable import Deterministic
    kind = spec[0]
    if kind == 'normal':
        return Normal(mu=spec[1], sigma=spec[2])
    if kind == 'exponential':
        return Exponential(loc=spec[1], scale=spec[2])
    if kind == 'uniform':
        return Uniform(loc=spec[1], scale=spec[2])
    if kind == 'gamma':
        return Gamma(a=spec[1], loc=spec[2], scale=spec[3])
    if kind == 'lognormal':
        return LogNormal(s=spec[1], loc=spec[2], scale=spec[3])
    if kind == 'deterministic':
        return Deterministic(value=spec[1])
    return Constant0()


def build(spec):
    from pm4py.objects.petri_net.obj import PetriNet, Marking
    from pm4py.objects.petri_net.utils.petri_utils import add_arc_from_to
    from pm4py.objects.log.obj import EventLog
    net = PetriNet(spec['name'])
    places = {name: PetriNet.Place(name) for name in spec['places']}
    transitions = {name: PetriNet.Transition(name, label) for name, label in spec['transitions']}
    net.places.update(places.values())
    net.transitions.update(transitions.values())
    nodes = dict(places, **transitions)
    for source, target in spec['arcs']:
        add_arc_from_to(nodes[source], nodes[target], net)
    initial = Marking({places[name]: count for name, count in spec['initial'].items()})
    final = Marking({places[name]: count for name, count in spec['final'].items()})
    if 'traces' in spec:
        from pm4py.objects.log.obj import Trace, Event
        log = EventLog()
        for index, events in enumerate(spec['traces']):
            trace = Trace(attributes={'concept:name': str(index)})
            for label, second in events:
                trace.append(Event({'concept:name': label, 'time:timestamp':
                                    datetime(2026, 1, 1, tzinfo=timezone.utc) + timedelta(seconds=second)}))
            log.append(trace)
        return net, initial, final, log, None
    smap = {}
    for name, transition in transitions.items():
        variable = random_variable(spec['laws'][name])
        variable.set_weight(spec['weights'][name])
        smap[transition] = variable
    return net, initial, final, EventLog(), smap


def outcome(spec):
    import numpy
    from pm4py.algo.simulation.montecarlo import algorithm
    numpy.random.seed(spec['seed'])
    net, initial, final, log, smap = build(spec)
    parameters = {'num_simulations': spec['simulations'],
                  'default_num_resources_per_place': spec['capacity'], 'show_progress_bar': False}
    if smap is not None:
        parameters['provided_stochastic_map'] = smap
    if 'arrival' in spec:
        parameters['case_arrival_ratio'] = spec['arrival']
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            simulated, result = algorithm.apply(log, net, initial, final, parameters=parameters)
    except Exception as error:
        return {'error': type(error).__name__}
    traces = [[[event['concept:name'], event['time:timestamp'].isoformat()] for event in trace]
              for trace in simulated]
    return {'traces': traces, 'cases_ex_time': [float(x) for x in result['cases_ex_time']],
            'median_cases_ex_time': float(result['median_cases_ex_time']),
            'total_cases_time': float(result['total_cases_time'])}


def child():
    rows = {}
    for spec in specs():
        seen = []
        for _ in range(REPEATS):
            observed = outcome(spec)
            if observed not in seen:
                seen.append(observed)
        rows[spec['name']] = seen
    json.dump(rows, sys.stdout)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--child', action='store_true')
    args = parser.parse_args()
    if args.child:
        child()
        return
    assert os.environ.get('TZ') == 'UTC', 'Run captures with TZ=UTC for reproducible public timestamps'
    import pm4py
    assert pm4py.__version__ == '2.7.22'
    observations = {}
    for hash_seed in HASH_SEEDS:
        output = subprocess.run([sys.executable, __file__, '--child'], check=True, capture_output=True,
                                env=dict(os.environ, PYTHONHASHSEED=str(hash_seed))).stdout
        for name, seen in json.loads(output).items():
            bucket = observations.setdefault(name, [])
            for observed in seen:
                if observed not in bucket:
                    bucket.append(observed)
    cases = [dict(spec, observed=observations[spec['name']]) for spec in specs()]
    document = dict(reference='2.7.22', bank_seed=BANK_SEED, hash_seeds=HASH_SEEDS, repeats=REPEATS,
                    environment=dict(TZ='UTC'), cases=cases)
    args.output.write_bytes(gzip.compress(json.dumps(document, sort_keys=True).encode(), mtime=0))
    print(len(cases), 'cases,', sum(len(c['observed']) for c in cases), 'outcomes')


if __name__ == '__main__':
    main()
