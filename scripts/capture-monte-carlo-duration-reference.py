"""Capture public raw Normal/Uniform draws and nonnegative simulation timestamps."""
import argparse
from datetime import datetime, timezone
import importlib.util
import json
import math
import os
from pathlib import Path
import random
import warnings
warnings.filterwarnings('ignore')
import numpy
import pm4py
from pm4py.algo.simulation.montecarlo import algorithm
from pm4py.objects.random_variables.normal.random_variable import Normal
from pm4py.objects.random_variables.uniform.random_variable import Uniform

spec = importlib.util.spec_from_file_location('fitting', Path(__file__).with_name('capture-stochastic-fit-reference.py'))
fitting = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fitting)
COUNT = 2048


def cases():
    for distribution, parameters in [('normal', [(-2, 1), (-1, 1), (-.1, 2), (0, 1), (2, 1), (0, 0), (3, 0)]),
                                     ('uniform', [(-2, 3), (-1, 2), (-.1, 3), (0, 1), (2, 4), (0, 0), (3, 0)])]:
        for location, spread in parameters:
            yield dict(name=f'{distribution}_{location}_{spread}', distribution=distribution,
                       location=location, spread=spread)


def variable(case):
    return (Normal(mu=case['location'], sigma=case['spread']) if case['distribution'] == 'normal'
            else Uniform(loc=case['location'], scale=case['spread']))


def capture(case):
    random.seed(1)
    numpy.random.seed(1)
    rv = variable(case)
    draws = [float(rv.get_value()) for _ in range(COUNT)]
    random.seed(1)
    numpy.random.seed(1)
    model = dict(name=case['name'], places=['source', 'sink'], transitions=[('a', 'A')],
                 arcs=[('source', 'a', 1), ('a', 'sink', 1)], initial={'source': 1}, final={'sink': 1}, traces=[])
    net, initial, final, log = fitting.make_case(model)
    rv = variable(case)
    rv.set_weight(1)
    simulated, _ = algorithm.apply(log, net, initial, final, parameters={
        'num_simulations': COUNT, 'case_arrival_ratio': 100, 'provided_stochastic_map': {next(iter(net.transitions)): rv},
        'enable_diagnostics': False, 'default_num_resources_per_place': 1, 'show_progress_bar': False})
    timestamps = []
    for trace in simulated:
        assert len(trace) == 1 and trace[0]['concept:name'] == 'A'
        timestamps.append(trace[0]['time:timestamp'].isoformat())
    assert len(timestamps) == COUNT
    durations = [datetime.fromisoformat(value).timestamp() - (1000000 + i * 100) for i, value in enumerate(timestamps)]
    assert all(math.isfinite(x) and x < 100 for x in draws)
    assert all(math.isfinite(x) and 0 <= x < 100 for x in durations)
    return dict(spec=case, direct_draws=draws, simulation_timestamps=timestamps,
                simulation_durations=durations)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    assert pm4py.__version__ == '2.7.22'
    assert os.environ.get('TZ') == 'UTC'
    assert datetime.fromtimestamp(1000000) == datetime.fromtimestamp(1000000, timezone.utc).replace(tzinfo=None)
    rows = []
    for case in cases():
        rows.append(capture(case))
        print(case['name'], 'captured', flush=True)
    document = dict(reference='2.7.22', reference_commit='0715c31f3f1d85924107d6aa5329806db69df899',
        public_apis=['Normal.get_value', 'Uniform.get_value', 'pm4py.algo.simulation.montecarlo.algorithm.apply'],
        environment=dict(TZ='UTC', numpy=numpy.__version__, numpy_seed=1, random_seed=1),
        sampling=dict(count=COUNT, start=1000000, arrival=100, native_seed=1,
                      one_sample_ks_limit=.065, two_sample_ks_limit=.09),
        limits=['Finite bounded laws only; no claim of matching cross-runtime RNG streams.',
                'Negative fixed laws and uniform laws with no nonnegative support are excluded because public calls may not terminate.',
                'Simulation timestamps have microsecond precision; all captured durations are below the arrival spacing.'],
        observations=rows)
    args.output.write_text(json.dumps(document, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print(json.dumps(dict(error=type(error).__name__, message=str(error))))
        raise SystemExit(1)
