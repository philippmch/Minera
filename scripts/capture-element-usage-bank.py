"""Capture the public bank for algo.comparison.petrinet.element_usage_comparison.

The 40 frozen nets of docs/reference-2.7.22-conformance-parity-nets.json.gz
get seeded pairs of logs (model words, repeated variants, unknown labels,
empty traces). Each pair goes through the public
compare_element_usage_two_logs on the PNML-imported net in fresh interpreters
under eight PYTHONHASHSEED values; a call whose output varies is run again
under 64 more and keeps every distinct output (duplicate-label choices also
follow object addresses, so some outputs are rare).

The bank is a gzip text file of tagged lines read by
minera_element_usage_comparison_parity_tests:

  N index json     a frozen net's PNML (once, before its first case)
  C name net-index
  A trace activities of log 1 ("-" alone for an empty trace)
  B trace activities of log 2
  O json   one observed output: [kind, element, log1, log2, total, label,
           direction, color] rows sorted, arcs named "source->target"

    python -I -B scripts/capture-element-usage-bank.py
"""
import argparse
import concurrent.futures
import gzip
import json
import os
from pathlib import Path
import random
import subprocess
import sys

BANK_SEED = 20261025
HASH_SEEDS = tuple(range(8))
EXTRA_SEEDS = tuple(range(8, 72))
ROOT = Path(__file__).resolve().parent.parent
NETS = ROOT / 'docs/reference-2.7.22-conformance-parity-nets.json.gz'


def generate(rounds):
    rng = random.Random(BANK_SEED)
    corpus = json.loads(gzip.decompress(NETS.read_bytes()))['cases']
    cases = []
    for net_index, net in enumerate(corpus):
        words = [list(word) for word in net['traces']]
        labels = sorted({a for word in words for a in word}) or ['a']

        def log():
            traces = []
            for _ in range(rng.randint(1, 7)):
                if traces and rng.random() < 0.2:
                    traces.append(list(rng.choice(traces)))
                elif rng.random() < 0.6 and words:
                    traces.append(list(rng.choice(words)))
                else:
                    traces.append([rng.choice(labels + ['zz']) for _ in range(rng.randint(0, 5))])
            return traces

        for round_index in range(rounds):
            cases.append({'name': f'{net["name"]}-{round_index}', 'net': net_index, 'pnml': net['model_pnml'],
                          'log1': log(), 'log2': log()})
    return cases


CHILD = r'''
import json, os, signal, sys, tempfile, warnings
warnings.filterwarnings('ignore')
os.environ['TQDM_DISABLE'] = '1'
from pm4py.objects.petri_net.importer import importer as pnml_importer
from pm4py.objects.petri_net.obj import PetriNet
from pm4py.algo.comparison.petrinet import element_usage_comparison
from pm4py.objects.log.obj import EventLog, Trace, Event
class Timeout(BaseException):
    pass
def alarm(*_):
    raise Timeout()
signal.signal(signal.SIGALRM, alarm)
LIMIT = int(sys.argv[1])
def log(traces):
    return EventLog([Trace([Event({'concept:name': a}) for a in t]) for t in traces])
def element(key):
    if isinstance(key, PetriNet.Arc):
        return 'arc', key.source.name + '->' + key.target.name
    return ('place' if isinstance(key, PetriNet.Place) else 'transition'), key.name
cases = json.load(sys.stdin)
out = []
directory = tempfile.mkdtemp()
for case in cases:
    path = os.path.join(directory, 'net.pnml')
    with open(path, 'w', encoding='utf-8') as stream:
        stream.write(case['pnml'])
    net, im, fm = pnml_importer.apply(path)
    signal.alarm(LIMIT)
    try:
        result = element_usage_comparison.compare_element_usage_two_logs(
            net, im, fm, log(case['log1']), log(case['log2']))
        value = sorted([*element(k), v['log1_occ'], v['log2_occ'], v['total_occ'], v['label'],
                        v['direction'], v['color']] for k, v in result.items())
    except Timeout:
        value = 'timeout'
    except Exception as error:
        value = ['error', type(error).__name__, str(error)]
    finally:
        signal.alarm(0)
    out.append(value)
print(json.dumps(out))
'''


def capture(cases, seed, limit):
    env = dict(os.environ, PYTHONHASHSEED=str(seed))
    result = subprocess.run([sys.executable, '-I', '-B', '-c', CHILD, str(limit)],
                            input=json.dumps(cases), capture_output=True, text=True, env=env)
    if result.returncode:
        raise SystemExit(result.stderr[-3000:])
    return json.loads(result.stdout.strip().splitlines()[-1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rounds', type=int, default=50)
    parser.add_argument('--limit', type=int, default=20)
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/element-usage-bank.txt.gz')
    args = parser.parse_args()
    cases = generate(args.rounds)
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        runs = list(pool.map(lambda seed: capture(cases, seed, args.limit), HASH_SEEDS))
    first = [[run[index] for run in runs] for index in range(len(cases))]
    again = [index for index, values in enumerate(first) if any(v != values[0] for v in values)]
    if again:
        subset = [cases[index] for index in again]
        with concurrent.futures.ThreadPoolExecutor(4) as pool:
            extra = list(pool.map(lambda seed: capture(subset, seed, args.limit), EXTRA_SEEDS))
        for position, index in enumerate(again):
            first[index] += [run[position] for run in extra]
    text, varying, timeouts, written = [], 0, 0, set()
    for index, case in enumerate(cases):
        if case['net'] not in written:
            written.add(case['net'])
            text.append(f'N {case["net"]} ' + json.dumps(case['pnml']))
        seen = []
        for value in first[index]:
            if value not in seen:
                seen.append(value)
        if 'timeout' in seen:
            timeouts += 1
            continue
        varying += len(seen) > 1
        text.append(f'C {case["name"]} {case["net"]}')
        for tag, traces in (('A', case['log1']), ('B', case['log2'])):
            text += [f'{tag} ' + (' '.join(trace) if trace else '-') for trace in traces]
        text += ['O ' + json.dumps(value, separators=(',', ':')) for value in seen]
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0, filename='') as stream:
        stream.write(('\n'.join(text) + '\n').encode())
    print(json.dumps({'cases': len(cases) - timeouts, 'varying': varying, 'timeouts': timeouts}))


if __name__ == '__main__':
    main()
