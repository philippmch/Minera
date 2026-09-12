"""Pinned public stochastic-map fitting calls; no implementation inspection."""
import argparse
from datetime import datetime, timedelta, timezone
import json
from pathlib import Path
import warnings

warnings.filterwarnings('ignore')

import pm4py
from pm4py.algo.simulation.montecarlo.utils import replay
from pm4py.objects.log.obj import EventLog, Trace, Event
from pm4py.objects.petri_net.obj import PetriNet, Marking
from pm4py.objects.petri_net.utils.petri_utils import add_arc_from_to

assert pm4py.__version__ == '2.7.22'


def cases():
    places = ['source', 'middle', 'sink']
    transitions = [('a', 'A'), ('b', 'B')]
    arcs = [('source', 'a', 1), ('a', 'middle', 1),
            ('middle', 'b', 1), ('b', 'sink', 1)]
    traces = [[['A', 0], ['B', 2]], [['A', 10], ['B', 14]]]
    for name, initial, final, selected_arcs, selected_traces in [
        ('sequence', {'source': 1}, {'sink': 1}, arcs, traces),
        ('weighted', {'source': 2}, {'sink': 1}, [('source', 'a', 2), *arcs[1:]], traces),
        ('weighted_shortage', {'source': 1}, {'sink': 1}, [('source', 'a', 2), *arcs[1:]], traces),
        ('multimarked', {'source': 1, 'middle': 1}, {'middle': 1, 'sink': 1}, arcs, traces),
        ('zero_keys', {'source': 1, 'sink': 0}, {'source': 0, 'sink': 1}, arcs, traces),
        ('zero_boundaries', {'source': 0}, {'sink': 0}, arcs, traces),
        ('missing_tail', {'source': 1}, {'sink': 1}, arcs, [[['A', 0]]]),
        ('unknown_event', {'source': 1}, {'sink': 1}, arcs, [[['A', 0], ['unknown', 1], ['B', 2]]]),
    ]:
        yield dict(name=name, places=places, transitions=transitions, arcs=selected_arcs,
                   initial=initial, final=final, traces=selected_traces)
    yield dict(name='named_duplicates', places=places,
        transitions=[('z_first_A', 'A'), ('a_second_A', 'A'), ('b', 'B')],
        arcs=[('source', 'z_first_A', 1), ('z_first_A', 'middle', 1),
              ('source', 'a_second_A', 1), ('a_second_A', 'middle', 1),
              ('middle', 'b', 1), ('b', 'sink', 1)],
        initial={'source': 1}, final={'sink': 1}, traces=traces)


def make_case(spec):
    net = PetriNet(spec['name'])
    places = {name: PetriNet.Place(name) for name in spec['places']}
    transitions = {name: PetriNet.Transition(name, label)
                   for name, label in spec['transitions']}
    net.places.update(places.values())
    order = spec.get('transition_insertion_order', list(transitions))
    net.transitions.update(transitions[name] for name in order)
    nodes = dict(places, **transitions)
    for source, target, weight in spec['arcs']:
        add_arc_from_to(nodes[source], nodes[target], net, weight=weight)
    initial = Marking({places[name]: count for name, count in spec['initial'].items()})
    final = Marking({places[name]: count for name, count in spec['final'].items()})
    log = EventLog()
    for index, events in enumerate(spec['traces']):
        trace = Trace(attributes={'concept:name': str(index)})
        for label, second in events:
            trace.append(Event({'concept:name': label,
                'time:timestamp': datetime(2026, 1, 1, tzinfo=timezone.utc) + timedelta(seconds=second)}))
        log.append(trace)
    return net, initial, final, log


def capture(spec, with_statistics=False, force_distribution='EXPONENTIAL'):
    net, initial, final, log = make_case(spec)
    try:
        result = replay.get_map_from_log_and_net(log, net, initial, final,
            force_distribution=spec.get('force_distribution', force_distribution),
            parameters={'show_progress_bar': False})
        records = {}
        for transition, value in result.items():
            records[transition.name] = {
                'distribution': value.get_distribution_type(),
                'parameters': value.get_distribution_parameters(),
                'weight': value.get_weight(),
                'priority': value.get_priority(),
            }
        row = dict(spec=spec, records=records)
        if with_statistics:
            from pm4py.algo.conformance.tokenreplay import algorithm as token_replay
            from pm4py.objects.petri_net.utils import performance_map
            from pm4py.statistics.variants.log import get as variants
            aligned = token_replay.apply(log, net, initial, final,
                parameters={'show_progress_bar': False})
            variants_idx = variants.get_variants_from_log_trace_idx(log)
            statistics = performance_map.single_element_statistics(
                log, net, initial, aligned, variants_idx)
            row['statistics'] = {transition.name: value for transition, value in statistics.items()
                                 if isinstance(transition, PetriNet.Transition)}
            row['aligned_traces'] = [dict(activated=[t.name for t in item['activated_transitions']],
                fit=item['trace_is_fit'], reached={p.name: v for p, v in item['reached_marking'].items()})
                for item in aligned]
        return row
    except Exception as error:
        return dict(spec=spec, error={'type': type(error).__name__, 'message': str(error)})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--statistics', action='store_true')
    parser.add_argument('--specs', type=Path,
        help='Replay the input specs from a saved observation document')
    args = parser.parse_args()
    selected = [dict(spec=spec) for spec in cases()]
    if args.specs:
        document = json.loads(args.specs.read_text(encoding='utf-8'))
        selected = document['observations']
    observations = []
    for source in selected:
        row = capture(source['spec'], args.statistics)
        if source.get('alternatives'):
            row['alternatives'] = [capture(alternative['spec'], args.statistics)
                                   for alternative in source['alternatives']]
        observations.append(row)
    result = dict(reference='2.7.22', reference_commit='0715c31f3f1d85924107d6aa5329806db69df899',
        public_api='pm4py.algo.simulation.montecarlo.utils.replay.get_map_from_log_and_net',
        forced_distribution='EXPONENTIAL', observations=observations)
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    for row in observations:
        print(json.dumps(dict(name=row['spec']['name'], **{k: v for k, v in row.items() if k != 'spec'})))


if __name__ == '__main__':
    main()
