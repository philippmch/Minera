"""Capture the bank for algo.querying.llm.connectors (anthropic, openai, google).

requests is replaced by a stub whose post records the call and answers with
a seeded response; each connector's apply runs on a seeded prompt and
parameters (api_url with and without a trailing '/', the empty url and urls
naming api.openai.com elsewhere, api keys including None and '', models,
max_tokens including None and 0, thinking budgets, use_responses_api as
bools, ints and None, extra_payload overriding or adding keys, and images
with upper- and lower-case, double and missing extensions or a missing
file). Responses are mostly well-formed for the connector, the rest mixes
other providers' shapes, API errors (with and without a message), empty
lists and missing keys.

The bank is a gzip text file of tagged lines read by
minera_llm_connector_parity_tests:

  C name
  K connector
  S json   {"prompt", "params", "response", "images"}: images maps each
           image file name to its content (image_path names one of them)
  F json   {"calls": [{"url", "headers", "json", "timeout"?}, ...]} plus
           {"result": answer} or {"raises": ExceptionType, "message"}
"""
import argparse
import contextlib
import copy
import gzip
import io
import json
import os
from pathlib import Path
import random
import sys
import tempfile
import types

requests = types.ModuleType('requests')
sys.modules['requests'] = requests
from pm4py.algo.querying.llm.connectors import anthropic, google, openai

BANK_SEED = 20261031
ROOT = Path(__file__).resolve().parent.parent
R = random.Random(BANK_SEED)
IMAGES = {'img.png': 'abc', 'pic.JPEG': 'xyz', 'a.gif': 'gif89', 'a.webp': 'RIFF', 'a.jpg': 'jpeg!', 'a.bmp': 'BM',
          'noext': 'raw', 'a.PNG': 'PNG', 'a.tar.gz': 'tgz', '.hidden': 'h'}
CONNECTORS = {'anthropic': anthropic, 'openai': openai, 'google': google}


def maybe(parameters, key, choices, p=0.5):
    if R.random() < p:
        parameters[key] = R.choice(choices)


def parameters_for(name):
    p = {}
    maybe(p, 'api_key', ['k', '', None, 'sk-1'], 0.95 if name == 'google' else 0.5)
    maybe(p, 'image_path', list(IMAGES) + ['missing.png', None], 0.3)
    maybe(p, 'extra_payload', [{}, {'temperature': 0}, {'max_tokens': 7}, {'messages': []}, {'model': 'over', 'x': [1, 2]},
                               {'generationConfig': {'t': 1}}, None], 0.3)
    if name == 'anthropic':
        maybe(p, 'api_url', ['https://h/x/', 'https://h/x', ''], 0.3)
        maybe(p, 'anthropic_model', ['m', 'claude-x'], 0.3)
        maybe(p, 'max_tokens', [5, None, 0], 0.3)
        maybe(p, 'thinking_tokens', [0, 3, None, 1024], 0.3)
    if name == 'openai':
        maybe(p, 'api_url', ['https://h/v/', 'https://h/v', '', 'https://api.openai.com/v1/', 'https://api.openai.com/v2',
                             'http://localhost/api.openai.com/'], 0.3)
        maybe(p, 'openai_model', ['m', 'gpt-x'], 0.3)
        maybe(p, 'max_tokens', [5, None, 0], 0.3)
        maybe(p, 'use_responses_api', [True, False, None, 0, 1], 0.5)
    if name == 'google':
        maybe(p, 'google_model', ['m', 'gemini-x'], 0.3)
    return p


def text_block():
    return R.choice([{'type': 'text', 'text': R.choice(['A', 'B', ''])}] * 4 +
                    [{'type': 'thinking', 'thinking': 't'}, {'text': 'x'}, {'type': 'text'}, {}])


def valid_response(name):
    if name == 'anthropic':
        return {'content': [text_block() for _ in range(R.randint(1, 3))]}
    if name == 'google':
        return {'candidates': [{'content': {'parts': [R.choice([{'text': R.choice(['G', 'H'])}] * 3 + [{}])
                                                      for _ in range(R.randint(1, 2))]}} for _ in range(R.randint(1, 2))]}
    d = {}
    if R.random() < 0.7:
        d['output'] = [R.choice([{'type': 'reasoning'}, {'type': 'message', 'content': [{'type': 'output_text', 'text': R.choice(['M', 'N'])}]},
                                 {'type': 'message', 'content': [{'type': 'output_text', 'text': 'P'}, {'type': 'output_text', 'text': 'Q'}]},
                                 {'type': 'message', 'content': []}, {'type': 'message', 'content': [{'type': 'refusal', 'refusal': 'no'}]},
                                 {'type': 'message'}, {}]) for _ in range(R.randint(1, 3))]
    if R.random() < 0.7:
        d['choices'] = [{'message': {'content': R.choice(['C', 'D', None, ''])}} for _ in range(R.randint(1, 2))]
    if R.random() < 0.1:
        d['output_text'] = 'OT'
    return d


def other_block():
    return R.choice([{'type': 'text', 'text': R.choice(['A', 'B', ''])}, {'type': 'thinking', 'thinking': 't'}, {'text': 'x'},
                     {}, 's', {'type': 'message', 'content': [{'type': 'output_text', 'text': 'M'}]}, {'type': 'reasoning'},
                     {'content': {'parts': [{'text': 'G'}]}}, {'content': {'parts': []}}, {'content': {}},
                     {'message': {'content': R.choice(['C', None, ''])}}, {'message': {}}])


def response_for(name):
    if R.random() < 0.75:
        return valid_response(name)
    if R.random() < 0.15:
        return R.choice([{'error': {'message': 'bad'}}, {'error': 'oops'}, {'error': {}}, {'error': {'message': 3}}, {}, [], None,
                         'text', ['error']])
    d = {}
    for key in ['content', 'output', 'candidates', 'choices']:
        if R.random() < 0.6:
            d[key] = [other_block() for _ in range(R.randint(0, 3))]
    if R.random() < 0.05:
        d['error'] = {'message': 'bad'}
    return d


def capture(name, index, directory):
    parameters = parameters_for(name)
    response = response_for(name)
    prompt = R.choice(['hi', 'a "q"\nb', 'ü', ''])
    calls = []

    def post(url, *args, **kwargs):
        call = {'url': url, 'headers': kwargs.get('headers'), 'json': kwargs.get('json')}
        if 'timeout' in kwargs:
            call['timeout'] = kwargs['timeout']
        calls.append(copy.deepcopy(call))
        return types.SimpleNamespace(json=lambda: copy.deepcopy(response), status_code=200, text=json.dumps(response))

    requests.post = post
    given = copy.deepcopy(parameters)
    if given.get('image_path') is not None:
        given['image_path'] = os.path.join(directory, given['image_path'])
    outcome = {}
    try:
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            outcome['result'] = CONNECTORS[name].apply(prompt, given)
    except Exception as e:
        outcome = {'raises': type(e).__name__, 'message': str(e) if type(e) is Exception else ''}
    outcome['calls'] = calls
    image = parameters.get('image_path')
    images = {image: IMAGES[image]} if image in IMAGES else {}
    return f'{name}{index}', name, {'prompt': prompt, 'params': parameters, 'response': response, 'images': images}, outcome


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--count', type=int, default=800)
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/llm-connector-bank.txt.gz')
    args = parser.parse_args()
    lines, summary = [], {}
    with tempfile.TemporaryDirectory() as directory:
        for image, content in IMAGES.items():
            Path(directory, image).write_text(content)
        for name in CONNECTORS:
            for index in range(args.count):
                case, kind, source, outcome = capture(name, index, directory)
                lines += [f'C {case}', f'K {kind}', 'S ' + json.dumps(source, ensure_ascii=False),
                          'F ' + json.dumps(outcome, ensure_ascii=False, separators=(',', ':'))]
                key = kind + (' raising' if 'raises' in outcome else '')
                summary[key] = summary.get(key, 0) + 1
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0, filename='') as stream:
        stream.write(('\n'.join(lines) + '\n').encode())
    print(json.dumps(dict(sorted(summary.items()))))


if __name__ == '__main__':
    main()
