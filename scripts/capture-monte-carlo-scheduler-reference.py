"""Capture pinned public Monte Carlo scheduler outputs without source inspection."""
import argparse
from datetime import datetime, timezone
import importlib.util
import json
import os
from pathlib import Path
import random
import warnings

warnings.filterwarnings('ignore')

import numpy
import pm4py
from pm4py.algo.simulation.montecarlo import algorithm
from pm4py.objects.random_variables.normal.random_variable import Normal

assert pm4py.__version__ == '2.7.22'

spec = importlib.util.spec_from_file_location('fitting',
    Path(__file__).with_name('capture-stochastic-fit-reference.py'))
fitting = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fitting)


def chain(name, labels, durations, count=3, arrival=1, capacity=1, **extra):
    places = [f'p{i}' for i in range(len(labels) + 1)]
    transitions = [(f't{i}', label) for i, label in enumerate(labels)]
    arcs = [arc for i in range(len(labels))
            for arc in [(places[i], f't{i}', 1), (f't{i}', places[i + 1], 1)]]
    return dict(name=name, places=places, transitions=transitions, arcs=arcs,
                initial={places[0]: 1}, final={places[-1]: 1}, traces=[],
                durations=dict(zip([t[0] for t in transitions], durations)),
                simulations=count, arrival=arrival, capacity=capacity, **extra)


def cases():
    for count in [1, 3]:
        yield chain(f'zero_default_capacity_{count}', ['A', 'B'], [2, 3], count=count, capacity=0)
    for count in [1, 2, 3, 4]:
        for arrival in [0, 1, 10]:
            for capacity in [1, 2]:
                yield chain(f'sequence_{count}_{arrival}_{capacity}', ['A', 'B'], [2, 3],
                            count, arrival, capacity)
    for labels, durations, name in [
        (['A'], [2], 'single'),
        (['A', 'B', 'C'], [2, 3, 5], 'triple'),
        (['A', 'B', 'C', 'D'], [1, 7, 2, 3], 'four'),
        ([None, 'A', 'B'], [2, 3, 5], 'hidden_start'),
        (['A', None, 'B'], [2, 3, 5], 'hidden_middle'),
        (['A', 'B', None], [2, 3, 5], 'hidden_end'),
        (['A', 'B'], [0, 2], 'zero_first'),
        (['A', 'B'], [2, 0], 'zero_last'),
        (['A', 'B'], [0, 0], 'all_immediate'),
        (['A', 'B'], [0.0001, 0.0002], 'microseconds'),
    ]:
        for count in [1, 3]:
            yield chain(f'{name}_{count}', labels, durations, count=count)
    for place, resources in [('p0', 2), ('p1', 2), ('p2', 2), ('p1', 0)]:
        for key_type in ['object', 'name']:
            yield chain(f'resource_{place}_{resources}_{key_type}', ['A', 'B'], [2, 3],
                        resources={place: resources}, resource_key_type=key_type)
    for diagnostics in [True, False]:
        yield chain(f'diagnostics_{diagnostics}', ['A', 'B'], [2, 3],
                    diagnostics=diagnostics)
    for count in [1, 3]:
        for final_tokens in [1, 2]:
            case = chain(f'multiple_tokens_{count}_{final_tokens}', ['A', 'B'], [2, 3], count=count)
            case['initial'] = {'p0': 2}
            case['final'] = {'p2': final_tokens}
            case['weights'] = {'t0': 0, 't1': 1}
            yield case
        for capacity in [1, 2]:
            case = chain(f'weighted_arcs_{count}_{capacity}', ['A', 'B'], [2, 3],
                         count=count, capacity=capacity)
            case['initial'] = {'p0': 2}
            case['arcs'] = [('p0', 't0', 2), ('t0', 'p1', 2),
                            ('p1', 't1', 2), ('t1', 'p2', 1)]
            yield case
        yield dict(name=f'parallel_join_{count}',
            places=['left', 'right', 'left_done', 'right_done', 'sink'],
            transitions=[('a', 'A'), ('b', 'B'), ('join', 'C')],
            arcs=[('left', 'a', 1), ('a', 'left_done', 1),
                  ('right', 'b', 1), ('b', 'right_done', 1),
                  ('left_done', 'join', 1), ('right_done', 'join', 1), ('join', 'sink', 1)],
            initial={'left': 1, 'right': 1}, final={'sink': 1}, traces=[],
            durations={'a': 2, 'b': 3, 'join': 5}, weights={'a': 1, 'b': 0, 'join': 1},
            simulations=count, arrival=1, capacity=1)
    for count in [0, 1, 3]:
        yield chain(f'empty_visible_{count}', [None], [2], count=count)
    yield chain('zero_cases', ['A', 'B'], [2, 3], count=0)


def intervals(tree):
    # The public result exposes a tree with public root/left/right/interval data.
    pending = [tree.root]
    records = []
    while pending:
        node = pending.pop()
        if node is not None:
            value = node.interval
            records.append(dict(start=value.start, end=value.end, data=value.data))
            pending.extend([node.left, node.right])
    return sorted(records, key=lambda x: (x['start'], x['end'], json.dumps(x['data'], sort_keys=True)))


def capture(case):
    random.seed(1)
    numpy.random.seed(1)
    net, initial, final, log = fitting.make_case(case)
    smap = {}
    for transition in net.transitions:
        rv = Normal(mu=case['durations'][transition.name], sigma=0)
        rv.set_weight(case.get('weights', {}).get(transition.name, 1))
        smap[transition] = rv
    parameters = {'num_simulations': case['simulations'],
        'case_arrival_ratio': case['arrival'], 'provided_stochastic_map': smap,
        'enable_diagnostics': case.get('diagnostics', False),
        'default_num_resources_per_place': case['capacity'], 'show_progress_bar': False}
    if 'resources' in case:
        places = {p.name: p for p in net.places}
        parameters['map_resources_per_place'] = {
            places[name] if case['resource_key_type'] == 'object' else name: value
            for name, value in case['resources'].items()}
    try:
        simulated, result = algorithm.apply(log, net, initial, final, parameters=parameters)
        result = dict(result)
        for key in ['output_places_interval_trees', 'output_transitions_interval_trees']:
            result[key] = {str(getattr(node, 'name', node)): intervals(tree)
                           for node, tree in result[key].items()}
        traces = [dict(attributes=dict(trace.attributes), events=[{
            key: value.isoformat() if isinstance(value, datetime) else value
            for key, value in event.items()} for event in trace]) for trace in simulated]
        return dict(spec=case, log=traces, result=result)
    except Exception as error:
        return dict(spec=case, error=dict(type=type(error).__name__, message=str(error)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    assert os.environ.get('TZ') == 'UTC', 'Run captures with TZ=UTC for reproducible public timestamps'
    assert datetime.fromtimestamp(1000000) == datetime.fromtimestamp(
        1000000, timezone.utc).replace(tzinfo=None), 'The capture runtime must use UTC local timestamps'
    rows = []
    for case in cases():
        row = capture(case)
        rows.append(row)
        print(case['name'], 'error' if 'error' in row else 'captured', flush=True)
    document = dict(reference='2.7.22', reference_commit='0715c31f3f1d85924107d6aa5329806db69df899',
        public_api='pm4py.algo.simulation.montecarlo.algorithm.apply',
        dependencies=dict(numpy=numpy.__version__), environment=dict(TZ='UTC'), observations=rows)
    args.output.write_text(json.dumps(document, indent=2) + '\n', encoding='utf-8')
    print('Captured', len(rows), 'public scheduler outcomes')


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print(json.dumps(dict(error=type(error).__name__, message=str(error))))
        raise SystemExit(1)
