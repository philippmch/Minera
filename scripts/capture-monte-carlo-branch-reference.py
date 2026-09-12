"""Capture weighted public branch choices without inspecting reference implementation."""
import argparse
from collections import Counter
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
spec = importlib.util.spec_from_file_location('fitting', Path(__file__).with_name('capture-stochastic-fit-reference.py'))
fitting = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fitting)
COUNT = 2048


def cases():
    matrices = [([1,3], 'positive'), ([0,0], 'zero'), ([-1,-3], 'negative'),
                ([-1,0], 'negative_forced'), ([0,1], 'positive_forced'), ([-1,1], 'cancel'),
                ([1,2,3], 'three_positive'), ([0,0,0], 'three_zero'),
                ([-1,-2,-3], 'three_negative'), ([-1,-1,2], 'three_cancel'),
                ([1e-320,3e-320], 'subnormal'), ([-1e-320,-3e-320], 'negative_subnormal')]
    for weights, name in matrices:
        yield dict(name=name, stages=[weights], seed=1)
    for weights, name in [([-1e-320,1e308],'negative_underflows_to_zero'),
                          ([1e-320,-1e308],'positive_underflows_to_negative_zero')]:
        yield dict(name=name,stages=[weights],seed=1)
    for seed in [7]:
        for weights, name in [([1,3],'positive'),([-1,-3],'negative'),([-1,1],'cancel')]:
            yield dict(name=f'{name}_seed{seed}', stages=[weights], seed=seed)
    for priorities in [[5,0],[0,5],[5,5]]:
        yield dict(name='priority_'+'_'.join(map(str,priorities)), stages=[[1,3]], priorities=priorities, seed=1)
    yield dict(name='sparse_default', stages=[[3,1]], omitted=[1], seed=1)
    yield dict(name='sparse_empty', stages=[[1,1]], omitted=[0,1], seed=1)
    yield dict(name='sequential_opposite_signs', stages=[[-1,-3],[2,1]], seed=1)
    yield dict(name='sequential_cancellation', stages=[[-1,1],[1,3]], seed=1)
    yield dict(name='sequential_forced', stages=[[-1,0],[0,2]], seed=1)
    yield dict(name='sequential_three', stages=[[1,2,3],[-1,-2,-1]], seed=7)


def model(case):
    places = [f'p{i}' for i in range(len(case['stages'])+1)]
    transitions, arcs = [], []
    for stage, weights in enumerate(case['stages']):
        for branch in range(len(weights)):
            name = f's{stage}b{branch}'
            transitions.append((name,name))
            arcs.extend([(places[stage],name,1),(name,places[stage+1],1)])
    return dict(name=case['name'], places=places, transitions=transitions, arcs=arcs,
                initial={places[0]:1}, final={places[-1]:1}, traces=[])


def capture(case, count=COUNT):
    random.seed(case['seed']); numpy.random.seed(case['seed'])
    net, initial, final, log = fitting.make_case(model(case))
    names = [name for name,label in model(case)['transitions']]
    weights = [weight for stage in case['stages'] for weight in stage]
    smap = {}
    for transition in net.transitions:
        index = names.index(transition.name)
        if index in case.get('omitted',[]): continue
        rv = Normal(0,0)
        rv.set_weight(weights[index])
        rv.set_priority(case.get('priorities',[0]*len(names))[index])
        smap[transition] = rv
    try:
        simulated, _ = algorithm.apply(log,net,initial,final,parameters={
            'num_simulations':count,'case_arrival_ratio':100,
            'provided_stochastic_map':smap,'enable_diagnostics':False,
            'default_num_resources_per_place':1,'show_progress_bar':False})
        paths = [[event['concept:name'] for event in trace] for trace in simulated]
        assert len(paths)==count and all(len(path)==len(case['stages']) for path in paths)
        return dict(spec=case, paths=paths, counts=dict(sorted(Counter('|'.join(path) for path in paths).items())))
    except Exception as error:
        return dict(spec=case,error=dict(type=type(error).__name__,message=str(error)))


def invalid_cases():
    for weights,name in [([-1,2],'mixed_positive_sum'),([-2,1],'mixed_negative_sum'),
                         ([1e308,1e308],'positive_overflow'),([-1e308,-1e308],'negative_overflow'),
                         ([float('nan'),1],'nan'),([float('inf'),1],'infinity')]:
        yield dict(name=name,stages=[weights],seed=1)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    assert pm4py.__version__=='2.7.22'
    rows=[]
    for case in cases():
        row=capture(case)
        assert 'error' not in row, row
        rows.append(row)
        print(case['name'],'captured',flush=True)
    invalid=[]
    for case in invalid_cases():
        row=capture(case,1)
        assert 'error' in row,row
        # JSON strings represent nonfinite public inputs without nonstandard JSON.
        row['spec']['stages']=[[x if math.isfinite(x) else str(x) for x in stage] for stage in case['stages']]
        invalid.append(row)
    document=dict(reference='2.7.22',reference_commit='0715c31f3f1d85924107d6aa5329806db69df899',
        public_apis=['Normal.set_weight','Normal.set_priority','pm4py.algo.simulation.montecarlo.algorithm.apply'],
        environment=dict(numpy=numpy.__version__,TZ=os.environ.get('TZ')),
        sampling=dict(count=COUNT,arrival=100,start=1000000,absolute_proportion_limit=.065,public_native_limit=.09),
        limits=['Empirical branch distributions, not identical cross-runtime RNG sequences.',
                'Zero fixed durations isolate branch selection; timestamps are not asserted.',
                'Sparse omitted records have effective weight one; public absence may have separate service-time behavior.',
                'Weight validity is assessed for each enabled subset, not globally.'],
        observations=rows,invalid_observations=invalid)
    args.output.write_text(json.dumps(document,indent=2,allow_nan=False)+'\n',encoding='utf-8')


if __name__=='__main__':
    try: main()
    except Exception as error:
        print(json.dumps(dict(error=type(error).__name__,message=str(error))))
        raise SystemExit(1)
