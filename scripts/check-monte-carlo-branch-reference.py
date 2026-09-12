"""Compare public and native branch distributions with preselected proportion limits."""
import argparse
from collections import Counter
import importlib.util
import itertools
import json
import math
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
spec=importlib.util.spec_from_file_location('scheduler',Path(__file__).with_name('check-monte-carlo-scheduler-reference.py'))
scheduler=importlib.util.module_from_spec(spec)
spec.loader.exec_module(scheduler)


def model(case):
    places=[f'p{i}' for i in range(len(case['stages'])+1)]
    transitions=[];arcs=[]
    for stage,weights in enumerate(case['stages']):
        for branch in range(len(weights)):
            name=f's{stage}b{branch}'
            transitions.append((name,name));arcs.extend([(places[stage],name,1),(name,places[stage+1],1)])
    return scheduler.support.pnml(places,transitions,arcs,{places[0]:1},{places[-1]:1})


def expected(case):
    stages=[]
    for weights in case['stages']:
        total=sum(weights)
        probabilities=[1/len(weights)]*len(weights) if total==0 else [x/total for x in weights]
        assert all(math.isfinite(x) and 0<=x<=1 for x in probabilities)
        stages.append(probabilities)
    paths={}
    for choices in itertools.product(*(range(len(x)) for x in stages)):
        path='|'.join(f's{i}b{choice}' for i,choice in enumerate(choices))
        paths[path]=math.prod(stages[i][choice] for i,choice in enumerate(choices))
    return stages,paths


def validate(paths,row,sampling,compare_public=False):
    assert len(paths)==sampling['count']
    stages,probabilities=expected(row['spec'])
    counts=Counter('|'.join(path) for path in paths)
    assert set(counts)<=probabilities.keys(),'Unexpected path'
    assert all(probabilities[path]>0 for path in counts),'Zero-probability path occurred'
    for path in paths:
        assert len(path)==len(stages),'Wrong number of stages'
    n=len(paths)
    maximum=max(abs(counts[path]/n-p) for path,p in probabilities.items())
    for stage,probs in enumerate(stages):
        observed=Counter(path[stage] for path in paths)
        maximum=max(maximum,max(abs(observed[f's{stage}b{i}']/n-p) for i,p in enumerate(probs)))
    assert maximum<=sampling['absolute_proportion_limit'],f'Analytical proportion discrepancy {maximum}'
    result=dict(maximum_proportion_error=maximum,counts=dict(sorted(counts.items())))
    if compare_public:
        public=row['counts']
        discrepancy=max(abs(counts[path]/n-public.get(path,0)/n) for path in probabilities)
        assert discrepancy<=sampling['public_native_limit'],f'Public/native discrepancy {discrepancy}'
        result['maximum_public_native_error']=discrepancy
    return result


def parameters(case,sampling,mode):
    weights=[w for stage in case['stages'] for w in stage]
    values=dict(monte_carlo_simulations=str(sampling['count']),monte_carlo_max_steps=str(len(case['stages'])),
        monte_carlo_start=str(sampling['start']),monte_carlo_case_arrival_ratio=str(sampling['arrival']),
        monte_carlo_seed=str(case['seed']),monte_carlo_default_resources_per_place='1',
        monte_carlo_transition_durations=','.join(['normal:0;0']*len(weights)))
    if mode in ['direct','override']:
        values['monte_carlo_transition_weights']=','.join(map(str,weights))
    if mode in ['map','override']:
        records=[]
        for i,weight in enumerate(weights):
            if i in case.get('omitted',[]):continue
            priority=case.get('priorities',[0]*len(weights))[i]
            records.append(f'{i}:timed:normal:0:0:0:{weight if mode=="map" else 1}:{priority}')
        values['monte_carlo_provided_stochastic_map']=','.join(records) or 'empty'
    return values


def parse_paths(xml):
    return [[event['concept:name'] for event in trace] for trace in scheduler.trace_events(xml)]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures',type=Path,default=Path(__file__).resolve().parents[1]/'docs/monte-carlo-branch-reference-fixtures.json')
    parser.add_argument('--exporter',type=Path)
    parser.add_argument('--server',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    document=json.loads(args.fixtures.read_text(encoding='utf-8'))
    rows,sampling=document['observations'],document['sampling']
    checks=[];native={}
    def check(name,route,action):
        try:
            value=action()
            checks.append(dict(name=name,route=route,passed=True,**(value or {})))
        except Exception as error:
            checks.append(dict(name=name,route=route,passed=False,error=f'{type(error).__name__}: {error}'))
        print(name,route,'passed' if checks[-1]['passed'] else 'DIFF',flush=True)
    for row in rows:
        def public():
            assert dict(Counter('|'.join(path) for path in row['paths']))==row['counts']
            return validate(row['paths'],row,sampling)
        check(row['spec']['name'],'public',public)
    for row in document['invalid_observations']:
        def invalid():
            assert row['error']['type']=='ValueError'
        check(row['spec']['name'],'public-invalid',invalid)
    with tempfile.TemporaryDirectory(prefix='minera-branch-') as temporary:
        directory=Path(temporary)
        log,source,output=directory/'log.csv',directory/'model.pnml',directory/'generated.xes'
        log.write_text('case_id,activity,timestamp\n1,s0b0,2026-01-01T00:00:00Z\n',encoding='utf-8')
        if args.exporter:
            for row in rows+document['invalid_observations']:
                source.write_text(model(row['spec']),encoding='utf-8')
                for mode in ['direct','map','override']:
                    def cli():
                        command=[str(args.exporter.resolve()),str(log),'--check-pnml',str(source),'--playout-miner','imported']
                        for key,value in parameters(row['spec'],sampling,mode).items():
                            command.extend(['--'+key.replace('_','-'),value])
                        command.extend(['--playout',str(output)])
                        result=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',timeout=60)
                        if 'error' in row:
                            assert result.returncode!=0,'Expected invalid-weight failure'
                            return
                        assert result.returncode==0,result.stdout+result.stderr
                        paths=parse_paths(output.read_text(encoding='utf-8'))
                        native[row['spec']['name'],'cli',mode]=paths
                        return validate(paths,row,sampling,True)
                    check(row['spec']['name'],'cli-'+mode,cli)
        if args.server:
            with socket.socket() as listener:
                listener.bind(('127.0.0.1',0));port=listener.getsockname()[1]
            def request(path,data=None,content_type='application/xml'):
                req=urllib.request.Request(f'http://127.0.0.1:{port}'+path,data=data,headers={'Content-Type':content_type})
                with urllib.request.urlopen(req,timeout=60) as response:return response.read().decode()
            with (directory/'server.log').open('w',encoding='utf-8') as server_log:
                server=subprocess.Popen([str(args.server.resolve()),'--port',str(port)],stdout=server_log,stderr=server_log,
                    creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
                try:
                    for _ in range(200):
                        try:request('/api/version');break
                        except OSError:
                            assert server.poll() is None,'Server exited before readiness'
                            time.sleep(.05)
                    else:raise RuntimeError('Server readiness timed out')
                    request('/api/mine',log.read_bytes(),'text/csv')
                    for row in rows+document['invalid_observations']:
                        request('/api/check-model',model(row['spec']).encode())
                        for mode in ['direct','map','override']:
                            def http():
                                query=urllib.parse.urlencode(dict(parameters(row['spec'],sampling,mode),miner='imported',mode='monte_carlo'))
                                if 'error' in row:
                                    try:request('/api/playout?'+query,b'')
                                    except urllib.error.HTTPError as error:
                                        assert error.code==400,f'Unexpected HTTP {error.code}'
                                        return
                                    raise AssertionError('Expected invalid-weight failure')
                                paths=parse_paths(request('/api/playout?'+query,b''))
                                native[row['spec']['name'],'http',mode]=paths
                                return validate(paths,row,sampling,True)
                            check(row['spec']['name'],'http-'+mode,http)
                finally:
                    server.terminate()
                    try:server.wait(timeout=10)
                    except subprocess.TimeoutExpired:server.kill();server.wait(timeout=10)
        for row in rows:
            available=[key for key in native if key[0]==row['spec']['name']]
            if len(available)>1:
                def equivalent():
                    assert all(native[key]==native[available[0]] for key in available),'Native same-seed path mismatch'
                check(row['spec']['name'],'native-seed-equivalence',equivalent)
    args.output.write_text(json.dumps(dict(checks=checks),indent=2)+'\n',encoding='utf-8')
    passed=sum(x['passed'] for x in checks)
    print(f'{passed}/{len(checks)} branch checks passed')
    return int(passed!=len(checks))


if __name__=='__main__':
    try:raise SystemExit(main())
    except Exception as error:
        print(json.dumps(dict(error=type(error).__name__,message=str(error))))
        raise SystemExit(1)
