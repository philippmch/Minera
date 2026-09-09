"""Capture Petri element-usage comparison through the pinned public runtime."""
import argparse
import inspect
import itertools
import json
import tempfile
from pathlib import Path
from xml.sax.saxutils import quoteattr

import pm4py
from pm4py.algo.comparison.petrinet import element_usage_comparison as comparison
from pm4py.objects.log.obj import EventLog, Trace, Event
from pm4py.objects.petri_net.obj import PetriNet, Marking
from pm4py.objects.petri_net.utils.petri_utils import add_arc_from_to
from pm4py.objects.petri_net.exporter import exporter as pnml_exporter


def specification(places, transitions, arcs, initial, final):
    return dict(places=places, transitions=transitions, arcs=arcs, initial=initial, final=final)


def make_net(spec):
    net = PetriNet('element-usage')
    nodes = {name: PetriNet.Place(name) for name in spec['places']}
    net.places.update(nodes.values())
    for name, label in spec['transitions']:
        nodes[name] = PetriNet.Transition(name, label)
        net.transitions.add(nodes[name])
    for source, target, weight in spec['arcs']:
        add_arc_from_to(nodes[source], nodes[target], net, weight=weight)
    return net, Marking({nodes[key]: value for key, value in spec['initial'].items()}), Marking({nodes[key]: value for key, value in spec['final'].items()})


def log(events, activity_key='concept:name'):
    return EventLog([Trace([Event({} if activity is None else {activity_key: activity}) for activity in trace], attributes={'concept:name': str(index)})
                     for index, trace in enumerate(events)])


def xes(events, activity_key='concept:name'):
    return '<log>' + ''.join('<trace><string key="concept:name" value=' + quoteattr(str(index)) + '/>' +
                            ''.join('<event></event>' if activity is None else '<event><string key=' + quoteattr(activity_key) + ' value=' + quoteattr(activity) + '/></event>' for activity in trace) + '</trace>'
                            for index, trace in enumerate(events)) + '</log>'


def element_key(element):
    if isinstance(element, PetriNet.Place):
        return 'place:' + element.name
    if isinstance(element, PetriNet.Transition):
        return 'transition:' + element.name
    return 'arc:' + element.source.name + '->' + element.target.name


def cases():
    sequence = specification(['p0', 'p1'], [('a', 'A')], [('p0', 'a', 1), ('a', 'p1', 1)], {'p0': 1}, {'p1': 1})
    loop = specification(['p'], [('a', 'A')], [('p', 'a', 1), ('a', 'p', 1)], {'p': 1}, {'p': 1})
    chain = specification(['p0', 'p1', 'p2'], [('a', 'A'), ('b', 'B')], [('p0', 'a', 1), ('a', 'p1', 1), ('p1', 'b', 1), ('b', 'p2', 1)], {'p0': 1}, {'p2': 1})
    invisible = specification(['p0', 'p1', 'p2'], [('tau', None), ('a', 'A')], [('p0', 'tau', 1), ('tau', 'p1', 1), ('p1', 'a', 1), ('a', 'p2', 1)], {'p0': 1}, {'p2': 1})
    invisible_end = specification(['p0', 'p1', 'p2'], [('a', 'A'), ('tau', None)], [('p0', 'a', 1), ('a', 'p1', 1), ('p1', 'tau', 1), ('tau', 'p2', 1)], {'p0': 1}, {'p2': 1})
    weighted = specification(['p0', 'p1'], [('a', 'A')], [('p0', 'a', 2), ('a', 'p1', 3)], {'p0': 2}, {'p1': 3})
    isolated = specification(['p0', 'p1', 'unused'], [('a', 'A'), ('unused_t', 'Unused')], [('p0', 'a', 1), ('a', 'p1', 1)], {'p0': 1}, {'p1': 1})
    for left, right in itertools.product(range(17), repeat=2):
        yield dict(name=f'ratios_{left}_{right}', net=sequence, left=[['A']] * left, right=[['A']] * right, parameters={})
    trace_options = [[], [[]], [['A']], [['X']], [['A', 'A']], [['A', 'X', 'A']], [['A'], ['A']], [[], ['A']]]
    for net_name, net in [('sequence', sequence), ('loop', loop), ('invisible_start', invisible), ('invisible_end', invisible_end), ('weighted', weighted), ('isolated', isolated)]:
        for index, events in enumerate(trace_options):
            yield dict(name=f'{net_name}_{index}', net=net, left=events, right=[['A']], parameters={})
    for index, events in enumerate([[], [[]], [['A', 'B']], [['B', 'A']], [['A']], [['B']], [['X']], [['A', 'B', 'B']]]):
        yield dict(name=f'chain_{index}', net=chain, left=events, right=[['A', 'B']], parameters={})
    yield dict(name='custom_activity_key', net=sequence, left=[['A'], ['X']], right=[['A']], parameters={'pm4py:param:activity_key': 'step'}, activity_key='step')
    for parameter, values in [('stop_immediately_unfit', [True, False]), ('try_to_reach_final_marking_through_hidden', [True, False]), ('disable_variants', [True, False]), ('consider_remaining_in_fitness', [True, False])]:
        for value in values:
            yield dict(name=f'parameter_{parameter}_{value}', net=chain, left=[['B', 'A'], ['A', 'B']], right=[['A', 'B']], parameters={parameter: value})
    for net_name, net in [('hidden_start', invisible), ('hidden_end', invisible_end)]:
        for walk, finish, stop in itertools.product((False, True), repeat=3):
            yield dict(name=f'controls_{net_name}_{walk}_{finish}_{stop}', net=net,
                       left=[['X', 'A'], ['A', 'A']], right=[['A']],
                       parameters={'walk_through_hidden_trans': walk,
                                   'try_to_reach_final_marking_through_hidden': finish,
                                   'stop_immediately_unfit': stop})
    empty = specification([], [], [], {}, {})
    no_places = specification([], [('a', 'A')], [], {}, {})
    no_transitions = specification(['p0', 'p1'], [], [], {'p0': 1}, {'p1': 1})
    for name, net in [('empty_net', empty), ('no_places', no_places), ('no_transitions', no_transitions)]:
        for index, events in enumerate([[], [[]], [['A']]]):
            yield dict(name=f'{name}_{index}', net=net, left=events, right=[[]], parameters={})
    yield dict(name='missing_activity', net=sequence, left=[[None]], right=[['A']], parameters={})
    yield dict(name='missing_custom_activity', net=sequence, left=[[None]], right=[['A']],
               parameters={'pm4py:param:activity_key': 'step'}, activity_key='step')
    supplemental = {
        'sink_with_residual_cleanup': specification(
            ['p0', 'p1', 'p2'], [('a', 'A'), ('tau', None)],
            [('p0', 'a', 1), ('a', 'p1', 1), ('a', 'p2', 1),
             ('p1', 'tau', 1), ('p2', 'tau', 1), ('tau', 'p2', 1)],
            {'p0': 1}, {'p2': 1}),
        'zero_markings': specification(sequence['places'], sequence['transitions'], sequence['arcs'], {}, {}),
        'multiple_markings': specification(
            ['p0', 'p1', 'p2', 'p3'], [('a', 'A')],
            [('p0', 'a', 2), ('p1', 'a', 1), ('a', 'p2', 1), ('a', 'p3', 2)],
            {'p0': 3, 'p1': 2}, {'p2': 2, 'p3': 3}),
        'parallel_join': specification(
            ['p0', 'p1', 'p2', 'p3', 'p4', 'p5'], [('split', None), ('a', 'A'), ('b', 'B'), ('join', None)],
            [('p0', 'split', 1), ('split', 'p1', 1), ('split', 'p2', 1),
             ('p1', 'a', 1), ('a', 'p3', 1), ('p2', 'b', 1), ('b', 'p4', 1),
             ('p3', 'join', 1), ('p4', 'join', 1), ('join', 'p5', 1)],
            {'p0': 1}, {'p5': 1}),
        'duplicate_label_sequence': specification(
            ['p0', 'p1', 'p2'], [('a1', 'A'), ('a2', 'A')],
            [('p0', 'a1', 1), ('a1', 'p1', 1), ('p1', 'a2', 1), ('a2', 'p2', 1)],
            {'p0': 1}, {'p2': 1}),
        'source_free_transition': specification(['p'], [('a', 'A')], [('a', 'p', 1)], {}, {'p': 1}),
        'sink_free_transition': specification(['p'], [('a', 'A')], [('p', 'a', 1)], {'p': 1}, {}),
    }
    for name, net in supplemental.items():
        for index, events in enumerate([[], [[]], [['A']], [['A', 'A']], [['X']], [['A', 'B']], [['B', 'A']]]):
            yield dict(name=f'{name}_{index}', net=net, left=events, right=[['A']], parameters={})
    for tokens, weight, repeats in itertools.product(range(7), range(1, 5), range(5)):
        net = specification(['p0', 'p1'], [('a', 'A')],
                            [('p0', 'a', weight), ('a', 'p1', 1)],
                            {'p0': tokens}, {'p1': 1})
        yield dict(name=f'weighted_deficit_{tokens}_{weight}_{repeats}', net=net,
                   left=[['A'] * repeats], right=[['A']], parameters={})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    assert pm4py.__version__ == '2.7.22'
    observations = []
    with tempfile.TemporaryDirectory(prefix='minera-element-usage-') as directory:
        path = Path(directory) / 'model.pnml'
        for case in cases():
            net, initial, final = make_net(case['net'])
            activity_key = case.get('activity_key', 'concept:name')
            try:
                parameters = dict(case['parameters'], show_progress_bar=False)
                result = comparison.compare_element_usage_two_logs(net, initial, final, log(case['left'], activity_key), log(case['right'], activity_key), parameters)
                result = {element_key(key): value for key, value in result.items()}
            except Exception as error:
                result = {'error': type(error).__name__, 'message': str(error)}
            pnml_exporter.apply(net, initial, str(path), final_marking=final)
            observations.append(dict(case, result=result, pnml=path.read_text(encoding='utf-8'),
                                     left_xes=xes(case['left'], activity_key), right_xes=xes(case['right'], activity_key)))
    colors = [{'direction': value / 1000, 'dynamic': comparison.give_color_to_direction_dynamic(value / 1000),
               'static': comparison.give_color_to_direction_static(value / 1000)} for value in range(-1000, 1001)]
    args.output.write_text(json.dumps(dict(reference=pm4py.__version__, signature=str(inspect.signature(comparison.compare_element_usage_two_logs)),
                                         observations=observations, colors=colors), indent=2) + '\n', encoding='utf-8')
    print(f'Captured {len(observations)} element-usage comparisons and {len(colors)} color observations')


if __name__ == '__main__':
    main()
