"""Audit a clean-room search model through public CLASSIC calls.

Compares costs and counters only; it does not verify reconstructed alignments
or execute the native implementation. Never inspects upstream source.
"""
import argparse, heapq, itertools, json, os, warnings
import hashlib
from pathlib import Path


REFERENCE_COMMIT = '0715c31f3f1d85924107d6aa5329806db69df899'
REFERENCE_COMMIT_SCOPE = 'configured_reference_pin'


def _load_public_reference_api():
    """Load public reference modules only when running the search audit."""
    warnings.filterwarnings('ignore')
    import pm4py
    import pandas
    from pm4py.algo.conformance.alignments.dfg.variants import classic
    from pm4py.objects.log.obj import Trace, Event

    return pm4py, pandas, classic, Trace, Event


def _environment():
    return {'PYTHONHASHSEED': os.environ.get('PYTHONHASHSEED'),
            'TZ': os.environ.get('TZ')}


def _require_versions(pm4py, pandas):
    versions = (getattr(pm4py, '__version__', None),
                getattr(pandas, '__version__', None))
    if versions != ('2.7.22', '3.0.5'):
        raise RuntimeError(
            f'This audit requires pm4py==2.7.22 and pandas==3.0.5; got '
            f'pm4py=={versions[0]!r}, pandas=={versions[1]!r}')


def _file_metadata(path):
    resolved = Path(path).resolve()
    digest = hashlib.sha256()
    try:
        with resolved.open("rb") as source:
            for chunk in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(chunk)
    except OSError as error:
        raise RuntimeError(f"Unable to hash {resolved}: {error}") from error
    return {"path": str(resolved), "sha256": digest.hexdigest()}


def _module_metadata(module):
    filename = getattr(module, "__file__", None)
    return _file_metadata(filename) if filename else None

def simulate(word,dfg,starts,ends,parameters):
    nodes=set(starts)|set(ends)
    for a,b in dfg:nodes.update((a,b))
    start='@@start'; end='@@end'
    out={n:[] for n in nodes|{start,end}}
    out[start]=sorted(starts)
    for a,b in dfg:out[a].append(b)
    for a in ends:out[a].append(end)
    for n in out:out[n].sort()
    def cost(name,label,default):
        return parameters.get(name+'_cost_function',{}).get(label,default)
    heap=[(0,0,0,start,False,0,0)] # cost,-position,sequence,node,last-mm,reported,snapshot
    sequence=0; pops=0; closed={}; expansions=[]
    while heap:
        internal,negative_pos,seq,node,mm,reported,snapshot=heapq.heappop(heap)
        pos=-negative_pos;pops+=1
        if node==end and pos==len(word):
            return dict(visited_states=snapshot,closed=len(closed),internal_cost=internal,cost=reported),expansions
        if any(n==node and p>=pos for n,p in closed):continue
        closed[node,pos]=internal
        expansions.append((pops,internal,node,pos,mm))
        def push(n,p,m,inc,rep):
            nonlocal sequence
            if any(cn==n and cp>=p for cn,cp in closed):return
            sequence+=1
            heapq.heappush(heap,(internal+inc,-p,sequence,n,m,reported+rep,pops))
        has=pos<len(word);label=word[pos] if has else end
        sync=has and label in out[node]
        unknown=has and label not in nodes
        for n in out[node]:
            if unknown:continue
            if n==end:push(n,pos,False,0,0);continue
            if has and label==n:
                c=cost('sync',n,0);push(n,pos+1,False,c,c)
            if not sync:
                c=cost('model_move',n,10000);push(n,pos,True,c,c)
        if has and not mm:
            rep=cost('log_move',label,10000)
            inc=cost('internal_log_move',label,rep if label in nodes else 0)
            push(node,pos+1,False,inc,rep)
    return None,expansions

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args()
    pm4py, pandas, classic, Trace, Event = _load_public_reference_api()
    _require_versions(pm4py, pandas)
    models={
      'path':({('a','b'):1},{'a':1},{'b':1},'abx'),
      'singleton':({}, {'a':1},{'a':1},'ax'),
      'cycle':({('a','b'):1,('b','a'):1},{'a':1},{'a':1},'abx'),
      'fork':({('a','b'):1,('a','c'):1},{'a':1},{'b':1,'c':1},'abcx'),
      'chain3':({('a','b'):1,('b','c'):1},{'a':1},{'c':1},'abcx'),
    }
    scenarios={
      'default':{},
      'sync':{'sync_cost_function':dict(a=2,b=3,c=4,x=5)},
      'model':{'model_move_cost_function':dict(a=2,b=3,c=4,x=5)},
      'log':{'log_move_cost_function':dict(a=2,b=3,c=4,x=5)},
      'internal':{'internal_log_move_cost_function':dict(a=2,b=3,c=4,x=5)},
      'unequal':{'sync_cost_function':dict(a=2,b=3,c=7,x=11),
          'log_move_cost_function':dict(a=123,b=456,c=147,x=59),
          'model_move_cost_function':dict(a=789,b=135,c=249,x=41)},
    }
    rows=[]; summary={}
    for model,(dfg,starts,ends,alphabet) in models.items():
      for scenario,parameters in scenarios.items():
        count=0;mismatch=0
        for length in range(5):
          for word in itertools.product(alphabet,repeat=length):
            expected=classic.apply_trace(Trace([Event({'concept:name':x}) for x in word]),dfg,starts,ends,parameters=parameters)
            expected={k:expected[k] for k in ('visited_states','closed','internal_cost','cost')} if expected else None
            actual,expansions=simulate(word,dfg,starts,ends,parameters)
            count+=1
            if actual!=expected:
              mismatch+=1
              rows.append(dict(model=model,scenario=scenario,word=word,expected=expected,actual=actual,expansions=expansions))
        summary[model+'/'+scenario]=dict(cases=count,mismatches=mismatch)
    result=dict(reference=pm4py.__version__,pandas=pandas.__version__,
                reference_commit=REFERENCE_COMMIT,
                reference_commit_scope=REFERENCE_COMMIT_SCOPE,
                environment=_environment(), reference_module=_module_metadata(classic),
                summary=summary,mismatches=rows)
    args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(summary,indent=2))
    print('total',sum(x['cases'] for x in summary.values()),'mismatches',len(rows))
    return 1 if rows else 0
if __name__=='__main__':raise SystemExit(main())
