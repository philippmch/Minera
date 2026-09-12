"""Compare native PNML usage results with captured public comparison outputs."""
import argparse
import collections
import json
import math
import subprocess
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path


def ordered_ids(pnml, kind):
    return [node.attrib['id'] for node in ET.fromstring(pnml).iter()
            if node.tag.rsplit('}', 1)[-1] == kind and 'id' in node.attrib]


def canonicalize(body, observation):
    places = ordered_ids(observation['pnml'], 'place')
    transitions = ordered_ids(observation['pnml'], 'transition')
    assert body['family'] == 'PETRI_USAGE_COMPARISON'
    assert len(body['places']) == len(places)
    assert len(body['transitions']) == len(transitions)
    result = {}
    multiplicities = collections.Counter()

    def record(value):
        left, right, total = (value[name + '_occurrences'] for name in ('log1', 'log2', 'total'))
        assert total == left + right
        assert math.isclose(value['direction'], (right - left) / total if total else 0, abs_tol=1e-9)
        return dict(log1_occ=left, log2_occ=right, total_occ=total,
                    direction=value['direction'], label=value['label'], color=value['color'])

    for kind, ids in [('places', places), ('transitions', transitions)]:
        for value in body[kind]:
            key = kind[:-1] + ':' + ids[value['id']]
            assert key not in result
            result[key] = record(value)
    for arc in body['arcs']:
        assert arc['kind'] == ('input' if arc['input'] else 'output'), 'ordinary PNML arc changed kind'
        transition = transitions[arc['transition']]
        place = places[arc['place']]
        key = 'arc:' + (place + '->' + transition if arc['input'] else transition + '->' + place)
        multiplicities[key] += 1
        value = record(arc)
        # Native unit arcs encode weights by repeated endpoint entries. Every
        # entry must retain the same firing count; summing would inflate usage.
        if key in result:
            assert result[key] == value
        result[key] = value
    expected_multiplicities = {'arc:' + source + '->' + target: weight
                               for source, target, weight in observation['net']['arcs']}
    assert dict(multiplicities) == expected_multiplicities
    return result


def equal(actual, expected):
    if set(actual) != set(expected):
        return False
    for key in actual:
        if set(actual[key]) != set(expected[key]):
            return False
        for field in actual[key]:
            if field == 'direction':
                if not math.isclose(actual[key][field], expected[key][field], abs_tol=1e-9):
                    return False
            elif actual[key][field] != expected[key][field]:
                return False
    return True


def validated_renamings(observation):
    """Validate declared symmetries independently of the observed usage counts."""
    net = observation['net']
    kinds = {name: ('place', None) for name in net['places']}
    for name, label in net['transitions']:
        if name in kinds:
            raise ValueError('ambiguous net node name')
        kinds[name] = ('transition', label)
    result = []
    annotations = observation.get('equivalent_element_renamings', [])
    if not isinstance(annotations, list):
        raise ValueError('equivalent_element_renamings must be a list')
    for annotation in annotations:
        if not isinstance(annotation, dict) or any(
                key not in kinds or not isinstance(value, str) or value not in kinds
                for key, value in annotation.items()):
            raise ValueError('invalid net automorphism node')
        mapping = {name: annotation.get(name, name) for name in kinds}
        if set(mapping.values()) != set(kinds):
            raise ValueError('net automorphism must be bijective')
        if any(kinds[name] != kinds[target] for name, target in mapping.items()):
            raise ValueError('net automorphism changes node kind or visible label')
        arcs = collections.Counter(tuple(arc) for arc in net['arcs'])
        renamed = collections.Counter((mapping[source], mapping[target], weight)
                                      for source, target, weight in net['arcs'])
        if arcs != renamed:
            raise ValueError('net automorphism changes weighted arcs')
        for marking in ('initial', 'final'):
            if {mapping[name]: count for name, count in net[marking].items()} != net[marking]:
                raise ValueError('net automorphism changes ' + marking + ' marking')
        # Construct keys from endpoints, avoiding assumptions about delimiters
        # in node names. Every record value remains completely untouched.
        keys = {kind + ':' + name: kind + ':' + mapping[name]
                for name, (kind, _) in kinds.items()}
        for source, target, _ in net['arcs']:
            keys['arc:' + source + '->' + target] = 'arc:' + mapping[source] + '->' + mapping[target]
        if len(set(keys.values())) != len(keys):
            raise ValueError('ambiguous net automorphism element keys')
        result.append(keys)
    return result


def match_result(actual, expected, renamings):
    if equal(actual, expected):
        return 'identity'
    for keys in renamings:
        if set(actual) <= set(keys) and equal({keys[key]: value for key, value in actual.items()}, expected):
            return 'net_automorphism'
    return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--native', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    reference = json.loads(args.reference.read_text(encoding='utf-8'))
    checks = []
    skipped = []
    with tempfile.TemporaryDirectory(prefix='minera-usage-native-') as directory:
        directory = Path(directory)
        for observation in reference['observations']:
            # Reject malformed annotations even if identity would match or the
            # fixture has unsupported parameters; they are never trusted data.
            renamings = validated_renamings(observation)
            match_kind = None
            parameters = observation['parameters']
            flags = {'stop_immediately_unfit': '--compare-petri-stop-immediately-unfit',
                     'cleaning_token_flood': '--compare-petri-cleaning-token-flood',
                     'exhaustive_invisible_exploration': '--compare-petri-exhaustive-invisible-exploration',
                     'walk_through_hidden_trans': '--compare-petri-walk-through-hidden',
                     'try_to_reach_final_marking_through_hidden': '--compare-petri-reach-final-through-hidden',
                     'pm4py:param:activity_key': '--compare-petri-activity-attribute'}
            unsupported = set(parameters) - set(flags)
            if unsupported:
                skipped.append(dict(name=observation['name'], parameters=sorted(unsupported)))
                continue
            model, left, right = (directory / name for name in ('model.pnml', 'left.xes', 'right.xes'))
            model.write_text(observation['pnml'], encoding='utf-8')
            left.write_text(observation['left_xes'], encoding='utf-8')
            right.write_text(observation['right_xes'], encoding='utf-8')
            command = [str(args.native.resolve()), str(left), '--compare-petri-model', str(model), '--compare-petri-reference', str(right)]
            for key, value in parameters.items():
                command.extend([flags[key], str(value).lower() if isinstance(value, bool) else value])
            process = subprocess.run(command, capture_output=True, text=True, encoding='utf-8')
            if process.returncode:
                actual = dict(error=process.returncode, stderr=process.stderr)
                expected = observation['result']
                matched = (expected.get('error') == 'KeyError'
                           and expected['message'].strip("'") in process.stderr)
                if (expected.get('error') == 'IndexError' and parameters.get('cleaning_token_flood')
                        and not any(observation['net']['initial'].values())):
                    matched = 'initial marking' in process.stderr
            else:
                try:
                    actual = canonicalize(json.loads(process.stdout), observation)
                    match_kind = match_result(actual, observation['result'], renamings)
                    matched = match_kind is not None
                except (AssertionError, KeyError, ValueError) as error:
                    actual = dict(error=str(error), stdout=process.stdout)
                    matched = False
            checks.append(dict(name=observation['name'], match=matched,
                               **({'match_kind': match_kind or 'identity'} if matched else {}),
                               **({} if matched else {'expected': observation['result'], 'actual': actual})))
    unresolved = reference.get('unresolved_groups', [])
    excluded = reference.get('excluded_models', [])
    identity_matches = sum(check.get('match_kind') == 'identity' for check in checks)
    automorphism_matches = sum(check.get('match_kind') == 'net_automorphism' for check in checks)
    args.output.write_text(json.dumps(dict(checks=checks, skipped=skipped,
        identity_matches=identity_matches, net_automorphism_matches=automorphism_matches,
        unresolved_reference_groups=unresolved, excluded_reference_models=excluded), indent=2) + '\n', encoding='utf-8')
    failures = [check for check in checks if not check['match']]
    print(f'{len(checks) - len(failures)}/{len(checks)} native element-usage comparisons matched; {len(skipped)} unsupported-parameter observations skipped')
    print(f'{identity_matches} identity matches; {automorphism_matches} validated net-automorphism matches')
    if unresolved or excluded:
        unresolved_count = sum(len(group['cases']) for group in unresolved)
        print(f'{unresolved_count} reference cases remain unresolved; {len(excluded)} reference models excluded from the comparison profile')
    for failure in failures[:12]:
        print(failure['name'])
    assert not failures


if __name__ == '__main__':
    main()
