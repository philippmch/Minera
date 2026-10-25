"""Public PM4Py 2.7.22 token replay root-cause capture; no native calls.

The 40 frozen nets of docs/reference-2.7.22-conformance-parity-nets.json.gz
get seeded logs whose events carry a string `org:resource`, an integer
`amount` and a float `cost` (repeated variants, unknown labels, empty traces
and value-equal trace copies). Each log is replayed by the public forward
token replay with ENABLE_PLTR_FITNESS, and its transition fitness and unknown
activity maps are passed to the public
`tokenreplay.diagnostics.root_cause_analysis.diagnose_from_trans_fitness`
and `diagnose_from_notexisting_activities` with random string/numeric
attribute selections and ENABLE_MULTIPLIER. Classifier dependency:
scikit-learn 1.9.0, as in docs/token-replay-root-cause-fixtures.json.

Per model the bank keeps the feature names, data matrix, target and classes
and from the fitted tree the root's class counts and Gini gain (equal for
every optimal root split). The tree's equal-score feature choice is unseeded,
so its other structure is not kept. Replay can break duplicate-label ties by
hash order, so each case runs under sixteen PYTHONHASHSEED values and keeps
every distinct outcome.

    python -I -B scripts/capture-root-cause-bank.py \
        --output docs/token-replay-root-cause-bank.json.gz
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
import tempfile
import warnings

BANK_SEED = 20261025
HASH_SEEDS = tuple(range(16))
NETS = Path(__file__).resolve().parent.parent / 'docs/reference-2.7.22-conformance-parity-nets.json.gz'
STRINGS = ('org:resource', 'concept:name')
NUMERICS = ('amount', 'cost')


def generate():
    rng = random.Random(BANK_SEED)
    corpus = json.loads(gzip.decompress(NETS.read_bytes()))['cases']
    cases = []
    for net in corpus:
        words = [list(word) for word in net['traces']]
        labels = sorted({a for word in words for a in word}) or ['a']
        for round_index in range(6):
            traces = []
            for index in range(rng.randint(4, 12)):
                if traces and rng.random() < 0.15:
                    traces.append(json.loads(json.dumps(rng.choice(traces))))
                    continue
                if rng.random() < 0.6 and words:
                    activities = list(rng.choice(words))
                else:
                    activities = [rng.choice(labels + ['zz']) for _ in range(rng.randint(0, 5))]
                events = [{'concept:name': a, 'org:resource': rng.choice(('R', 'S', 'T')),
                           'amount': rng.randint(0, 9), 'cost': rng.choice((0.5, 1.25, 2.0, 3.75))}
                          for a in activities]
                traces.append({'key': f'k{index}', 'events': events})
            strings = [s for s in STRINGS if rng.random() < 0.5]
            numerics = [n for n in NUMERICS if rng.random() < 0.5]
            cases.append({'name': f'{net["name"]}-{round_index}', 'pnml': net['model_pnml'],
                          'traces': traces, 'string_attributes': strings,
                          'numeric_attributes': numerics, 'enable_multiplier': rng.random() < 0.3})
    return cases


def models(result):
    out = []
    for key, model in result.items():
        tree = model['clf'].tree_
        counts = [float(x) for x in tree.value[0][0]]
        total = int(tree.n_node_samples[0])
        gain = 0.0
        if tree.children_left[0] != -1:
            left, right = tree.children_left[0], tree.children_right[0]
            gain = float(tree.impurity[0]) - (tree.n_node_samples[left] * float(tree.impurity[left]) +
                                              tree.n_node_samples[right] * float(tree.impurity[right])) / total
        out.append({'key': key if isinstance(key, str) else key.name,
                    'feature_names': list(model['feature_names']),
                    'data': [[float(x) for x in row] for row in model['data']],
                    'target': [int(x) for x in model['target']],
                    'classes': list(model['classes']),
                    'root_samples': total, 'root_fractions': counts,
                    'root_impurity': float(tree.impurity[0]), 'root_gain': round(gain, 12)})
    return sorted(out, key=lambda item: item['key'])


def call(function):
    try:
        return {'models': models(function())}
    except Exception as error:
        return {'error': type(error).__name__}


def run(cases):
    """Diagnoses every case in this interpreter; returns one outcome pair per case."""
    warnings.filterwarnings('ignore')
    from pm4py.objects.petri_net.importer import importer as pnml_importer
    from pm4py.algo.conformance.tokenreplay.variants import token_replay as tr
    from pm4py.algo.conformance.tokenreplay.diagnostics import root_cause_analysis as rca
    from pm4py.objects.log.obj import EventLog, Trace, Event
    directory = tempfile.mkdtemp()
    out = []
    for case in cases:
        path = os.path.join(directory, 'net.pnml')
        with open(path, 'w', encoding='utf-8') as stream:
            stream.write(case['pnml'])
        net, im, fm = pnml_importer.apply(path)
        log = EventLog([Trace([Event(dict(event)) for event in trace['events']],
                              attributes={'concept:name': trace['key']}) for trace in case['traces']])
        _, _, transitions, unknown = tr.apply(log, net, im, fm,
                                              parameters={tr.Parameters.ENABLE_PLTR_FITNESS: True})
        R = rca.Parameters
        parameters = {R.STRING_ATTRIBUTES: case['string_attributes'],
                      R.NUMERIC_ATTRIBUTES: case['numeric_attributes'],
                      R.ENABLE_MULTIPLIER: case['enable_multiplier']}
        out.append({'transitions': call(lambda: rca.diagnose_from_trans_fitness(log, transitions, parameters=dict(parameters))),
                    'unknown': call(lambda: rca.diagnose_from_notexisting_activities(log, unknown, parameters=dict(parameters)))})
    return out


def capture(seed):
    env = dict(os.environ, PYTHONHASHSEED=str(seed))
    result = subprocess.run([sys.executable, '-I', '-B', os.path.abspath(__file__), '--child'],
                            capture_output=True, text=True, env=env)
    if result.returncode:
        raise SystemExit(result.stderr[-3000:])
    return json.loads(result.stdout.strip().splitlines()[-1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path)
    parser.add_argument('--child', action='store_true')
    args = parser.parse_args()
    if args.child:
        print(json.dumps(run(generate())))
        return
    import pm4py
    import sklearn
    cases = generate()
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        runs = list(pool.map(capture, HASH_SEEDS))
    summary = {}
    for index, case in enumerate(cases):
        for form in ('transitions', 'unknown'):
            seen = []
            for run_result in runs:
                if run_result[index][form] not in seen:
                    seen.append(run_result[index][form])
            case[form] = seen[0] if len(seen) == 1 else {'observed': seen}
            kind = 'observed' if len(seen) > 1 else 'error' if 'error' in seen[0] else 'models'
            summary[kind] = summary.get(kind, 0) + 1
    bank = {'reference': f'pm4py {pm4py.__version__}', 'classifier': f'scikit-learn {sklearn.__version__}',
            'bank_seed': BANK_SEED, 'hash_seeds': list(HASH_SEEDS), 'cases': cases}
    data = json.dumps(bank, indent=0, sort_keys=True).encode('utf-8')
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0,
                                                      compresslevel=9, filename='') as stream:
        stream.write(data)
    print(f'{len(cases)} cases, outcomes {summary}')


if __name__ == '__main__':
    main()
