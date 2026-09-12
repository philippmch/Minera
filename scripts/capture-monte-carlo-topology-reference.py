"""Capture pinned public Monte Carlo source, cycle, and terminal behavior."""
import argparse
from datetime import datetime, timezone
import importlib.util
import json
import os
from pathlib import Path

spec = importlib.util.spec_from_file_location('scheduler',
    Path(__file__).with_name('capture-monte-carlo-scheduler-reference.py'))
scheduler = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scheduler)


def cases():
    for count in [1, 3]:
        for capacity in [0, 1, 2]:
            for arrival in [0, 1, 10]:
                for loops in [1, 2, 4]:
                    yield dict(name=f'fuel_cycle_{loops}_{count}_{capacity}_{arrival}',
                        places=['source', 'fuel', 'middle', 'sink'],
                        transitions=[('a', 'A'), ('b', 'B')],
                        arcs=[('source', 'a', 1), ('fuel', 'a', 1), ('a', 'middle', 1),
                              ('middle', 'b', 1), ('b', 'source', 1), ('b', 'sink', 1)],
                        initial={'source': 1, 'fuel': loops}, final={'sink': loops}, traces=[],
                        durations={'a': 2, 'b': 3}, simulations=count,
                        arrival=arrival, capacity=capacity)
                for loops in [1, 3]:
                    yield dict(name=f'source_generator_{loops}_{count}_{capacity}_{arrival}',
                        places=['source', 'middle', 'sink'],
                        transitions=[('a', 'A'), ('b', 'B')],
                        arcs=[('a', 'middle', 1), ('middle', 'b', 1), ('b', 'sink', 1)],
                        initial={'source': 1}, final={'sink': loops}, traces=[],
                        durations={'a': 2, 'b': 3}, weights={'a': 0, 'b': 1},
                        simulations=count, arrival=arrival, capacity=capacity)
                for loops in [1, 3]:
                    for duration in [0, 2]:
                        yield dict(name=f'self_loop_{loops}_{duration}_{count}_{capacity}_{arrival}',
                            places=['source', 'sink'], transitions=[('a', 'A')],
                            arcs=[('source', 'a', 1), ('a', 'source', 1), ('a', 'sink', 1)],
                            initial={'source': 1}, final={'sink': loops}, traces=[],
                            durations={'a': duration}, simulations=count,
                            arrival=arrival, capacity=capacity)
            for name, initial in [('positive', {'source': 1}),
                                  ('zero_key', {'source': 0}), ('empty', {})]:
                case = dict(name=f'no_input_{name}_{count}_{capacity}',
                    places=['source', 'sink'], transitions=[('a', 'A')],
                    arcs=[('a', 'sink', 1)], initial=initial, final={'sink': 1}, traces=[],
                    durations={'a': 2}, simulations=count, arrival=1, capacity=capacity)
                if not initial:
                    # The explicit native override represents an absent initial
                    # marking without triggering the PNML adapter's inference.
                    case['native_initial_marking'] = [0, 0]
                yield case
            yield dict(name=f'no_output_{count}_{capacity}', places=['source', 'sink'],
                transitions=[('a', 'A')], arcs=[('source', 'a', 1)],
                initial={'source': 1}, final={'sink': 1}, traces=[], durations={'a': 2},
                simulations=count, arrival=1, capacity=capacity)
        yield dict(name=f'initial_is_final_{count}', places=['source', 'sink'],
            transitions=[('a', 'A')], arcs=[('source', 'a', 1), ('a', 'sink', 1)],
            initial={'source': 1}, final={'source': 1}, traces=[], durations={'a': 2},
            simulations=count, arrival=1, capacity=1)
    for count in [1, 3]:
        for arrival in [1, 10]:
            for duration in [0, 1, 7]:
                for left, right in [(0, 1), (1, 2), (2, 1)]:
                    for reverse in [False, True]:
                        output_order = ['right', 'left'] if reverse else ['left', 'right']
                        yield dict(name=f'multi_output_{count}_{arrival}_{duration}_{left}_{right}_{reverse}',
                            places=['source', 'left', 'right', 'sink'],
                            transitions=[('a', 'A'), ('b', 'B')],
                            arcs=[('source', 'a', 1), ('a', 'left', 1), ('a', 'right', 1),
                                  ('left', 'b', 1), ('right', 'b', 1)] +
                                 [('b', p, 1) for p in output_order] + [('b', 'sink', 1)],
                            initial={'source': 1}, final={'sink': 3}, traces=[],
                            durations={'a': 2, 'b': duration}, simulations=count,
                            arrival=arrival, capacity=1, resource_key_type='object',
                            resources={'left': left, 'right': right})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    assert os.environ.get('TZ') == 'UTC', 'Run captures with TZ=UTC'
    assert datetime.fromtimestamp(1000000) == datetime.fromtimestamp(
        1000000, timezone.utc).replace(tzinfo=None), 'The capture runtime must use UTC'
    rows = []
    for case in cases():
        row = scheduler.capture(case)
        rows.append(row)
        print(case['name'], 'error' if 'error' in row else 'captured', flush=True)
    document = dict(reference='2.7.22',
        reference_commit='0715c31f3f1d85924107d6aa5329806db69df899',
        public_api='pm4py.algo.simulation.montecarlo.algorithm.apply',
        dependencies=dict(numpy=scheduler.numpy.__version__), environment=dict(TZ='UTC'),
        observations=rows)
    args.output.write_text(json.dumps(document, indent=2) + '\n', encoding='utf-8')
    print('Captured', len(rows), 'public topology outcomes')


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print(json.dumps(dict(error=type(error).__name__, message=str(error))))
        raise SystemExit(1)
