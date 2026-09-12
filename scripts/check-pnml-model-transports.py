"""Exercise supplied PNML markings through CLI and cached HTTP model consumers.

This is transport regression evidence, not a public-importer parity claim.
Run only after exporter and server builds have finished.
"""
import argparse
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET

def pnml(places, transitions, arcs, initial, final):
    root = ET.Element('pnml')
    net = ET.SubElement(root, 'net', id='model')
    page = ET.SubElement(net, 'page', id='page')
    for name in places:
        place = ET.SubElement(page, 'place', id=name)
        ET.SubElement(ET.SubElement(place, 'name'), 'text').text = name
        if name in initial:
            ET.SubElement(ET.SubElement(place, 'initialMarking'), 'text').text = str(initial[name])
    for name, label in transitions:
        transition = ET.SubElement(page, 'transition', id=name)
        ET.SubElement(ET.SubElement(transition, 'name'), 'text').text = label or name
        if label is None:
            ET.SubElement(transition, 'toolspecific', tool='ProM', version='6.4', activity='$invisible$')
    for index, (source, target, weight) in enumerate(arcs):
        arc = ET.SubElement(page, 'arc', id=f'arc{index}', source=source, target=target)
        ET.SubElement(ET.SubElement(arc, 'inscription'), 'text').text = str(weight)
    marking = ET.SubElement(ET.SubElement(net, 'finalmarkings'), 'marking')
    for name, count in final.items():
        ET.SubElement(ET.SubElement(marking, 'place', idref=name), 'text').text = str(count)
    return ET.tostring(root, encoding='unicode')


def metadata(xml):
    root = ET.fromstring(xml)
    for node in root.iter():
        node.tag = node.tag.rsplit('}', 1)[-1]
    return {
        'places': sorted(p.attrib['id'] for p in root.iter('place') if 'id' in p.attrib),
        'transitions': sorted(t.attrib['id'] for t in root.iter('transition')),
        'initial': {p.attrib['id']: int(p.findtext('initialMarking/text'))
                    for p in root.iter('place') if p.find('initialMarking') is not None},
        'final': {p.attrib['idref']: int(p.findtext('text'))
                  for p in root.findall('.//finalmarkings/marking/place')},
    }


def multipart(name, filename, content):
    boundary = 'minera-pnml-model-transports'
    body = (f'--{boundary}\r\nContent-Disposition: form-data; name="{name}"; '
            f'filename="{filename}"\r\nContent-Type: application/octet-stream\r\n\r\n'
            f'{content}\r\n--{boundary}--\r\n').encode()
    return body, {'Content-Type': f'multipart/form-data; boundary={boundary}'}


def fixtures():
    sequence = [('source', 'a', 1), ('a', 'middle', 1),
                ('middle', 'b', 1), ('b', 'sink', 1)]
    places, transitions = ['source', 'middle', 'sink'], [('a', 'A'), ('b', 'B')]
    weighted = [('source', 'a', 2), *sequence[1:]]
    return [
        dict(name='weighted', xml=pnml(places, transitions, weighted, {'source': 2}, {'sink': 1}),
             initial={'source': 2}, final={'sink': 1}, sound=True, fit=True, relations=1),
        dict(name='multimarked_nonboundary', xml=pnml(places, transitions, sequence,
             {'source': 1, 'middle': 1}, {'middle': 1, 'sink': 1}),
             initial={'source': 1, 'middle': 1}, final={'middle': 1, 'sink': 1}, sound=True, fit=True),
        dict(name='explicit_zero', xml=pnml(places, transitions, sequence, {'source': 0}, {'sink': 0}),
             initial={'source': 0}, final={'sink': 0}, sound=True, fit=False, relations=0),
        dict(name='omitted_unique', xml=pnml(places, transitions, sequence, {}, {}),
             initial={'source': 1}, final={'sink': 1}, sound=True, fit=True, relations=1),
        dict(name='omitted_ambiguous', xml=pnml(['left', 'right'], [], [], {}, {}),
             initial={}, final={}, sound=True, fit=True, relations=0, alignment_cost=2),
        dict(name='empty', xml=pnml([], [], [], {}, {}),
             initial={}, final={}, sound=True, fit=True, relations=0, alignment_cost=2),
        dict(name='named_duplicates', xml=pnml(places,
             [('z_first_A', 'A'), ('a_second_A', 'A'), ('b', 'B')],
             [('source', 'z_first_A', 1), ('z_first_A', 'middle', 1),
              ('source', 'a_second_A', 1), ('a_second_A', 'middle', 1),
              ('middle', 'b', 1), ('b', 'sink', 1)], {'source': 1}, {'sink': 1}),
             initial={'source': 1}, final={'sink': 1}, sound=True, fit=True, relations=1),
    ]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exporter', type=Path, required=True)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    with socket.socket() as listener:
        listener.bind(('127.0.0.1', 0))
        port = listener.getsockname()[1]
    base = f'http://127.0.0.1:{port}'

    def request(path, data=None, headers=None):
        req = urllib.request.Request(base + path, data=data, headers=headers or {})
        try:
            with urllib.request.urlopen(req, timeout=90) as response:
                return response.status, response.read().decode()
        except urllib.error.HTTPError as error:
            return error.code, error.read().decode()

    def get(path, data=None, headers=None, decode=True):
        status, body = request(path, data, headers)
        assert status == 200, (path, status, body)
        return json.loads(body) if decode else body

    def run(*arguments):
        result = subprocess.run([str(args.exporter.resolve()), *map(str, arguments)],
                                capture_output=True, text=True, encoding='utf-8', timeout=120)
        assert result.returncode == 0, (arguments, result.stderr, result.stdout)
        return json.loads(result.stdout)

    checks = []
    with tempfile.TemporaryDirectory(prefix='minera-pnml-model-transports-') as temporary:
        directory = Path(temporary)
        log = directory / 'log.csv'
        log.write_text('case_id,activity,timestamp\n1,A,2024-01-01T00:00:00Z\n'
                       '1,B,2024-01-01T00:00:01Z\n', encoding='utf-8')
        reference_body, reference_headers = multipart('reference_log', log.name, log.read_text())
        source, exported = directory / 'model.pnml', directory / 'exported.pnml'
        with (directory / 'server.log').open('w', encoding='utf-8') as server_log:
            server = subprocess.Popen([str(args.server.resolve()), '--port', str(port)],
                                      stdout=server_log, stderr=server_log,
                                      creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
            try:
                for _ in range(200):
                    try:
                        if request('/api/version')[0] == 200:
                            break
                    except (OSError, urllib.error.URLError):
                        if server.poll() is not None:
                            raise RuntimeError('server exited before readiness')
                    time.sleep(0.05)
                else:
                    raise RuntimeError('server did not become ready')
                get('/api/mine', log.read_bytes(), {'Content-Type': 'text/csv'})
                discovered_before = get('/api/align?miner=inductive', b'')
                replacements = {kind: get(f'/api/{kind}?miner=inductive', decode=False)
                                for kind in ('bpmn', 'ptml')}
                for case in fixtures():
                    source.write_text(case['xml'], encoding='utf-8')
                    cli = run(log, '--check-pnml', source, '--pnml', exported, '--pnml-miner', 'imported')
                    model = cli['conformance']['imported']
                    cached = get('/api/check-model', case['xml'].encode(), {'Content-Type': 'application/xml'})['model']
                    assert cached == {k: v for k, v in model.items() if k != 'alignments'}, case['name']
                    assert model['sound'] is case['sound'], (case['name'], model)
                    assert (model['token_replay']['fitting_traces'] == 1) is case['fit'], (case['name'], model)
                    if 'relations' in case:
                        assert model['footprints']['model_relations'] == case['relations'], (case['name'], model)
                    assert get('/api/align?miner=imported', b'') == model['alignments'], case['name']
                    if case['fit']:
                        # Replay ignores unknown labels by default; alignment
                        # must still pay for them as log moves on an empty net.
                        assert model['alignments']['computed'], case['name']
                        assert model['alignments']['total_cost'] == case.get('alignment_cost', 0), case['name']
                    expected = metadata(case['xml'])
                    expected.update(initial=case['initial'], final=case['final'])
                    assert metadata(exported.read_text(encoding='utf-8')) == expected, case['name']
                    server_xml = get('/api/pnml?miner=imported', decode=False)
                    assert metadata(server_xml) == expected, case['name']
                    assert get('/api/check-model', server_xml.encode(), {'Content-Type': 'application/xml'})['model'] == cached, case['name']
                    # Standalone replay takes raw markings. Use the exported document so
                    # legacy boundary inference has already been made explicit.
                    traces = run(log, '--token-replay-model', exported)['traces']
                    for key in ('produced', 'consumed', 'missing', 'remaining'):
                        assert model['token_replay'][key] == sum(t[key] for t in traces), (case['name'], key)
                    assert model['token_replay']['fitting_traces'] == sum(t['trace_is_fit'] for t in traces), case['name']
                    comparison = get('/api/compare-petri?miner=imported', reference_body, reference_headers)
                    assert comparison == run(log, '--compare-petri-model', exported,
                                             '--compare-petri-reference', log), case['name']
                    assert comparison == run(log, '--check-pnml', source, '--compare-petri-miner', 'imported',
                                             '--compare-petri-reference', log), case['name']
                    checks.append(dict(name=case['name'], replay=True, alignment=True,
                                       reachability=True, footprints=True, comparison=True, roundtrip=True))
                    if case['name'] == 'weighted':
                        # Matching explicit vectors are independent controls for
                        # consumers which expose markings but not replay details.
                        consumers = [
                            ('/api/evaluate', {'miner': 'imported', 'evaluation_family': 'replay_fitness'}, 'evaluation_'),
                            ('/api/petri-analysis', {'petri_analysis_miner': 'imported',
                             'petri_analysis_family': 'marking_equation'}, 'petri_analysis_'),
                            ('/api/align-decomposed', {'miner': 'imported'}, ''),
                            ('/api/align-petri-anti', {'miner': 'imported'}, ''),
                            ('/api/align-petri-multi', {'miner': 'imported'}, ''),
                            ('/api/playout', {'miner': 'imported', 'traces': '1', 'seed': '7',
                             'return_visited_elements': 'true'}, ''),
                            ('/api/playout', {'miner': 'imported', 'traces': '1', 'mode': 'extensive',
                             'return_elements': 'true'}, ''),
                        ]
                        for route, params, prefix in consumers:
                            implicit = get(route + '?' + urllib.parse.urlencode(params), b'')
                            explicit = dict(params, **{prefix + 'initial_marking': '2,0,0',
                                                       prefix + 'final_marking': '0,0,1'})
                            assert get(route + '?' + urllib.parse.urlencode(explicit), b'') == implicit, (route, params)
                            checks.append(dict(name=route + ':' + params.get('mode', 'default'),
                                               explicit_markings_match=True))
                        for flag, key, prefix in [
                            ('--align-decomposed', 'decomposed_alignment', '--decomposed-'),
                            ('--align-petri-anti', 'petri_anti_alignment', '--petri-'),
                            ('--align-petri-multi', 'petri_multi_alignment', '--petri-'),
                        ]:
                            arguments = [log, '--check-pnml', source, flag,
                                         '--advanced-petri-miner', 'imported']
                            implicit = run(*arguments)[key]
                            explicit = run(*arguments, prefix + 'initial-marking', '2,0,0',
                                           prefix + 'final-marking', '0,0,1')[key]
                            assert explicit == implicit, flag
                            checks.append(dict(name=flag, explicit_markings_match=True))
                        for mode, elements in [('basic', 'visited-elements'), ('extensive', 'elements')]:
                            destination = directory / 'playout.json'
                            arguments = [log, '--check-pnml', source, '--playout', destination,
                                '--playout-miner', 'imported', '--petri-playout-mode', mode,
                                '--petri-playout-return-' + elements, 'true']
                            run(*arguments)
                            implicit = json.loads(destination.read_text(encoding='utf-8'))
                            run(*arguments, '--petri-playout-initial-marking', '2,0,0',
                                '--petri-playout-final-marking', '0,0,1')
                            assert json.loads(destination.read_text(encoding='utf-8')) == implicit, mode
                            checks.append(dict(name='cli_playout_' + mode, explicit_markings_match=True))
                        baseline = comparison
                        # A previously unkeyed place becomes positive, detecting
                        # stale inherited key arrays after a numeric override.
                        for which, vector in [('initial', '4,1,0'), ('final', '0,1,2')]:
                            params = urllib.parse.urlencode({'miner': 'imported', which + '_marking': vector})
                            overridden = get('/api/compare-petri?' + params, reference_body, reference_headers)
                            expected_comparison = run(log, '--compare-petri-model', exported,
                                '--compare-petri-reference', log, f'--compare-petri-{which}-marking', vector)
                            assert overridden == expected_comparison, which
                            assert overridden != baseline, ('override had no effect', which)
                            assert overridden == run(log, '--check-pnml', source,
                                '--compare-petri-miner', 'imported', '--compare-petri-reference', log,
                                f'--compare-petri-{which}-marking', vector), which
                            assert metadata(get('/api/pnml?miner=imported', decode=False)) == expected, which
                            checks.append(dict(name=which + '_override', matched=True, cache_preserved=True))
                # Replacing PNML must discard its document sidecar. Each replacement
                # follows a fresh explicit-zero document to expose stale markings.
                zero = next(case for case in fixtures() if case['name'] == 'explicit_zero')
                for kind, xml in replacements.items():
                    get('/api/check-model', zero['xml'].encode(), {'Content-Type': 'application/xml'})
                    replacement = directory / ('replacement.' + kind)
                    replacement.write_text(xml, encoding='utf-8')
                    expected_model = run(log, '--check-' + kind, replacement)['conformance']['imported']
                    cached = get('/api/check-model', xml.encode(), {'Content-Type': 'application/xml'})['model']
                    assert cached == {k: v for k, v in expected_model.items() if k != 'alignments'}, kind
                    assert get('/api/align?miner=imported', b'') == expected_model['alignments'], kind
                    markings = metadata(get('/api/pnml?miner=imported', decode=False))
                    assert sum(markings['initial'].values()) == sum(markings['final'].values()) == 1, kind
                    checks.append(dict(name=kind + '_replacement', stale_metadata_cleared=True))
                assert get('/api/align?miner=inductive', b'') == discovered_before
                checks.append(dict(name='discovered_defaults', unchanged=True))
                preserved = metadata(get('/api/pnml?miner=imported', decode=False))
                for name, invalid in [
                    ('out_of_range_marking', pnml(['p'], [], [], {'p': 2147483648}, {'p': 0})),
                    ('firing_overflow', pnml(['p', 'q'], [('a', 'A')],
                         [('q', 'a', 1), ('a', 'p', 1)],
                         {'p': 2147483647, 'q': 1}, {'p': 0, 'q': 0})),
                ]:
                    status, body = request('/api/check-model', invalid.encode(),
                                           {'Content-Type': 'application/xml'})
                    assert status == 400 and 'token range' in body, (name, status, body)
                    source.write_text(invalid, encoding='utf-8')
                    rejected = subprocess.run([str(args.exporter.resolve()), str(log),
                        '--check-pnml', str(source)], capture_output=True,
                        text=True, encoding='utf-8', timeout=120)
                    assert rejected.returncode != 0 and 'token range' in rejected.stderr, name
                    assert metadata(get('/api/pnml?miner=imported', decode=False)) == preserved, name
                    checks.append(dict(name=name, cli_rejected=True,
                                       server_rejected=True, cache_preserved=True))
            finally:
                if server.poll() is None:
                    server.terminate()
                    try:
                        server.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        server.kill()
                        server.wait(timeout=10)
                if server.returncode not in (0, None) and not checks:
                    print((directory / 'server.log').read_text(encoding='utf-8', errors='replace'))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps({'checks': checks}, indent=2) + '\n', encoding='utf-8')
    print(f'{len(checks)} supplied-PNML transport checks passed')


if __name__ == '__main__':
    main()
