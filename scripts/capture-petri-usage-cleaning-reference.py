"""Public-only token-flood cleaning oracle with resumable bounded workers.

Run with --output FILE [--resume-from EXISTING] [--timeout-seconds 20]
[--memory-mib 512]. Existing output resumes automatically; --retry-unresolved
explicitly retries unfinished groups. Resource monitoring is polled every 50ms,
so the resident-memory threshold may be exceeded briefly before termination.
"""
import argparse
import importlib.util
import itertools
import json
import math
import os
import subprocess
import sys
import time
import ctypes
from pathlib import Path

import pm4py
from pm4py.algo.conformance.tokenreplay import algorithm as replay
from pm4py.algo.comparison.petrinet import element_usage_comparison as comparison
from pm4py.objects.log.obj import EventLog, Trace, Event
from pm4py.objects.petri_net.exporter import exporter as pnml_exporter
from pm4py.objects.petri_net.utils.petri_utils import get_s_components_from_petri


def capture_case(case):
    net, initial, final = base.make_net(case['net'])
    log = base.log(case['left'])
    reference = base.log(case['right'])
    parameters = dict(case['parameters'], show_progress_bar=False)
    try:
        compared = comparison.compare_element_usage_two_logs(net, initial, final, log, reference, parameters)
        result = {base.element_key(key): value for key, value in compared.items()}
    except Exception as error:
        result = dict(error=type(error).__name__, message=str(error))
    try:
        details = replay.apply(log, net, initial, final, parameters=dict(parameters, enable_pltr_fitness=True))
        traces, places, transitions, unknown = details
        diagnostics = [dict(activated=[t.name for t in item['activated_transitions']],
                            reached={p.name: value for p, value in item['reached_marking'].items()},
                            missing=item['missing_tokens'], remaining=item['remaining_tokens'],
                            problems=[t.name for t in item['transitions_with_problems']]) for item in traces]
        place_fitness = {p.name: {key: value[key] for key in ('c', 'p', 'm', 'r')} for p, value in places.items()}
    except Exception as error:
        diagnostics = dict(error=type(error).__name__, message=str(error))
        place_fitness = {}
    pnml = pnml_exporter.serialize(net, initial, final_marking=final)
    if isinstance(pnml, bytes):
        pnml = pnml.decode('utf-8')
    try:
        components = [sorted(p.name if hasattr(p, 'name') else p for p in component)
                      for component in get_s_components_from_petri(net, initial, final)]
    except Exception as error:
        components = dict(error=type(error).__name__, message=str(error))
    return dict(case, result=result, diagnostics=diagnostics, place_fitness=place_fitness,
                components=components, pnml=pnml, left_xes=base.xes(case['left']), right_xes=base.xes(case['right']))


def cases():
    templates = {case['name']: case['net'] for case in base.cases()}
    for model in ('chain_0', 'parallel_join_0', 'sink_with_residual_cleanup_0', 'weighted_0'):
        net = templates[model]
        for count in range(5):
            for trace in itertools.product('AB', repeat=count):
                for clean in (False, True):
                    yield dict(name=f'{model}_trace_{"".join(trace)}_clean_{clean}', net=net,
                               left=[list(trace)], right=[['A', 'B']],
                               parameters={'cleaning_token_flood': clean})
    for source, middle, sink in itertools.product(range(1, 4), range(3), range(3)):
        net = dict(templates['chain_0'], initial={key: value for key, value in
                   [('p0', source), ('p1', middle), ('p2', sink)] if value})
        for trace in ('B', 'BA', 'BBA', 'ABA'):
            yield dict(name=f'preloaded_{source}_{middle}_{sink}_{trace}', net=net,
                       left=[list(trace)], right=[['A', 'B']], parameters={'cleaning_token_flood': True})
    for source, other in itertools.product(range(1, 5), range(3)):
        net = dict(templates['multiple_markings_0'], initial={key: value for key, value in
                   [('p0', source), ('p1', other)] if value})
        for repeats in (1, 2, 3):
            yield dict(name=f'weighted_join_{source}_{other}_{repeats}', net=net,
                       left=[['A'] * repeats], right=[['A']], parameters={'cleaning_token_flood': True})
    for model in ('chain_0', 'parallel_join_0', 'invisible_start_0', 'invisible_end_0'):
        for walk, finish, stop, clean in itertools.product((False, True), repeat=4):
            yield dict(name=f'controls_{model}_{walk}_{finish}_{stop}_{clean}', net=templates[model],
                       left=[['B', 'A'], ['A', 'B', 'A']], right=[['A', 'B']],
                       parameters=dict(cleaning_token_flood=clean, walk_through_hidden_trans=walk,
                                       try_to_reach_final_marking_through_hidden=finish, stop_immediately_unfit=stop))
    for model in ('empty_net_0', 'no_places_0', 'zero_markings_0', 'source_free_transition_0'):
        yield dict(name='empty_initial_' + model, net=templates[model], left=[['A']], right=[[]],
                   parameters={'cleaning_token_flood': True})
    for clean in (False, True):
        yield dict(name=f'missing_after_stop_{clean}', net=templates['chain_0'], left=[['B', None]], right=[['A', 'B']],
                   parameters={'cleaning_token_flood': clean, 'stop_immediately_unfit': True})
    for depth in (5, 6, 7):
        net = dict(places=[f'p{i}' for i in range(depth + 1)] + [f'd{i}' for i in range(depth)] + ['sink'],
                   transitions=[(f'split{i}', None) for i in range(depth)] + [('a', 'A'), ('b', 'B')],
                   arcs=[], initial={'p0': 1}, final={'sink': 1})
        for index in range(depth):
            net['arcs'].extend([(f'p{index}', f'split{index}', 1),
                               (f'split{index}', f'p{index + 1}', 1), (f'split{index}', f'd{index}', 1)])
        net['arcs'].extend([(f'p{depth}', 'a', 1), ('a', 'sink', 1), ('p0', 'b', 1), ('b', 'p0', 1)])
        for clean in (False, True):
            yield dict(name=f'cleaning_depth_{depth}_{clean}', net=net, left=[['A', 'B']], right=[[]],
                       parameters={'cleaning_token_flood': clean, 'walk_through_hidden_trans': False})
    component_inputs = json.loads(Path(__file__).with_name('petri-usage-component-cases.json').read_text(encoding='utf-8'))['cases']
    for fixture in component_inputs:
        net = fixture['net']
        alphabet = sorted({label for _, label in net['transitions'] if label is not None}) or ['X']
        traces = [list(trace) for length in range(3) for trace in itertools.product(alphabet, repeat=length)]
        traces.extend([alphabet * 2, list(reversed(alphabet)) * 2])
        for index, trace in enumerate(traces):
            for clean in (False, True):
                yield dict(name=f'component_{fixture["name"]}_{index}_{clean}', net=net,
                           left=[trace], right=[alphabet], parameters={'cleaning_token_flood': clean})
    final_inputs = json.loads(Path(__file__).with_name('petri-usage-final-routing-cases.json').read_text(encoding='utf-8'))['cases']
    for fixture in final_inputs:
        for finish, clean in itertools.product((False, True), repeat=2):
            yield dict(name=f'routing_{fixture["name"]}_{finish}_{clean}', net=fixture['net'],
                       left=[[]], right=[], parameters={'cleaning_token_flood': clean,
                           'try_to_reach_final_marking_through_hidden': finish},
                       equivalent_element_renamings=fixture.get('equivalent_element_renamings', []))
    pre_event_inputs = json.loads(Path(__file__).with_name('petri-usage-pre-event-cases.json').read_text(encoding='utf-8'))['cases']
    for fixture in pre_event_inputs:
        cleaning_values = (False,) if fixture['name'].startswith(('linear_', 'parallel_')) else (False, True)
        for walk, stop, clean in itertools.product((False, True), (False, True), cleaning_values):
            yield dict(name=f'preevent_{fixture["name"]}_{walk}_{stop}_{clean}', net=fixture['net'],
                       left=fixture.get('left', [['A']]), right=[], parameters={'cleaning_token_flood': clean,
                           'walk_through_hidden_trans': walk, 'stop_immediately_unfit': stop,
                           'try_to_reach_final_marking_through_hidden': False})
    zero_inputs = json.loads(Path(__file__).with_name('petri-usage-zero-marking-cases.json').read_text(encoding='utf-8'))['cases']
    yield from zero_inputs


def save_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')
    temporary.replace(path)


def resident_bytes(child):
    """Read only the process launched by this runner, never unrelated PIDs."""
    if os.name == 'nt':
        from ctypes import wintypes
        class Counters(ctypes.Structure):
            _fields_ = [('cb', wintypes.DWORD), ('PageFaultCount', wintypes.DWORD)] + [
                (name, ctypes.c_size_t) for name in ('PeakWorkingSetSize', 'WorkingSetSize',
                'QuotaPeakPagedPoolUsage', 'QuotaPagedPoolUsage', 'QuotaPeakNonPagedPoolUsage',
                'QuotaNonPagedPoolUsage', 'PagefileUsage', 'PeakPagefileUsage')]
        counters = Counters()
        counters.cb = ctypes.sizeof(counters)
        get_memory = ctypes.WinDLL('psapi', use_last_error=True).GetProcessMemoryInfo
        get_memory.argtypes = [wintypes.HANDLE, ctypes.POINTER(Counters), wintypes.DWORD]
        get_memory.restype = wintypes.BOOL
        if get_memory(int(child._handle), ctypes.byref(counters), counters.cb):
            return counters.WorkingSetSize
        if child.poll() is None:
            raise OSError(ctypes.get_last_error(), 'GetProcessMemoryInfo failed')
        return 0
    # Linux fallback keeps the same resident-memory contract.
    status = Path(f'/proc/{child.pid}/status')
    if status.exists():
        for line in status.read_text().splitlines():
            if line.startswith('VmRSS:'):
                return int(line.split()[1]) * 1024
    if child.poll() is None:
        raise RuntimeError('Resident-memory monitoring is unavailable on this platform')
    return 0


def run_bounded(args):
    planned = json.loads(json.dumps(list(cases())))
    groups = {}
    for case in planned:
        name = case['name']
        group = (name.rsplit('_', 1)[0] if name.startswith('zerokey_') else
                 name.rsplit('_', 3)[0] if name.startswith('preevent_') else
                 name.rsplit('_', 2)[0] if name.startswith(('component_', 'routing_')) else 'base')
        groups.setdefault(group, []).append(case)
    previous = {}
    source = args.output if args.output.exists() else args.resume_from
    if source and source.exists():
        previous = json.loads(source.read_text(encoding='utf-8'))
        if previous.get('reference') != pm4py.__version__:
            raise ValueError('Cannot resume an oracle from another reference version')
    observations = {item['name']: item for item in previous.get('observations', [])}
    for case in planned:
        prior = observations.get(case['name'])
        if prior and any(prior.get(key) != case.get(key) for key in ('net', 'left', 'right', 'parameters')):
            raise ValueError(f"Resumed fixture inputs changed: {case['name']}")
        if prior:
            # Equivalence declarations describe the checker's normalization,
            # not additional public outputs; refreshing them preserves the
            # captured comparison result and its exact input data.
            prior['equivalent_element_renamings'] = case.get('equivalent_element_renamings', [])
    unresolved = {item['group']: item for item in previous.get('unresolved_groups', [])}
    completed = dict(previous.get('completed_groups', {}))
    limits = dict(timeout_seconds=args.timeout_seconds, resident_memory_mib=args.memory_mib,
                  poll_interval_seconds=0.05, process_scope='one child per component/routing model; base cases may share one child')
    def persist():
        save_json(args.output, dict(reference=pm4py.__version__, observations=list(observations.values()),
                  limits=limits, completed_groups=completed, unresolved_groups=list(unresolved.values()),
                  excluded_models=[], planned_case_count=len(planned),
                  note='Captured public errors are observations. Timeout/memory/crash cases are unresolved, never expected outputs. Declared net equivalences are validated by the native checker; they are not additional public observations.'))
    # Preserve all previous observations before launching potentially failing children.
    persist()
    work = args.output.parent / (args.output.stem + '-workers')
    work.mkdir(parents=True, exist_ok=True)
    for number, (group, members) in enumerate(groups.items()):
        pending = [case for case in members if case['name'] not in observations]
        if not pending:
            completed[group] = dict(observation_count=len(members), source='captured or resumed')
            unresolved.pop(group, None)
            persist()
            continue
        completed.pop(group, None)
        if group in unresolved and not args.retry_unresolved:
            unresolved[group]['cases'] = [case['name'] for case in pending]
            unresolved[group]['partial_observations'] = len(members) - len(pending)
            persist()
            continue
        stem = work / f'{time.time_ns()}-{number}'
        input_path = stem.with_suffix('.input.json')
        output_path = stem.with_suffix('.jsonl')
        log_path = stem.with_suffix('.log')
        save_json(input_path, pending)
        command = [sys.executable, str(Path(__file__).resolve()), '--worker-input', str(input_path.resolve()),
                   '--worker-output', str(output_path.resolve())]
        started = time.monotonic()
        reason = None
        peak = 0
        with log_path.open('wb') as log_file:
            child = subprocess.Popen(command, stdout=log_file, stderr=subprocess.STDOUT,
                                     creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            try:
                while child.poll() is None:
                    used = resident_bytes(child)
                    peak = max(peak, used)
                    if used > args.memory_mib * 1024 * 1024:
                        reason = 'resident_memory_limit'
                        break
                    if time.monotonic() - started > args.timeout_seconds:
                        reason = 'wall_time_limit'
                        break
                    time.sleep(0.05)
            except BaseException:
                child.kill()
                child.wait()
                raise
            if reason:
                child.kill()
            code = child.wait()
        if output_path.exists():
            for line in output_path.read_text(encoding='utf-8').splitlines():
                try:
                    item = json.loads(line)
                except json.JSONDecodeError:
                    # A killed write is not a completed observation.
                    continue
                observations[item['name']] = item
        remaining = [case['name'] for case in members if case['name'] not in observations]
        elapsed = time.monotonic() - started
        if remaining:
            unresolved[group] = dict(group=group, reason=reason or 'child_failure', exit_code=code,
                elapsed_seconds=round(elapsed, 3), peak_resident_bytes=peak, cases=remaining,
                diagnostic_log=str(log_path), partial_observations=len(members) - len(remaining), limits=limits)
        else:
            completed[group] = dict(observation_count=len(members), elapsed_seconds=round(elapsed, 3), peak_resident_bytes=peak)
            unresolved.pop(group, None)
        persist()
        print(f'{group}: {len(members) - len(remaining)}/{len(members)} captured' +
              (f'; {reason or "child_failure"}' if remaining else ''), flush=True)
    print(f'Saved {len(observations)} observations; {len(unresolved)} unresolved groups to {args.output}', flush=True)


if __name__ == '__main__':
    spec = importlib.util.spec_from_file_location('base', Path(__file__).with_name('capture-petri-element-usage-reference.py'))
    base = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(base)
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path)
    parser.add_argument('--resume-from', type=Path)
    parser.add_argument('--retry-unresolved', action='store_true')
    parser.add_argument('--timeout-seconds', type=float, default=20)
    parser.add_argument('--memory-mib', type=int, default=512)
    parser.add_argument('--worker-input', type=Path, help=argparse.SUPPRESS)
    parser.add_argument('--worker-output', type=Path, help=argparse.SUPPRESS)
    args = parser.parse_args()
    assert pm4py.__version__ == '2.7.22'
    if args.worker_input:
        with args.worker_output.open('w', encoding='utf-8') as output:
            for case in json.loads(args.worker_input.read_text(encoding='utf-8')):
                output.write(json.dumps(capture_case(case)) + '\n')
                output.flush()
                os.fsync(output.fileno())
    else:
        if not args.output or not math.isfinite(args.timeout_seconds) or args.timeout_seconds <= 0 or args.memory_mib <= 0:
            parser.error('--output and positive timeout/memory limits are required')
        run_bounded(args)
