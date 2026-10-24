"""Public PM4Py 2.7.22 model-quality capture; no native calls.

Builds a deterministic corpus of Petri nets and logs, then records the public
evaluation results for every replay-fitness, precision, generalization,
simplicity and composite selector (plus the automatic dispatchers). Each case
runs in a fresh child; a public call exceeding its time limit is recorded as
unresolved rather than as a result. POSIX only (per-call SIGALRM).

    python -I -B scripts/capture-evaluation-reference.py \
        --output docs/evaluation-reference-fixtures.json
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import random
import signal
import subprocess
import sys
import tempfile
import time

LIMIT_SECONDS = 600
CALL_SECONDS = 30


class CallTimeout(Exception):
    pass


def save(path, value):
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=1, allow_nan=False) + '\n', encoding='utf-8')
    temporary.replace(path)


# ---------------------------------------------------------------- corpus

def hand_net(name, places, transitions, arcs, initial, final):
    return {'name': name, 'places': places, 'transitions': transitions,
            'arcs': [list(a) if len(a) == 3 else [a[0], a[1], 1] for a in arcs],
            'initial': initial, 'final': final}


def hand_nets():
    nets = []
    nets.append(hand_net('sequence', ['i', 'p1', 'p2', 'o'],
                         [['tA', 'A'], ['tB', 'B'], ['tC', 'C']],
                         [('i', 'tA'), ('tA', 'p1'), ('p1', 'tB'), ('tB', 'p2'), ('p2', 'tC'), ('tC', 'o')],
                         {'i': 1}, {'o': 1}))
    nets.append(hand_net('xor', ['i', 'p1', 'o'],
                         [['tA', 'A'], ['tB', 'B'], ['tC', 'C'], ['tD', 'D']],
                         [('i', 'tA'), ('tA', 'p1'), ('p1', 'tB'), ('p1', 'tC'), ('tB', 'o'), ('tC', 'o'),
                          ('o', 'tD'), ('tD', 'o')],
                         {'i': 1}, {'o': 1}))
    nets.append(hand_net('parallel', ['i', 'p1', 'p2', 'p3', 'p4', 'o'],
                         [['tA', 'A'], ['tB', 'B'], ['tC', 'C'], ['tD', 'D']],
                         [('i', 'tA'), ('tA', 'p1'), ('tA', 'p2'), ('p1', 'tB'), ('p2', 'tC'),
                          ('tB', 'p3'), ('tC', 'p4'), ('p3', 'tD'), ('p4', 'tD'), ('tD', 'o')],
                         {'i': 1}, {'o': 1}))
    nets.append(hand_net('loop_tau', ['i', 'p1', 'p2', 'o'],
                         [['tA', 'A'], ['tB', 'B'], ['tC', 'C'], ['tau_back', None], ['tau_skip', None]],
                         [('i', 'tA'), ('tA', 'p1'), ('p1', 'tB'), ('tB', 'p2'), ('p2', 'tau_back'),
                          ('tau_back', 'p1'), ('p2', 'tC'), ('tC', 'o'), ('p1', 'tau_skip'), ('tau_skip', 'p2')],
                         {'i': 1}, {'o': 1}))
    nets.append(hand_net('duplicate_labels', ['i', 'l', 'r', 'o'],
                         [['tA1', 'A'], ['tA2', 'A'], ['tB1', 'B'], ['tC2', 'C']],
                         [('i', 'tA1'), ('i', 'tA2'), ('tA1', 'l'), ('tA2', 'r'), ('l', 'tB1'), ('r', 'tC2'),
                          ('tB1', 'o'), ('tC2', 'o')],
                         {'i': 1}, {'o': 1}))
    nets.append(hand_net('unsound_dead', ['i', 'p1', 'dead', 'o'],
                         [['tA', 'A'], ['tB', 'B'], ['tC', 'C']],
                         [('i', 'tA'), ('tA', 'p1'), ('tA', 'dead'), ('p1', 'tB'), ('tB', 'o'),
                          ('dead', 'tC'), ('tC', 'dead')],
                         {'i': 1}, {'o': 1}))
    nets.append(hand_net('weighted', ['i', 'p1', 'o'],
                         [['tA', 'A'], ['tB', 'B'], ['tC', 'C']],
                         [('i', 'tA'), ('tA', 'p1', 2), ('p1', 'tB'), ('tB', 'p1'), ('p1', 'tC', 2), ('tC', 'o')],
                         {'i': 1}, {'o': 1}))
    nets.append(hand_net('flower', ['p'],
                         [['tA', 'A'], ['tB', 'B'], ['tC', 'C'], ['tD', 'D']],
                         [('p', 'tA'), ('tA', 'p'), ('p', 'tB'), ('tB', 'p'), ('p', 'tC'), ('tC', 'p'),
                          ('p', 'tD'), ('tD', 'p')],
                         {'p': 1}, {'p': 1}))
    nets.append(hand_net('optional_tau', ['i', 'p1', 'p2', 'o'],
                         [['tA', 'A'], ['tB', 'B'], ['skip', None], ['tC', 'C'], ['tD', 'D']],
                         [('i', 'tA'), ('tA', 'p1'), ('p1', 'tB'), ('p1', 'skip'), ('tB', 'p2'), ('skip', 'p2'),
                          ('p2', 'tC'), ('p2', 'tD'), ('tC', 'o'), ('tD', 'o')],
                         {'i': 1}, {'o': 1}))
    nets.append(hand_net('two_tokens', ['i', 'p1', 'o'],
                         [['tA', 'A'], ['tB', 'B']],
                         [('i', 'tA'), ('tA', 'p1'), ('p1', 'tB'), ('tB', 'o')],
                         {'i': 2}, {'o': 2}))
    nets.append(hand_net('silent_start_end', ['i', 'p1', 'p2', 'p3', 'o'],
                         [['tau_in', None], ['tA', 'A'], ['tB', 'B'], ['tau_out', None]],
                         [('i', 'tau_in'), ('tau_in', 'p1'), ('tau_in', 'p2'), ('p1', 'tA'), ('p2', 'tB'),
                          ('tA', 'p3'), ('tB', 'o'), ('p3', 'tau_out'), ('tau_out', 'o', 1)],
                         {'i': 1}, {'o': 2}))
    nets.append(hand_net('silent_skip', ['i', 'o'],
                         [['tA', 'A'], ['skip', None]],
                         [('i', 'tA'), ('tA', 'o'), ('i', 'skip'), ('skip', 'o')],
                         {'i': 1}, {'o': 1}))
    nets.append(hand_net('weighted_split', ['i', 'p1', 'p2', 'o'],
                         [['tA', 'A'], ['tB', 'B'], ['tC', 'C'], ['tD', 'D']],
                         [('i', 'tA'), ('tA', 'p1', 3), ('tA', 'p2'), ('p1', 'tB'), ('tB', 'p2'),
                          ('p1', 'tC', 2), ('tC', 'p2', 2), ('p2', 'tD', 3), ('tD', 'o')],
                         {'i': 1}, {'o': 1}))
    nets.append(hand_net('weighted_silent', ['i', 'p1', 'o'],
                         [['tA', 'A'], ['tau', None], ['tB', 'B'], ['tC', 'C']],
                         [('i', 'tA'), ('tA', 'p1'), ('p1', 'tau'), ('tau', 'p1', 2), ('p1', 'tB', 2),
                          ('tB', 'o'), ('p1', 'tC'), ('tC', 'o')],
                         {'i': 1}, {'o': 1}))
    return nets


HAND_LOGS = {
    'fitting': [['A', 'B', 'C'], ['A', 'C', 'B'], ['A', 'B', 'C'], ['A', 'B', 'D', 'C']],
    'noisy': [['A', 'B', 'C'], ['A', 'X', 'C'], ['B', 'A', 'C'], ['A', 'B', 'B', 'C', 'D'], ['C'], [], ['A', 'A']],
    'loops': [['A', 'B', 'C'], ['A', 'B', 'B', 'C'], ['A', 'C'], ['A', 'B', 'B', 'B', 'C'], ['A', 'D', 'C']],
}


def to_spec(name, net, im, fm):
    places = sorted(p.name for p in net.places)
    transitions = sorted([t.name, t.label] for t in net.transitions)
    arcs = sorted([a.source.name, a.target.name, a.weight] for a in net.arcs)
    return {'name': name, 'places': places, 'transitions': transitions, 'arcs': arcs,
            'initial': {p.name: n for p, n in im.items()}, 'final': {p.name: n for p, n in fm.items()}}


def discovered_cases(seed_count):
    import numpy
    import pm4py
    from pm4py.objects.log.obj import EventLog, Trace, Event
    cases = []
    for seed in range(seed_count):
        random.seed(seed)
        numpy.random.seed(seed)
        tree = pm4py.generate_process_tree(parameters={'min': 4, 'mode': 6, 'max': 8, 'silent': 0.1,
                                                       'loop': 0.1, 'or': 0.0, 'duplicate': 0.05})
        log = pm4py.play_out(tree, parameters={'num_traces': 25})
        traces = [[e['concept:name'] for e in t] for t in log]
        rng = random.Random(1000 + seed)
        noisy = []
        for trace in traces:
            trace = list(trace)
            if trace and rng.random() < 0.35:
                choice = rng.randrange(3)
                position = rng.randrange(len(trace))
                if choice == 0:
                    del trace[position]
                elif choice == 1:
                    trace.insert(position, rng.choice(trace + ['Z']))
                else:
                    other = rng.randrange(len(trace))
                    trace[position], trace[other] = trace[other], trace[position]
            noisy.append(trace)
        event_log = EventLog([Trace([Event({'concept:name': a}) for a in trace],
                                    attributes={'concept:name': str(i)})
                              for i, trace in enumerate(noisy)])
        clean_log = EventLog([Trace([Event({'concept:name': a}) for a in trace],
                                    attributes={'concept:name': str(i)})
                              for i, trace in enumerate(traces)])
        miners = [('inductive_fitting', lambda: pm4py.discover_petri_net_inductive(clean_log, noise_threshold=0.0)),
                  ('inductive', lambda: pm4py.discover_petri_net_inductive(event_log, noise_threshold=0.0)),
                  ('inductive_noise', lambda: pm4py.discover_petri_net_inductive(event_log, noise_threshold=0.3)),
                  ('alpha', lambda: pm4py.discover_petri_net_alpha(event_log)),
                  ('heuristics', lambda: pm4py.discover_petri_net_heuristics(event_log))]
        for miner, discover in miners:
            net, im, fm = discover()
            spec = to_spec(f'seed{seed}_{miner}', net, im, fm)
            cases.append({'name': f'seed{seed}_{miner}_clean', 'net': spec, 'traces': traces})
            cases.append({'name': f'seed{seed}_{miner}_noisy', 'net': spec, 'traces': noisy})
    return cases


def corpus(seed_count):
    cases = []
    for net in hand_nets():
        for log_name, traces in HAND_LOGS.items():
            cases.append({'name': f"{net['name']}_{log_name}", 'net': net, 'traces': traces})
    cases.append({'name': 'sequence_empty_log', 'net': hand_nets()[0], 'traces': []})
    cases.extend(discovered_cases(seed_count))
    return cases


# ---------------------------------------------------------------- child

def finite(value):
    if isinstance(value, bool) or value is None:
        return value
    if isinstance(value, (int, float)):
        value = float(value)
        return value if math.isfinite(value) else {'float': str(value)}
    if isinstance(value, dict):
        return {str(k): finite(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [finite(v) for v in value]
    return {'repr': repr(value)}


def capture(case):
    import pm4py
    from pm4py.objects.petri_net.obj import PetriNet, Marking
    from pm4py.objects.petri_net.utils.petri_utils import add_arc_from_to
    from pm4py.objects.log.obj import EventLog, Trace, Event
    from pm4py.algo.evaluation.replay_fitness import algorithm as fitness
    from pm4py.algo.evaluation.precision import algorithm as precision
    from pm4py.algo.evaluation.generalization import algorithm as generalization
    from pm4py.algo.evaluation.simplicity import algorithm as simplicity
    from pm4py.algo.evaluation import algorithm as composite
    assert pm4py.__version__ == '2.7.22'
    spec = case['net']
    net = PetriNet(spec['name'])
    nodes = {name: PetriNet.Place(name) for name in spec['places']}
    net.places.update(nodes.values())
    for name, label in spec['transitions']:
        nodes[name] = PetriNet.Transition(name, label)
        net.transitions.add(nodes[name])
    for source, target, weight in spec['arcs']:
        add_arc_from_to(nodes[source], nodes[target], net, weight=weight)
    im = Marking({nodes[k]: v for k, v in spec['initial'].items()})
    fm = Marking({nodes[k]: v for k, v in spec['final'].items()})
    log = EventLog([Trace([Event({'concept:name': a}) for a in trace], attributes={'concept:name': str(i)})
                    for i, trace in enumerate(case['traces'])])
    with tempfile.TemporaryDirectory() as directory:
        path = os.path.join(directory, 'net.pnml')
        pm4py.write_pnml(net, im, fm, path)
        pnml = Path(path).read_text(encoding='utf-8')
        # Metrics are taken on the net read back from the shipped PNML: the
        # reference's replay order follows its in-memory node order, so this
        # makes the expected values belong to exactly the document tested.
        net, im, fm = pm4py.read_pnml(path)
    params = {'show_progress_bar': False}
    calls = {
        'fitness_token': lambda: fitness.apply(log, net, im, fm, variant=fitness.Variants.TOKEN_BASED, parameters=params),
        'fitness_alignment': lambda: fitness.apply(log, net, im, fm, variant=fitness.Variants.ALIGNMENT_BASED, parameters=params),
        'fitness_auto': lambda: fitness.apply(log, net, im, fm, parameters=params),
        'precision_token': lambda: precision.apply(log, net, im, fm, variant=precision.Variants.ETCONFORMANCE_TOKEN, parameters=params),
        'precision_alignment': lambda: precision.apply(log, net, im, fm, variant=precision.Variants.ALIGN_ETCONFORMANCE, parameters=params),
        'precision_automaton': lambda: precision.apply(log, net, im, fm, variant=precision.Variants.AUTOMATON_AFTER_ALIGN, parameters=params),
        'precision_auto': lambda: precision.apply(log, net, im, fm, parameters=params),
        'generalization_token': lambda: generalization.apply(log, net, im, fm, parameters=params),
        'simplicity_arc_degree': lambda: simplicity.apply(net, variant=simplicity.Variants.SIMPLICITY_ARC_DEGREE),
        'simplicity_extended_cardoso': lambda: simplicity.apply(net, variant=simplicity.Variants.EXTENDED_CARDOSO),
        'simplicity_extended_cyclomatic': lambda: simplicity.apply(net, variant=simplicity.Variants.EXTENDED_CYCLOMATIC),
        'composite': lambda: composite.apply(log, net, im, fm, parameters=params),
    }
    def expire(signum, frame):
        raise CallTimeout()

    signal.signal(signal.SIGALRM, expire)
    results = {}
    for key, call in calls.items():
        started = time.monotonic()
        signal.alarm(CALL_SECONDS)
        try:
            results[key] = {'value': finite(call())}
        except CallTimeout:
            results[key] = {'unresolved': 'timeout'}
        except Exception as error:
            results[key] = {'error': type(error).__name__, 'message': str(error)[:300]}
        finally:
            signal.alarm(0)
        results[key]['seconds'] = round(time.monotonic() - started, 3)
    return {'pnml': pnml, 'results': results}


# ---------------------------------------------------------------- driver

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=Path('docs/evaluation-reference-fixtures.json'))
    parser.add_argument('--seeds', type=int, default=12)
    parser.add_argument('--worker', type=Path)
    parser.add_argument('--runs', type=int, default=2,
                        help='independent children per case (at least 2)')
    parser.add_argument('--recapture', action='append', default=[],
                        help='case name to capture again (repeatable), e.g. with more --runs')
    args = parser.parse_args()
    if args.worker:
        save(args.output, capture(json.loads(args.worker.read_text(encoding='utf-8'))))
        return
    import pm4py
    cases = corpus(args.seeds)
    digest = hashlib.sha256(json.dumps(cases, sort_keys=True).encode()).hexdigest()
    data = {'reference': pm4py.__version__, 'corpus_sha256': digest, 'limit_seconds': LIMIT_SECONDS,
            'call_seconds': CALL_SECONDS, 'observations': [], 'unresolved': []}
    if args.output.exists():
        # Resume: an existing capture of the identical corpus keeps its records.
        # Mined nets depend on the parent's hash seed, so a resume compares
        # case names and reuses each recorded case's own inputs.
        previous = json.loads(args.output.read_text(encoding='utf-8'))
        recorded = {r['name'] for r in previous['observations']} | {r['name'] for r in previous['unresolved']}
        if previous['corpus_sha256'] != digest and not recorded <= {c['name'] for c in cases}:
            raise ValueError('Changed corpus: use a different output path')
        stored = {r['name']: r for r in previous['observations']}
        cases = [{k: stored[c['name']][k] for k in c} if c['name'] in stored else c for c in cases]
        data = previous
    if args.runs < 2:
        parser.error('--runs must be at least 2')
    data['observations'] = [r for r in data['observations'] if r['name'] not in args.recapture]
    data['unresolved'] = [r for r in data['unresolved'] if r['name'] not in args.recapture]
    done = {r['name'] for r in data['observations']} | {r['name'] for r in data['unresolved']}
    with tempfile.TemporaryDirectory() as work:
        for index, case in enumerate(cases):
            if case['name'] in done:
                continue
            source = Path(work) / f'{index}.json'
            save(source, case)
            runs, reason, stderr = [], None, b''
            # Independent children (fresh hash randomization each): a metric
            # whose public value differs between any of them is recorded as
            # nondeterministic instead of as an expected result.
            for attempt in range(args.runs):
                output = Path(work) / f'{index}-{attempt}.json'
                child = subprocess.Popen([sys.executable, '-I', '-B', str(Path(__file__).resolve()),
                                          '--worker', str(source), '--output', str(output)],
                                         stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
                try:
                    _, stderr = child.communicate(timeout=LIMIT_SECONDS)
                    reason = None if child.returncode == 0 else 'crash'
                except subprocess.TimeoutExpired:
                    child.kill()
                    _, stderr = child.communicate()
                    reason = 'timeout'
                if reason:
                    break
                runs.append(json.loads(output.read_text(encoding='utf-8')))
            if reason is None:
                merged = runs[0]
                strip = lambda r: {k: v for k, v in r.items() if k != 'seconds'}
                for key, first in merged['results'].items():
                    observed = []
                    for run in runs:
                        if strip(run['results'][key]) not in observed:
                            observed.append(strip(run['results'][key]))
                    if len(observed) > 1:
                        merged['results'][key] = {'unresolved': 'nondeterministic',
                                                  'observed': observed,
                                                  'runs': len(runs),
                                                  'seconds': first['seconds']}
                data['observations'].append(dict(case, **merged))
            else:
                data['unresolved'].append({'name': case['name'], 'reason': reason,
                                           'stderr': stderr.decode('utf-8', 'replace')[-500:]})
            save(args.output, data)
            print(f"{index + 1}/{len(cases)} {case['name']} {reason or 'ok'}", flush=True)
    data['observations'].sort(key=lambda r: [c['name'] for c in cases].index(r['name']))
    save(args.output, data)


if __name__ == '__main__':
    main()
