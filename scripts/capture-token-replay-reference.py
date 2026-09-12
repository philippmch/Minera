"""Resumable public PM4Py token-replay capture; no comparison/native calls.

Pinned Python/PYTHONPATH required. Each (fixture, pltr mode) uses a fresh child.
Resume by repeating the command; --retry-unresolved retries failed children.
"""
import argparse
import collections
import ctypes
import hashlib
import inspect
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import time


def save(path, value):
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n', encoding='utf-8')
    temporary.replace(path)


def capture(case, pltr):
    import pm4py
    from pm4py.algo.conformance.tokenreplay import algorithm as replay
    from pm4py.objects.petri_net.obj import PetriNet, Marking
    from pm4py.objects.petri_net.utils.petri_utils import add_arc_from_to
    from pm4py.objects.log.obj import EventLog, Trace, Event
    assert pm4py.__version__ == '2.7.22'
    spec = case['net']
    net = PetriNet('replay-reference')
    nodes = {name: PetriNet.Place(name) for name in spec['places']}
    net.places.update(nodes.values())
    for name, label in spec['transitions']:
        nodes[name] = PetriNet.Transition(name, label)
        net.transitions.add(nodes[name])
    for source, target, weight in spec['arcs']:
        add_arc_from_to(nodes[source], nodes[target], net, weight=weight)
    initial = Marking({nodes[k]: v for k,v in spec['initial'].items()})
    final = Marking({nodes[k]: v for k,v in spec['final'].items()})
    params = dict(case['parameters'], show_progress_bar=False, enable_pltr_fitness=pltr)
    key = params.get('pm4py:param:activity_key', 'concept:name')
    log = EventLog([Trace([Event({} if a is None else {key:a}) for a in trace], attributes={'concept:name':str(i)}) for i,trace in enumerate(case['left'])])
    trace_ids = {id(trace): i for i,trace in enumerate(log)}
    def canonical(value):
        if value is None or isinstance(value, (str, bool, int)):
            return value
        if isinstance(value, float):
            return value if math.isfinite(value) else {'float':str(value)}
        if isinstance(value, PetriNet.Place): return {'place':value.name}
        if isinstance(value, PetriNet.Transition): return {'transition':value.name}
        if isinstance(value, PetriNet.Arc): return {'arc':[value.source.name,value.target.name,value.weight]}
        if isinstance(value, Trace):
            return {'trace_index':trace_ids[id(value)]} if id(value) in trace_ids else {'trace': [canonical(e) for e in value], 'attributes':canonical(value.attributes)}
        if isinstance(value, Marking): return {'marking':{p.name:n for p,n in sorted(value.items(), key=lambda x:x[0].name)}}
        if isinstance(value, (dict, Event)):
            if all(isinstance(k,str) for k in value): return {k:canonical(v) for k,v in value.items()}
            pairs = [[canonical(k), canonical(v)] for k,v in value.items()]
            return {'mapping': sorted(pairs,key=lambda pair:json.dumps(pair[0],sort_keys=True))}
        if isinstance(value, (list,tuple)): return [canonical(v) for v in value]
        if isinstance(value, (set,frozenset)): return {'set':sorted([canonical(v) for v in value],key=lambda v:json.dumps(v,sort_keys=True))}
        raise TypeError('Unrepresentable public result type: '+type(value).__name__)
    try:
        raw = replay.apply(log,net,initial,final,parameters=params)
    except Exception as error:
        return {'parameters':params,'error':type(error).__name__,'message':str(error)}
    return {'parameters':params,'result':canonical(raw),'signature':str(inspect.signature(replay.apply))}


def resident_bytes(child):
    if os.name == 'nt':
        from ctypes import wintypes
        class Counters(ctypes.Structure):
            _fields_ = [('cb',wintypes.DWORD),('faults',wintypes.DWORD)] + [(n,ctypes.c_size_t) for n in ('peak','working','qpp','qpn','qnp','qnn','page','peakpage')]
        c= Counters(); c.cb=ctypes.sizeof(c)
        fn=ctypes.WinDLL('psapi',use_last_error=True).GetProcessMemoryInfo
        fn.argtypes=[wintypes.HANDLE,ctypes.POINTER(Counters),wintypes.DWORD]; fn.restype=wintypes.BOOL
        if fn(int(child._handle),ctypes.byref(c),c.cb): return c.working
        if child.poll() is None: raise OSError(ctypes.get_last_error(),'memory monitoring failed')
        return 0
    path=Path(f'/proc/{child.pid}/status')
    if path.exists():
        for line in path.read_text().splitlines():
            if line.startswith('VmRSS:'): return int(line.split()[1])*1024
    if child.poll() is None: raise RuntimeError('memory monitoring unavailable')
    return 0


def select(observations, limit):
    groups=collections.defaultdict(list); seen=set()
    for row in observations:
        identity=json.dumps([row['net'],row['left'],row['parameters']],sort_keys=True)
        if identity in seen: continue
        seen.add(identity)
        family=re.sub(r'_(?:False|True)(?:_(?:False|True))*$', '',row['name'])
        family=re.sub(r'_trace_.*$', '',family)
        groups[family].append(row)
    # Reserve several controls per family instead of consuming the budget on
    # first (often walk=false) cases from hundreds of families.
    def priority(name):
        important = ('depth_51','depth_52','parallel_stages_6','parallel_stages_8',
                     'blocked_join_after_prefix','blocked_direct_join',
                     'duplicate_order_after_route','duplicate_order_original',
                     'progress_chain_unreachable','disabled_join_after_progress',
                     'weighted_visible','weighted_hidden_join','zero_boundary',
                     'all_zero_markings','multiple_keys_one_positive')
        return (not any(x in name for x in important),
                not any(x in name for x in ('weighted','chain','zero')), name)
    # Component suffixes enumerate traces rather than distinct routing models.
    regrouped=collections.defaultdict(list)
    for name,rows in groups.items():
        family=re.sub(r'_\d+$', '',name) if name.startswith('component_') else name
        regrouped[family].extend(rows)
    groups=regrouped
    names=sorted(groups,key=priority)[:max(1,limit//4)]
    selected=[]
    for name in names:
        rows=groups[name]
        # Prefer active routing and then sample the complete control range.
        rows.sort(key=lambda r:(not r['parameters'].get('walk_through_hidden_trans',True),
                                not r['parameters'].get('try_to_reach_final_marking_through_hidden',True),
                                r['name']))
        indices=sorted({round(i*(len(rows)-1)/3) for i in range(4)})
        selected.extend(rows[i] for i in indices)
    # Fill remaining capacity without duplicating selected inputs.
    selected_names={r['name'] for r in selected}
    for name in sorted(groups,key=priority):
        for row in groups[name]:
            if len(selected)>=limit: break
            if row['name'] not in selected_names:
                selected.append(row); selected_names.add(row['name'])
        if len(selected)>=limit: break
    return [{k:r[k] for k in ('name','net','left','parameters','pnml','left_xes','equivalent_element_renamings') if k in r} for r in selected]


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--source',type=Path,default=Path('build/petri-zero-marking-reference.json'))
    p.add_argument('--output',type=Path,default=Path('build/token-replay-routing-reference-controls.json'))
    p.add_argument('--limit',type=int,default=160)
    p.add_argument('--retry-unresolved',action='store_true')
    p.add_argument('--worker',type=Path); p.add_argument('--pltr',type=int,choices=(0,1))
    a=p.parse_args()
    if a.worker:
        save(a.output,capture(json.loads(a.worker.read_text(encoding='utf-8')),bool(a.pltr))); return
    if not 1<=a.limit<=160: p.error('limit must be 1..160')
    selected=select(json.loads(a.source.read_text(encoding='utf-8'))['observations'],a.limit)
    digest=hashlib.sha256(json.dumps(selected,sort_keys=True).encode()).hexdigest()
    data=json.loads(a.output.read_text(encoding='utf-8')) if a.output.exists() else {'reference':'2.7.22','selection_sha256':digest,'selected':selected,'limits':{'seconds':20,'resident_mib':512,'poll_seconds':0.05},'observations':[],'unresolved':[]}
    if data['selection_sha256']!=digest: raise ValueError('Changed selection: use a different output path')
    done={r['id'] for r in data['observations']}
    if not a.retry_unresolved: done.update(r['id'] for r in data['unresolved'])
    work=a.output.parent/(a.output.stem+'-workers'); work.mkdir(parents=True,exist_ok=True)
    for i,case in enumerate(selected):
        for mode in (0,1):
            identity=f'{i}:{mode}'
            if identity in done: continue
            source=work/f'{i}.json'; output=work/f'{i}-{mode}.json'; logs=work/f'{i}-{mode}.log'
            save(source,case); reason=None; peak=0; started=time.monotonic()
            with logs.open('wb') as stream:
                child=subprocess.Popen([sys.executable,str(Path(__file__).resolve()),'--worker',str(source.resolve()),'--pltr',str(mode),'--output',str(output.resolve())],stdout=stream,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
                try:
                    while child.poll() is None:
                        peak=max(peak,resident_bytes(child))
                        if peak>512*1024*1024: reason='memory'; break
                        if time.monotonic()-started>20: reason='timeout'; break
                        time.sleep(.05)
                finally:
                    if child.poll() is None: child.kill()
                    code=child.wait()
            data['unresolved']=[r for r in data['unresolved'] if r['id']!=identity]
            record={'id':identity,'name':case['name'],'enable_pltr_fitness':bool(mode),'peak_resident_bytes':peak}
            if not reason and code==0:
                data['observations'].append(dict(record,**json.loads(output.read_text(encoding='utf-8'))))
            else: data['unresolved'].append(dict(record,reason=reason or 'crash',exit_code=code,log=str(logs)))
            save(a.output,data)
        print(f'{i+1}/{len(selected)} fixtures saved',flush=True)

if __name__=='__main__': main()
