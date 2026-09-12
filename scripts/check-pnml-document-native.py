"""Compare PNML document export metadata to saved public serializer results."""
import argparse
import collections
import json
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET


def structure(xml):
    tree = ET.fromstring(xml)
    for node in tree.iter(): node.tag = node.tag.rsplit('}', 1)[-1]
    places = {p.attrib['id']: p.findtext('name/text') for p in tree.iter('place') if 'id' in p.attrib}
    transitions = {}
    for t in tree.iter('transition'):
        silent = any(n.attrib.get('activity') == '$invisible$' for n in t.findall('toolspecific'))
        transitions[t.attrib['id']] = None if silent else t.findtext('name/text')
    arcs = collections.Counter()
    for arc in tree.iter('arc'):
        arcs[(arc.attrib['source'], arc.attrib['target'], arc.findtext('type/text', 'normal'))] += int(arc.findtext('inscription/text', '1'))
    initial = {p.attrib['id']: int(p.findtext('initialMarking/text', '0')) for p in tree.iter('place')
               if 'id' in p.attrib and p.find('initialMarking') is not None}
    final = {p.attrib['idref']: int(p.findtext('text', '0')) for p in tree.findall('.//finalmarkings/marking/place')}
    return dict(places=places, transitions=transitions, arcs=arcs, initial=initial, final=final)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--native', type=Path, required=True)
    parser.add_argument('--replay-reference', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    reference = json.loads(args.reference.read_text(encoding='utf-8'))
    controls = ['--pnml-infer-initial-marking', 'false', '--pnml-infer-final-marking', 'false',
                '--pnml-require-workflow-boundaries', 'false']
    def run(*arguments):
        return subprocess.run([str(args.native.resolve()), *map(str, arguments)],
                               capture_output=True, text=True, encoding='utf-8', timeout=30)
    def inspect(source, destination=None):
        result = run('--inspect-pnml', source, *controls,
                     *(['--pnml', str(destination)] if destination else []))
        assert result.returncode == 0, result.stderr
        return json.loads(result.stdout)
    checks = []
    replay_checks = 0
    with tempfile.TemporaryDirectory(prefix='minera-pnml-document-') as temporary:
        directory = Path(temporary)
        source, destination = directory / 'input.pnml', directory / 'output.pnml'
        for row in reference['observations']:
            assert 'export_error' not in row, row
            source.write_text(row['pnml'], encoding='utf-8')
            document = inspect(source, destination)
            exported = structure(destination.read_text(encoding='utf-8'))
            assert exported == structure(row['pnml']), row['name']
            assert exported['initial'] == row['exported_initial'], row['name']
            assert exported['final'] == row['exported_final'], row['name']
            assert sorted(exported['places']) == row['exported_place_ids'], row['name']
            assert sorted(exported['transitions']) == row['exported_transition_ids'], row['name']
            ids = document.get('place_ids', [])
            assert len(ids) == document['place_count']
            for which in ('initial', 'final'):
                marking = document[which + '_marking']
                keys = document.get(which + '_marking_keys', [])
                assert len(keys) == len(marking) == len(ids)
                assert {p: n for p, n, present in zip(ids, marking, keys) if present} == exported[which]
            assert inspect(destination) == document, row['name']
            checks.append(dict(name=row['name'], matched=True))
        if args.replay_reference:
            replay = json.loads(args.replay_reference.read_text(encoding='utf-8'))
            flags = {'cleaning_token_flood': '--token-replay-cleaning-token-flood',
                     'exhaustive_invisible_exploration': '--token-replay-exhaustive-invisible-exploration',
                     'walk_through_hidden_trans': '--token-replay-walk-through-hidden-transitions',
                     'try_to_reach_final_marking_through_hidden': '--token-replay-try-to-reach-final-marking-through-hidden',
                     'consider_remaining_in_fitness': '--token-replay-consider-remaining-in-fitness',
                     'consider_activities_not_in_model_in_fitness': '--token-replay-consider-activities-not-in-model-in-fitness',
                     'pm4py:param:activity_key': '--token-replay-activity-attribute'}
            for case in replay['selected']:
                source.write_text(case['pnml'], encoding='utf-8')
                inspect(source, destination)
                log = directory / 'log.xes'
                log.write_text(case['left_xes'], encoding='utf-8')
                parameters = []
                for key, value in case['parameters'].items():
                    if key == 'stop_immediately_unfit':
                        if value: parameters.append('--token-replay-stop-immediately-unfit')
                    else: parameters.extend([flags[key], str(value).lower() if isinstance(value, bool) else value])
                before = run(log, '--token-replay-model', source, *parameters)
                after = run(log, '--token-replay-model', destination, *parameters)
                assert before.returncode == after.returncode, case['name']
                if before.returncode:
                    assert before.stderr == after.stderr, case['name']
                else: assert json.loads(before.stdout) == json.loads(after.stdout), case['name']
                replay_checks += 1
        # A malformed option must not truncate an existing output artifact.
        destination.write_text('unchanged', encoding='utf-8')
        for value in ('invalid', ''):
            result = run('--inspect-pnml', source, '--pnml', destination,
                         '--pnml-require-workflow-boundaries', value)
            assert result.returncode != 0 and destination.read_text() == 'unchanged'
        result = run('--inspect-pnml', source, *controls, '--pnml', directory / 'missing' / 'out.pnml')
        assert result.returncode != 0 and 'Cannot open PNML output' in result.stderr
    args.output.write_text(json.dumps(dict(checks=checks, replay_roundtrips=replay_checks, rejections=3), indent=2) + '\n')
    print(f'{len(checks)} public PNML export fixtures, {replay_checks} replay-preserving round trips, 3 rejections passed')


if __name__ == '__main__':
    main()
