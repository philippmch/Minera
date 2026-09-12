"""Compare standalone native replay with saved public trace and place results."""
import argparse
import collections
import copy
import importlib.util
import json
import math
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET


def equivalence_context(case):
    specification = importlib.util.spec_from_file_location(
        'petri_usage_automorphism_validation',
        Path(__file__).with_name('check-petri-element-usage-native.py'))
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    # Always validate annotations, including for identity matches/error cases.
    renamings = module.validated_renamings(case)
    labels = dict(case['net']['transitions'])
    incident = {name: set() for name in labels}
    for source, target, _ in case['net']['arcs']:
        if source in incident: incident[source].add(target)
        if target in incident: incident[target].add(source)
    return renamings, labels, incident


def legal_hidden_reordering(actual, expected, labels, incident):
    """Construct the required permutation using only legal adjacent swaps."""
    if len(actual) != len(expected): return False
    pending = list(actual)
    for index, wanted in enumerate(expected):
        try: found = pending.index(wanted, index)
        except ValueError: return False
        while found > index:
            left, right = pending[found - 1], pending[found]
            if set(left) != {'transition'} or set(right) != {'transition'}: return False
            a, b = left['transition'], right['transition']
            if a not in labels or b not in labels or labels[a] is not None or labels[b] is not None:
                return False
            if incident[a] & incident[b]: return False
            pending[found - 1], pending[found] = right, left
            found -= 1
    return True


def rename_result(traces, places, keys):
    result = copy.deepcopy(traces)
    transition = lambda name: keys['transition:' + name].removeprefix('transition:')
    place = lambda name: keys['place:' + name].removeprefix('place:')
    for trace in result:
        for field in ('activated_transitions', 'transitions_with_problems'):
            trace[field] = [{'transition': transition(item['transition'])} for item in trace[field]]
        trace['enabled_transitions_in_marking']['set'] = sorted(
            [{'transition': transition(item['transition'])} for item in trace['enabled_transitions_in_marking']['set']],
            key=lambda value: json.dumps(value, sort_keys=True))
        trace['reached_marking']['marking'] = {
            place(name): count for name, count in trace['reached_marking']['marking'].items()}
    return result, None if places is None else {place(name): counters for name, counters in places.items()}


def match_replay(traces, places, expected_traces, expected_places, context):
    renamings, labels, incident = context
    def strict(left, totals):
        return difference(left, expected_traces, 'traces') or (
            difference(totals, expected_places, 'places') if expected_places is not None else None)
    mismatch = strict(traces, places)
    if mismatch is None: return 'identity', None
    def commute(left, totals):
        if len(left) != len(expected_traces): return False
        adjusted = copy.deepcopy(left)
        for actual, expected in zip(adjusted, expected_traces):
            if not legal_hidden_reordering(actual['activated_transitions'], expected['activated_transitions'], labels, incident):
                return False
            actual['activated_transitions'] = expected['activated_transitions']
        return strict(adjusted, totals) is None
    if commute(traces, places): return 'independent_hidden_order', None
    for keys in renamings:
        renamed, totals = rename_result(traces, places, keys)
        if strict(renamed, totals) is None or commute(renamed, totals):
            return 'net_automorphism', None
    return None, mismatch


def node_ids(pnml, kind):
    return [node.attrib['id'] for node in ET.fromstring(pnml).iter()
            if node.tag.rsplit('}', 1)[-1] == kind and 'id' in node.attrib]


def normalize_native(body, case):
    assert body['family'] == 'TOKEN_REPLAY' and body['variant'] == 'forward'
    places, transitions = node_ids(case['pnml'], 'place'), node_ids(case['pnml'], 'transition')
    traces = []
    totals = {place: dict(c=0, p=0, m=0, r=0) for place in places}
    for index, row in enumerate(body['traces']):
        assert row['trace_index'] == index
        assert len(row['reached_marking']) == len(places)
        keys = row['reached_marking_keys']
        assert len(keys) == len(places) and all(isinstance(value, bool) for value in keys)
        assert all(keys[i] or count == 0 for i, count in enumerate(row['reached_marking']))
        traces.append(dict(
            trace_is_fit=row['trace_is_fit'], trace_fitness=row['trace_fitness'],
            produced_tokens=row['produced'], consumed_tokens=row['consumed'],
            missing_tokens=row['missing'], remaining_tokens=row['remaining'],
            activated_transitions=[{'transition': transitions[t]} for t in row['activated_transitions']],
            transitions_with_problems=[{'transition': transitions[t]} for t in row['transitions_with_problems']],
            reached_marking={'marking': {place: row['reached_marking'][i] for i, place in enumerate(places) if keys[i]}},
            enabled_transitions_in_marking={'set': sorted(
                [{'transition': transitions[t]} for t in row['enabled_transitions']],
                key=lambda value: json.dumps(value, sort_keys=True))}))
        assert len(row['places']) == len(places)
        for place, values in zip(places, row['places']):
            for short, field in [('c', 'consumed'), ('p', 'produced'), ('m', 'missing'), ('r', 'remaining')]:
                totals[place][short] += values[field]
        events = row['event_diagnostics']
        assert len(events) == row['replayed_events'] <= len(case['left'][index])
        assert [event['event_index'] for event in events] == list(range(len(events)))
        activated = [t for event in events for t in event['activated_transitions']]
        assert row['activated_transitions'][:len(activated)] == activated
        assert [t for event in events for t in event['transitions_with_problems']] == row['transitions_with_problems']
    return traces, totals


def difference(actual, expected, path='result'):
    if type(actual) is not type(expected):
        if isinstance(actual, (int, float)) and not isinstance(actual, bool) and isinstance(expected, (int, float)) and not isinstance(expected, bool):
            return None if math.isclose(actual, expected, rel_tol=1e-12, abs_tol=1e-12) else path
        return path + '.type'
    if isinstance(actual, float):
        return None if math.isclose(actual, expected, rel_tol=1e-12, abs_tol=1e-12) else path
    if isinstance(actual, dict):
        if actual.keys() != expected.keys(): return path + '.keys'
        for key in actual:
            result = difference(actual[key], expected[key], path + '.' + key)
            if result: return result
        return None
    if isinstance(actual, list):
        if len(actual) != len(expected): return path + '.length'
        for index, (left, right) in enumerate(zip(actual, expected)):
            result = difference(left, right, path + '[' + str(index) + ']')
            if result: return result
        return None
    return None if actual == expected else path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--native', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    reference = json.loads(args.reference.read_text(encoding='utf-8'))
    observations = collections.defaultdict(list)
    for item in reference['observations']:
        observations[int(item['id'].split(':')[0])].append(item)
    flags = {'cleaning_token_flood': '--token-replay-cleaning-token-flood',
             'exhaustive_invisible_exploration': '--token-replay-exhaustive-invisible-exploration',
             'stop_immediately_unfit': '--token-replay-stop-immediately-unfit',
             'walk_through_hidden_trans': '--token-replay-walk-through-hidden-transitions',
             'try_to_reach_final_marking_through_hidden': '--token-replay-try-to-reach-final-marking-through-hidden',
             'consider_remaining_in_fitness': '--token-replay-consider-remaining-in-fitness',
             'consider_activities_not_in_model_in_fitness': '--token-replay-consider-activities-not-in-model-in-fitness',
             'pm4py:param:activity_key': '--token-replay-activity-attribute'}
    checks = []
    with tempfile.TemporaryDirectory(prefix='minera-token-replay-') as temporary:
        directory = Path(temporary)
        for index, case in enumerate(reference['selected']):
            context = equivalence_context(case)
            if index not in observations: continue
            assert not set(case['parameters']) - set(flags), case['parameters']
            model, log = directory / 'model.pnml', directory / 'log.xes'
            model.write_text(case['pnml'], encoding='utf-8')
            log.write_text(case['left_xes'], encoding='utf-8')
            command = [str(args.native.resolve()), str(log), '--token-replay-model', str(model)]
            for key, value in case['parameters'].items():
                if key == 'stop_immediately_unfit':
                    if value: command.append(flags[key])
                else: command.extend([flags[key], str(value).lower() if isinstance(value, bool) else value])
            try:
                process = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', timeout=30)
                native = dict(error=process.returncode, stderr=process.stderr) if process.returncode else json.loads(process.stdout)
                normalized = normalize_native(native, case) if not process.returncode else None
            except (subprocess.TimeoutExpired, AssertionError, KeyError, ValueError) as error:
                native = dict(error=type(error).__name__, message=str(error))
                normalized = None
            for expected in observations[index]:
                mismatch = None
                match_kind = 'identity'
                if 'error' in expected:
                    matched = bool(native.get('error')) and expected['error'] == 'IndexError' and 'initial marking' in native.get('stderr', '')
                    mismatch = None if matched else 'error'
                elif normalized is None:
                    mismatch = 'native_execution'
                else:
                    traces, places = normalized
                    expected_traces = expected['result'][0] if expected['enable_pltr_fitness'] else expected['result']
                    expected_places = None
                    if expected['enable_pltr_fitness']:
                        value = expected['result'][1]
                        expected_places = {key['place']: {field: counters[field] for field in ('c', 'p', 'm', 'r')}
                                           for key, counters in value.get('mapping', [])}
                    match_kind, mismatch = match_replay(
                        traces, places, expected_traces, expected_places, context)
                checks.append(dict(id=expected['id'], name=case['name'], pltr=expected['enable_pltr_fitness'], matched=mismatch is None,
                                   **({'match_kind': match_kind} if mismatch is None else {}),
                                   **({} if mismatch is None else dict(mismatch=mismatch, native=native, expected=expected))))
    failures = [item for item in checks if not item['matched']]
    match_counts = {kind: sum(row.get('match_kind') == kind for row in checks)
                    for kind in ('identity', 'independent_hidden_order', 'net_automorphism')}
    args.output.write_text(json.dumps(dict(checks=checks, match_counts=match_counts, unresolved=reference.get('unresolved', [])), indent=2) + '\n', encoding='utf-8')
    print(f'{len(checks)-len(failures)}/{len(checks)} public replay observations matched across {len(observations)} native inputs')
    print(json.dumps(match_counts, sort_keys=True))
    for item in failures[:20]: print(item['name'], 'pltr='+str(item['pltr']), item['mismatch'])
    raise SystemExit(bool(failures))


if __name__ == '__main__':
    main()
