"""Observe automatic and forced distribution fitting through the public map API."""
import importlib.util
import json
from pathlib import Path
import argparse

def module(name):
    path=Path(__file__).with_name(name+'.py')
    spec=importlib.util.spec_from_file_location(name,path)
    result=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result

api=module('capture-stochastic-fit-reference')
samples=module('capture-random-variable-fit-reference')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    base=next(api.cases())
    rows=[]
    for name in ['empty','one_positive','positive_constant','two_positive',
                 'normal_dominant','negative_varying','positive_mixed',
                 'near_zero_inside','near_zero_boundary','long_tail']:
        for force in [None,'EXPONENTIAL','NORMAL','UNIFORM','GAMMA']:
            spec=dict(base,name=name+'_'+str(force),force_distribution=force,
                traces=[[['A',0],['B',value]] for value in samples.SAMPLES[name]])
            rows.append(api.capture(spec,True))
    args.output.write_text(json.dumps(dict(reference='2.7.22',
        reference_commit='0715c31f3f1d85924107d6aa5329806db69df899',
        public_api='pm4py.algo.simulation.montecarlo.utils.replay.get_map_from_log_and_net',
        dependencies=dict(numpy=samples.numpy.__version__,scipy=samples.scipy.__version__),
        observations=rows),indent=2)+'\n',encoding='utf-8')
    print('Captured',len(rows),'public map outcomes,',sum('error' in row for row in rows),'errors')

if __name__=='__main__':
    main()
