"""Capture public raw shifted LogNormal/Gamma draws and nonnegative simulation timestamps."""
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
from pm4py.objects.random_variables.lognormal.random_variable import LogNormal
from pm4py.objects.random_variables.gamma.random_variable import Gamma
from scipy import stats
import scipy

spec = importlib.util.spec_from_file_location('fitting', Path(__file__).with_name('capture-stochastic-fit-reference.py'))
fitting = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fitting)
COUNT = 2048


def cases():
    matrices = {
        'lognormal': [(.5,-1,2),(1,-2,1),(2,-.1,.5),(.5,0,2),(1,0,1),(2,0,.5),(.5,2,1),(1,0,0),(1,2,0)],
        'gamma': [(.1,-.01,1),(.5,-.2,1),(1,-2,1),(2,-2,1),(5,-10,2),(.5,0,1),(2,0,1),(5,0,2),(2,3,1),(2,0,0),(2,3,0)]}
    for distribution, values in matrices.items():
        for shape, location, scale in values:
            yield dict(name=f'{distribution}_{shape}_{location}_{scale}', distribution=distribution,
                       shape=shape, location=location, scale=scale)


def variable(case):
    cls = LogNormal if case['distribution'] == 'lognormal' else Gamma
    return cls(case['shape'], case['location'], case['scale'])


def cdf_grid(case):
    if case['scale'] == 0:
        return []
    law = stats.lognorm if case['distribution'] == 'lognormal' else stats.gamma
    points = sorted(set([0.0, case['location']] + [case['location'] + case['scale'] * x
        for x in [1e-10,1e-8,1e-6,1e-4,.001,.01,.1,.5,1,2,5,10,20,50,100,1000]]))
    return [dict(x=x, cdf=float(law.cdf(x, case['shape'], loc=case['location'], scale=case['scale'])),
                 sf=float(law.sf(x, case['shape'], loc=case['location'], scale=case['scale']))) for x in points]


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
        'num_simulations': COUNT, 'case_arrival_ratio': 100000, 'provided_stochastic_map': {next(iter(net.transitions)): rv},
        'enable_diagnostics': False, 'default_num_resources_per_place': 1, 'show_progress_bar': False})
    timestamps = []
    for trace in simulated:
        assert len(trace) == 1 and trace[0]['concept:name'] == 'A'
        timestamps.append(trace[0]['time:timestamp'].isoformat())
    assert len(timestamps) == COUNT
    durations = [datetime.fromisoformat(value).timestamp() - (1000000 + i * 100000) for i, value in enumerate(timestamps)]
    assert all(math.isfinite(x) and x < 100000 for x in draws)
    assert all(math.isfinite(x) and 0 <= x < 100000 for x in durations)
    return dict(spec=case, direct_draws=draws, simulation_timestamps=timestamps,
                simulation_durations=durations, scipy_cdf_grid=cdf_grid(case))


def capture_invalid():
    rows = []
    for family in ['lognormal', 'gamma']:
        for shape, location, scale in [(0,0,1),(-1,0,1),(0,0,0),(1,0,-1)]:
            case = dict(name=f'{family}_invalid_{shape}_{location}_{scale}', distribution=family,
                        shape=shape, location=location, scale=scale)
            row = dict(spec=case)
            for route in ['direct', 'simulation']:
                try:
                    if route == 'direct':
                        variable(case).get_value()
                    else:
                        model = dict(name='invalid', places=['source','sink'], transitions=[('a','A')],
                            arcs=[('source','a',1),('a','sink',1)], initial={'source':1}, final={'sink':1}, traces=[])
                        net, initial, final, log = fitting.make_case(model)
                        rv = variable(case)
                        rv.set_weight(1)
                        algorithm.apply(log,net,initial,final,parameters={'num_simulations':1,
                            'provided_stochastic_map':{next(iter(net.transitions)):rv}, 'show_progress_bar':False})
                    raise AssertionError('Invalid law unexpectedly succeeded')
                except AssertionError:
                    raise
                except Exception as error:
                    row[route + '_error'] = dict(type=type(error).__name__, message=str(error))
            rows.append(row)
    return rows


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
        public_apis=['LogNormal.get_value', 'Gamma.get_value', 'scipy.stats.lognorm.cdf/sf', 'scipy.stats.gamma.cdf/sf', 'pm4py.algo.simulation.montecarlo.algorithm.apply'],
        environment=dict(TZ='UTC', numpy=numpy.__version__, scipy=scipy.__version__, numpy_seed=1, random_seed=1),
        sampling=dict(count=COUNT, start=1000000, arrival=100000, timestamp_rounding_tolerance=0.000001, native_seed=1,
                      one_sample_ks_limit=.065, two_sample_ks_limit=.09),
        limits=['Finite bounded laws only; no claim of matching cross-runtime RNG streams.',
                'Negative-location zero-scale laws are excluded because public simulation calls may not terminate.',
                'Simulation timestamps have microsecond precision; all captured durations are below the arrival spacing.',
                'One-sample simulation KS uses the minimum discrepancy compatible with +/-1 microsecond rounding intervals.',
                'Shifted laws are checked through direct vectors; maps are tested only for zero location, with Gamma(scale,shape) or LogNormal(log(scale),shape). LogNormal zero-scale has no finite map representation.'],
        observations=rows, invalid_observations=capture_invalid())
    args.output.write_text(json.dumps(document, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print(json.dumps(dict(error=type(error).__name__, message=str(error))))
        raise SystemExit(1)
