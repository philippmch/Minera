"""Capture the bank for algo.transformation.to_embeddings.

sentence_transformers is replaced by a stub model whose encode returns, per
sentence, four float32 values (md5 bytes minus 128, over 64): dyadic, so every
dot product and sum of squares is exact in float32 and the order BLAS sums in
cannot show. Seeded cases call embed_sentence.apply, similarity.apply (a
sentence or a list target; embeddings as float32 arrays or lists of numbers),
cases_transformers and events_transformers apply and keep_top_k_per_similarity
(k of 0, 1, 2, 3, 10 and -1, with the transformers' own embeddings or given
identifiers and embeddings, keep_cases on and off) on small logs with
repeated sentences (so ties are frequent), duplicate event identifiers,
empty logs and missing columns. Every encode call is recorded with the
model, the sentences and the vectors returned.

The bank is a gzip text file of tagged lines read by
minera_embedding_parity_tests:

  C name
  K function (embed, similarity, cases, events, top_cases, top_events)
  S json   the inputs: {"sentence"} / {"target", "embeddings"} (a float32
           array as {"f32": [...]}) / {"log": {column: values}, "k",
           "target", "given": [identifiers, embeddings] or null} plus
           "params"
  F json   {"encodes": [[model, sentences, vectors], ...]} plus "result"
           (the vector; [value, dtype] per similarity; [identifiers,
           vectors]; the kept row positions) or "raises"
"""
import argparse
import gzip
import hashlib
import io
import contextlib
import json
from pathlib import Path
import random
import sys
import types
import warnings

import numpy as np
import pandas as pd

warnings.filterwarnings('ignore')
ENCODES = []


class SentenceTransformer:
    def __init__(self, name, *args, **kwargs):
        self.name = name

    def encode(self, sentences, *args, **kwargs):
        def vector(sentence):
            digest = hashlib.md5(sentence.encode()).digest()
            return np.array([(b - 128) / 64 for b in digest[:4]], dtype=np.float32)
        single = isinstance(sentences, str)
        batch = [sentences] if single else list(sentences)
        vectors = np.stack([vector(s) for s in batch]) if batch else np.zeros((0, 4), dtype=np.float32)
        ENCODES.append([self.name, batch, vectors.tolist()])
        return vectors[0] if single else vectors


stub = types.ModuleType('sentence_transformers')
stub.SentenceTransformer = SentenceTransformer
sys.modules['sentence_transformers'] = stub
from pm4py.algo.transformation.to_embeddings.util import embed_sentence, similarity
from pm4py.algo.transformation.to_embeddings.variants import cases_transformers, events_transformers

BANK_SEED = 20261032
ROOT = Path(__file__).resolve().parent.parent
R = random.Random(BANK_SEED)


def seeded_log():
    n = R.randint(0, 7)
    cases = R.sample(['a', 'b', 'c', 'd', 'e'], R.randint(1, 4))
    columns = {'case:concept:name': [R.choice(cases) for _ in range(n)],
               'concept:name': [R.choice(['x', 'y', 'z', 'x y', '', ' ']) for _ in range(n)],
               'org:resource': [R.choice(['r', 's', 't']) for _ in range(n)]}
    if R.random() < 0.85:
        columns['@@index'] = R.sample(range(100), n) if R.random() < 0.7 else [R.randint(0, 3) for _ in range(n)]
    return columns


def vector():
    return [R.randint(-4, 4) / 4 for _ in range(4)]


def capture(index):
    kind = R.choice(['embed', 'similarity', 'cases', 'events', 'top_cases', 'top_events'])
    parameters = {}
    if R.random() < 0.3:
        parameters['embedding_model'] = R.choice(['mx', 'all-MiniLM-L6-v2'])
    source = {'params': parameters}
    ENCODES.clear()
    outcome = {}
    try:
        with contextlib.redirect_stderr(io.StringIO()):
            if kind == 'embed':
                source['sentence'] = R.choice(['hello', 'x', '', 'a b', 'ü'])
                outcome['result'] = [float(v) for v in embed_sentence.apply(source['sentence'], parameters)]
            elif kind == 'similarity':
                target = R.choice(['x', 'y', 'hello']) if R.random() < 0.5 else vector()
                source['target'], source['embeddings'], given = target, [], []
                for _ in range(R.randint(0, 4)):
                    v = vector()
                    if R.random() < 0.5:
                        given.append(np.array(v, dtype=np.float32))
                        source['embeddings'].append({'f32': v})
                    else:
                        given.append(v)
                        source['embeddings'].append(v)
                outcome['result'] = [[float(v), type(v).__name__] for v in similarity.apply(target, given, parameters)]
            else:
                log = seeded_log()
                source['log'] = log
                frame = pd.DataFrame(log)
                if R.random() < 0.3:
                    parameters['pm4py:param:attribute_key'] = 'org:resource'
                if 'events' in kind and R.random() < 0.2:
                    parameters['event_id_key'] = 'org:resource'
                if 'events' in kind and R.random() < 0.3:
                    parameters['keep_cases'] = R.choice([True, False])
                module = cases_transformers if 'cases' in kind else events_transformers
                if kind in ('cases', 'events'):
                    ids, embeddings = module.apply(frame, parameters)
                    outcome['result'] = [[i if isinstance(i, str) else int(i) for i in ids],
                                         [[float(v) for v in e] for e in embeddings]]
                else:
                    source['k'] = R.choice([0, 1, 2, 3, 10, -1])
                    source['target'] = R.choice(['x', 'y z', 'q'])
                    source['given'] = None
                    arguments = {}
                    if R.random() < 0.2:
                        key = 'case:concept:name' if 'cases' in kind else parameters.get('event_id_key', '@@index')
                        values = sorted(set(log.get(key, []))) if 'cases' in kind else list(log.get(key, []))
                        embeddings = [vector() for _ in values]
                        as_arrays = R.random() < 0.5
                        given = [np.array(e, dtype=np.float32) for e in embeddings] if as_arrays else embeddings
                        arguments = {'cases_identifiers' if 'cases' in kind else 'event_identifiers': values,
                                     'embeddings_list': given}
                        source['given'] = [values, [{'f32': e} for e in embeddings] if as_arrays else embeddings]
                    kept = module.keep_top_k_per_similarity(frame, source['target'], source['k'], parameters=parameters,
                                                            **arguments)
                    outcome['result'] = [int(i) for i in kept.index]
    except Exception as e:
        outcome = {'raises': type(e).__name__}
    outcome['encodes'] = [list(e) for e in ENCODES]
    return f'{kind}{index}', kind, source, outcome


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--count', type=int, default=3000)
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/embedding-bank.txt.gz')
    args = parser.parse_args()
    lines, summary = [], {}
    for index in range(args.count):
        name, kind, source, outcome = capture(index)
        lines += [f'C {name}', f'K {kind}', 'S ' + json.dumps(source, ensure_ascii=False),
                  'F ' + json.dumps(outcome, ensure_ascii=False, separators=(',', ':'))]
        key = kind + (' raising' if 'raises' in outcome else '')
        summary[key] = summary.get(key, 0) + 1
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0, filename='') as stream:
        stream.write(('\n'.join(lines) + '\n').encode())
    print(json.dumps(dict(sorted(summary.items()))))


if __name__ == '__main__':
    main()
