"""Capture the bank for algo.transformation.log_to_features.util.locally_linear_embedding.

smooth runs on seeded float arrays (lengths 0 to 40, box widths -1 to 15,
so both arrays can be the longer one) and records the result or the error.
apply runs on seeded event logs (1 to 120 traces of 1 to 6 events over 2 to
6 activities, whole-hour UTC timestamps with ties, events out of order inside
traces). scikit-learn 1.9.0 runs for real; its LocallyLinearEmbedding and
NearestNeighbors are only watched, to record the feature matrix the
embedding receives, the neighbour indices it uses (self column included) and
the embedding it returns, next to apply's dates and smoothed values.

The bank is a gzip text file of tagged lines read by
minera_lle_parity_tests:

  C name
  K smooth | apply
  S json   smooth: {"y": [...], "box": int}
           apply: {"traces": [[[activity, epoch seconds], ...], ...]}
  F json   smooth: {"result": [...]} or {"raises": type, "message"}
           apply: {"dates": [epoch seconds], "X": [[...]], "neighbors": [[...]],
                   "embedding": [...], "y": [...]} or {"raises": type, "message"}

    python -I -B scripts/capture-lle-bank.py --output docs/lle-bank.txt.gz
"""
import argparse
import datetime
import gzip
import json
import random
import warnings
from pathlib import Path

import numpy as np

BANK_SEED = 20261102
ROOT = Path(__file__).resolve().parent.parent
EPOCH = datetime.datetime(1970, 1, 1, tzinfo=datetime.timezone.utc)


def watch():
    import sklearn.manifold
    import sklearn.neighbors._base as neighbors_base
    seen = {}
    fit_transform = sklearn.manifold.LocallyLinearEmbedding.fit_transform

    def watched_fit_transform(self, X, y=None):
        seen['X'] = np.array(X, dtype=float).tolist()
        result = fit_transform(self, X, y)
        seen['embedding'] = np.asarray(result, dtype=float).ravel().tolist()
        return result

    kneighbors = neighbors_base.KNeighborsMixin.kneighbors

    def watched_kneighbors(self, *args, **kwargs):
        result = kneighbors(self, *args, **kwargs)
        indices = result[1] if isinstance(result, tuple) else result
        seen['neighbors'] = np.asarray(indices).tolist()
        return result

    sklearn.manifold.LocallyLinearEmbedding.fit_transform = watched_fit_transform
    neighbors_base.KNeighborsMixin.kneighbors = watched_kneighbors
    return seen


def generate(rng):
    cases = []
    for _ in range(1500):
        n = rng.choice((0, 1, 2) + tuple(range(3, 41)))
        y = [rng.choice((rng.uniform(-1, 1), float(rng.randint(-5, 5)), rng.uniform(-1e3, 1e3))) for _ in range(n)]
        cases.append(('smooth', {'y': y, 'box': rng.randint(-1, 15) if rng.random() < 0.05 else rng.randint(1, 15)}))
    for _ in range(500):
        count = rng.choice((1, 3, 5, 6, 7)) if rng.random() < 0.08 else rng.randint(6, 120)
        alphabet = 'abcdef'[:rng.randint(2, 6)]
        span = rng.choice((10, 48, 500))
        traces = []
        for _ in range(count):
            traces.append([[rng.choice(alphabet), 1704067200 + 3600 * rng.randint(0, span)]
                           for _ in range(rng.randint(1, 6))])
        cases.append(('apply', {'traces': traces}))
    return cases


def run(kind, source, seen):
    from pm4py.algo.transformation.log_to_features.util import locally_linear_embedding as lle
    from pm4py.objects.log.obj import Event, EventLog, Trace
    try:
        if kind == 'smooth':
            return {'result': lle.smooth(np.array(source['y'], dtype=float), source['box']).tolist()}
        log = EventLog()
        for index, events in enumerate(source['traces']):
            trace = Trace()
            trace.attributes['concept:name'] = f'c{index}'
            for activity, seconds in events:
                trace.append(Event({'concept:name': activity,
                                    'time:timestamp': EPOCH + datetime.timedelta(seconds=seconds)}))
            log.append(trace)
        seen.clear()
        dates, y = lle.apply(log)
        return {'dates': [int((d - EPOCH).total_seconds()) for d in dates], 'X': seen['X'],
                'neighbors': seen['neighbors'], 'embedding': seen['embedding'], 'y': np.asarray(y).tolist()}
    except Exception as error:
        return {'raises': type(error).__name__, 'message': str(error)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/lle-bank.txt.gz')
    args = parser.parse_args()
    warnings.filterwarnings('ignore')
    seen = watch()
    rng = random.Random(BANK_SEED)
    lines, summary = [], {}
    for index, (kind, source) in enumerate(generate(rng)):
        outcome = run(kind, source, seen)
        key = kind + (' raising' if 'raises' in outcome else '')
        summary[key] = summary.get(key, 0) + 1
        lines += [f'C lle-{index:04d}', f'K {kind}', 'S ' + json.dumps(source),
                  'F ' + json.dumps(outcome, separators=(',', ':'))]
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0, filename='') as stream:
        stream.write(('\n'.join(lines) + '\n').encode())
    print(json.dumps(dict(sorted(summary.items()))))


if __name__ == '__main__':
    main()
