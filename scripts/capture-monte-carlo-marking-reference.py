"""Observe ordered marking keys through the pinned public Monte Carlo API."""
import argparse
import importlib.util
import json
import os
from pathlib import Path

spec = importlib.util.spec_from_file_location('scheduler',
    Path(__file__).with_name('capture-monte-carlo-scheduler-reference.py'))
scheduler = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scheduler)


def cases():
    initials = [
        ('positive', {'p0': 1}),
        ('zero_middle_first', {'p1': 0, 'p0': 1}),
        ('zero_sink_first', {'p2': 0, 'p0': 1}),
        ('zero_middle_last', {'p0': 1, 'p1': 0}),
        ('zero_sink_last', {'p0': 1, 'p2': 0}),
        ('both_zero_first', {'p2': 0, 'p1': 0, 'p0': 1}),
        ('middle_first', {'p1': 1, 'p0': 1}),
        ('source_first', {'p0': 1, 'p1': 1}),
    ]
    finals = [
        ('positive', {'p2': 1}),
        ('zero_source', {'p0': 0, 'p2': 1}),
        ('zero_middle', {'p1': 0, 'p2': 1}),
        ('empty', {}),
    ]
    for count in [1, 3]:
        for initial_name, initial in initials:
            for final_name, final in finals:
                case = scheduler.chain(f'{initial_name}_{final_name}_{count}',
                                       ['A', 'B'], [2, 3], count=count)
                case['initial'] = initial
                case['final'] = final
                # Match PNML document order to the input Marking's key order.
                case['places'] = list(initial) + [p for p in case['places'] if p not in initial]
                case['weights'] = {'t0': 1, 't1': 0}
                if not final:
                    # The supplied-model adapter infers absent PNML boundaries;
                    # an explicit all-zero override represents the empty final
                    # marking of this direct public call.
                    case['native_final_marking'] = [0] * len(case['places'])
                yield case
        for which in ['initial', 'final']:
            case = scheduler.chain(f'{which}_override_preserves_other_side_{count}',
                                   ['A', 'B'], [2, 3], count=count)
            case['places'] = ['p2', 'p0', 'p1']
            case['pnml_initial'] = {'p2': 0, 'p0': 1, 'p1': 1}
            case['pnml_final'] = {'p0': 0, 'p2': 1}
            case['initial'] = dict(case['pnml_initial'])
            case['final'] = dict(case['pnml_final'])
            case['weights'] = {'t0': 1, 't1': 0}
            if which == 'initial':
                case['initial'] = {'p0': 1, 'p1': 1}
                case['native_initial_marking'] = [0, 1, 1]
            else:
                case['final'] = {'p2': 1}
                case['native_final_marking'] = [1, 0, 0]
            yield case


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    assert os.environ.get('TZ') == 'UTC', 'Run captures with TZ=UTC'
    rows = []
    for case in cases():
        row = scheduler.capture(case)
        rows.append(row)
        print(case['name'], 'error' if 'error' in row else 'captured', flush=True)
    result = dict(reference='2.7.22', reference_commit='0715c31f3f1d85924107d6aa5329806db69df899',
                  public_api='pm4py.algo.simulation.montecarlo.algorithm.apply',
                  environment=dict(TZ='UTC'), observations=rows)
    from pm4py.objects.petri_net.obj import PetriNet, Marking
    places = {name: PetriNet.Place(name) for name in ['p0', 'p1', 'p2']}
    result['public_marking_comparisons'] = []
    for required in [{}, {'p2': 1}, {'p0': 0, 'p2': 1}, {'p1': 0, 'p2': 1}]:
        for reached in [{'p2': 1}, {'p2': 2}, {'p1': 1, 'p2': 1},
                        {'p0': 0, 'p2': 1}, {'p0': 1, 'p2': 1}]:
            left = Marking({places[name]: value for name, value in required.items()})
            right = Marking({places[name]: value for name, value in reached.items()})
            result['public_marking_comparisons'].append(dict(
                required=required, reached=reached, required_le_reached=left <= right))
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('Captured', len(rows), 'public marking outcomes')


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print(json.dumps(dict(error=type(error).__name__, message=str(error))))
        raise SystemExit(1)
