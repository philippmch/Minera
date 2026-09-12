"""Check server/CLI element-usage transport agreement using exported cached nets."""
import argparse
import itertools
import json
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path


def multipart(name, filename, content):
    boundary = 'minera-petri-comparison-test'
    body = (f'--{boundary}\r\nContent-Disposition: form-data; name="{name}"; '
            f'filename="{filename}"\r\nContent-Type: application/octet-stream\r\n\r\n'
            f'{content}\r\n--{boundary}--\r\n').encode()
    return body, {'Content-Type': f'multipart/form-data; boundary={boundary}'}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--native', type=Path, required=True)
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
            with urllib.request.urlopen(req, timeout=30) as response:
                return response.status, response.read().decode()
        except urllib.error.HTTPError as error:
            return error.code, error.read().decode()

    def csv(traces):
        return 'case_id,activity,timestamp\n' + ''.join(
            f'{case},{activity},2024-01-01T00:00:{event:02d}Z\n'
            for case, trace in enumerate(traces) for event, activity in enumerate(trace))

    def xes(traces):
        return '<log>' + ''.join(f'<trace><string key="concept:name" value="{case}"/>' + ''.join(
            f'<event><string key="concept:name" value="{activity}"/>'
            f'<date key="time:timestamp" value="2024-01-01T00:00:{event:02d}Z"/></event>'
            for event, activity in enumerate(trace)) + '</trace>' for case, trace in enumerate(traces)) + '</log>'

    checks = []
    cleaning_effects = 0
    with tempfile.TemporaryDirectory(prefix='minera-usage-transports-') as directory:
        directory = Path(directory)
        with (directory / 'server.log').open('w', encoding='utf-8') as server_log:
            server = subprocess.Popen([str(args.server.resolve()), '--port', str(port)],
                                      stdout=server_log, stderr=server_log,
                                      creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
            try:
                for _ in range(100):
                    try:
                        if request('/api/version')[0] == 200:
                            break
                    except (OSError, urllib.error.URLError):
                        if server.poll() is not None:
                            raise RuntimeError('comparison server exited before readiness')
                    time.sleep(0.05)
                else:
                    raise RuntimeError('comparison server did not become ready')
                body, headers = multipart('reference_log', 'reference.xes', xes([['A', 'B']]))
                assert request('/api/compare-petri', body, headers)[0] == 409
                source = directory / 'source.csv'
                source.write_text(csv([['A', 'B']] * 2), encoding='utf-8')
                status, response = request('/api/mine', source.read_bytes(), {'Content-Type': 'text/csv'})
                assert status == 200, response
                for miner in ('inductive', 'alpha', 'heuristic', 'powl'):
                    status, pnml = request('/api/pnml?' + urllib.parse.urlencode({'miner': miner}))
                    assert status == 200, pnml
                    model = directory / 'model.pnml'
                    model.write_text(pnml, encoding='utf-8')
                    for extension, convert in [('csv', csv), ('xes', xes)]:
                        reference = directory / ('reference.' + extension)
                        reference.write_text(convert([['A', 'B']] * 4 + [['B', 'A']]), encoding='utf-8')
                        body, headers = multipart('reference_log', reference.name, reference.read_text(encoding='utf-8'))
                        for walk, finish, stop, exhaustive, clean in itertools.product((False, True), repeat=5):
                            params = dict(miner=miner, walk_through_hidden_trans=str(walk).lower(),
                                          try_to_reach_final_marking_through_hidden=str(finish).lower(),
                                          stop_immediately_unfit=str(stop).lower(),
                                          exhaustive_invisible_exploration=str(exhaustive).lower(),
                                          cleaning_token_flood=str(clean).lower())
                            status, response = request('/api/compare-petri?' + urllib.parse.urlencode(params), body, headers)
                            assert status == 200, response
                            command = [str(args.native.resolve()), str(source), '--compare-petri-model', str(model),
                                       '--compare-petri-reference', str(reference), '--compare-petri-walk-through-hidden', str(walk).lower(),
                                       '--compare-petri-reach-final-through-hidden', str(finish).lower(),
                                       '--compare-petri-stop-immediately-unfit', str(stop).lower(),
                                       '--compare-petri-exhaustive-invisible-exploration', str(exhaustive).lower(),
                                       '--compare-petri-cleaning-token-flood', str(clean).lower()]
                            native = subprocess.run(command, capture_output=True, text=True, encoding='utf-8')
                            assert native.returncode == 0, native.stderr
                            assert json.loads(response) == json.loads(native.stdout), (params, response, native.stdout)
                            if clean:
                                if json.loads(response) != uncleaned: cleaning_effects += 1
                            else:
                                uncleaned = json.loads(response)
                            checks.append(dict(miner=miner, format=extension, walk=walk, finish=finish, stop=stop,
                                               exhaustive=exhaustive, clean=clean))
                assert cleaning_effects > 0, 'cleaning never changed a transported result'
                native = subprocess.run([str(args.native.resolve()), str(source), '--compare-petri-model', str(model),
                    '--compare-petri-reference', str(reference)], capture_output=True, text=True, encoding='utf-8')
                assert native.returncode == 0, native.stderr
                custom_expected = json.loads(native.stdout)
                custom_source, custom_reference = directory / 'custom-source.csv', directory / 'custom-reference.csv'
                custom_source.write_text(csv([['A', 'B']] * 2).replace('case_id,activity,', 'case_id,step,'), encoding='utf-8')
                custom_reference.write_text(csv([['A', 'B']] * 4 + [['B', 'A']]).replace('case_id,activity,', 'case_id,step,'), encoding='utf-8')
                for selector in ('activity', 'step'):
                    native = subprocess.run([str(args.native.resolve()), str(custom_source), '--activity-column', 'step',
                        '--compare-petri-model', str(model), '--compare-petri-reference', str(custom_reference),
                        '--compare-petri-activity-attribute', selector], capture_output=True, text=True, encoding='utf-8')
                    assert native.returncode == 0, native.stderr
                    assert custom_expected == json.loads(native.stdout), (selector, custom_expected, native.stdout)
                for params in ({'miner': 'missing'}, {'activity_attribute': ' '}, {'stop_immediately_unfit': 'invalid'},
                               {'cleaning_token_flood': 'invalid'},
                               {'exhaustive_invisible_exploration': 'invalid'},
                               {'exhaustive_invisible_exploration': ''}):
                    status, response = request('/api/compare-petri?' + urllib.parse.urlencode(params), body, headers)
                    assert status == 400, response
                assert request('/api/compare-petri', b'')[0] == 400
                for extra in ([], ['--compare-petri-reference', str(reference), '--compare-petri-miner', 'alpha'],
                              ['--compare-petri-reference', str(reference), '--compare-petri-cleaning-token-flood', 'invalid'],
                              ['--compare-petri-reference', str(reference), '--compare-petri-exhaustive-invisible-exploration', 'invalid'],
                              ['--compare-petri-reference', str(reference), '--compare-petri-exhaustive-invisible-exploration'],
                              ['--compare-petri-reference', str(reference), '--compare-petri-cleaning-token-flood']):
                    native = subprocess.run([str(args.native.resolve()), str(source), '--compare-petri-model', str(model), *extra],
                                            capture_output=True, text=True, encoding='utf-8')
                    assert native.returncode != 0, native.stdout
            finally:
                server.terminate()
                server.wait(timeout=10)
    args.output.write_text(json.dumps(dict(checks=checks, cleaning_effects=cleaning_effects, custom_selector_checks=2, rejection_checks=14), indent=2) + '\n', encoding='utf-8')
    print(f'{len(checks)} server/CLI comparisons, {cleaning_effects} observed cleaning effects, 2 custom-selector checks and 14 rejection checks passed')


if __name__ == '__main__':
    main()
