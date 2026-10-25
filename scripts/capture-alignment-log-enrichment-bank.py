"""Public PM4Py 2.7.22 alignment log enrichment capture; no native calls.

Seeded logs (concept:name, sometimes org:resource, lifecycle:transition and a
generic attribute that may already hold the target key) are enriched through
the public `algo.conformance.alignments.petri_net.utils.log_enrichment` with
alignments in the `ret_tuple_as_trans_desc` move shape. Most alignments are
synthesized move by move (sync, log, visible and silent model moves, with
deliberate excess or missing log-side moves and None entries); the rest come
from the public alignment call on an inductive-miner model of the case's own
log, so real move shapes are covered too. Every case is called with and
without ENABLE_DEEPCOPY and records the error name (if any), the returned
log, and the input log after the call.

    python -I -B scripts/capture-alignment-log-enrichment-bank.py \
        --output docs/alignment-log-enrichment-bank.json.gz
"""
import argparse
import copy
import gzip
import json
from pathlib import Path
import random
import warnings

BANK_SEED = 20261025
LETTERS = 'abcde'
TARGETS = ('@@default', '@@transition_id', 'concept:name', 'org:resource',
           'lifecycle:transition', 'tag', 'enriched')


def generate(count):
    rng = random.Random(BANK_SEED)
    cases = []
    for index in range(count):
        traces = []
        for _ in range(rng.randint(1, 4)):
            events = []
            for _ in range(rng.randint(0, 5)):
                event = {'concept:name': rng.choice(LETTERS)}
                if rng.random() < 0.4:
                    event['org:resource'] = rng.choice(('r1', 'r2'))
                if rng.random() < 0.3:
                    event['lifecycle:transition'] = rng.choice(('start', 'complete'))
                if rng.random() < 0.4:
                    event['tag'] = rng.choice(('x', 'y'))
                events.append(event)
            traces.append(events)
        case = {'name': f'enrich_{index:04d}', 'traces': traces,
                'target': rng.choice(TARGETS)}
        if index % 5 == 4:
            case['real'] = True
        else:
            case['aligned'] = synthesize(rng, traces)
        cases.append(case)
    return cases


def synthesize(rng, traces):
    entries = len(traces) + rng.choice((0, 0, 0, 0, -1, 1))
    aligned = []
    for position in range(max(entries, 0)):
        if rng.random() < 0.05:
            aligned.append(None)
            continue
        events = traces[position] if position < len(traces) else []
        log_side = len(events) + (rng.choice((-1, 1)) if rng.random() < 0.1 else 0)
        moves = []
        for step in range(max(log_side, 0)):
            label = events[step]['concept:name'] if step < len(events) else 'z'
            while rng.random() < 0.25:
                moves.append(model_move(rng))
            if rng.random() < 0.7:
                moves.append([[f'e{step}', f'n_{label}{rng.randint(0, 2)}'], [label, label]])
            else:
                moves.append([[f'e{step}', '>>'], [label, '>>']])
        if rng.random() < 0.3:
            moves.append(model_move(rng))
        aligned.append({'alignment': moves, 'cost': 0})
    return aligned


def model_move(rng):
    if rng.random() < 0.5:
        return [['>>', f'tau{rng.randint(0, 3)}'], ['>>', None]]
    label = rng.choice(LETTERS)
    return [['>>', f'n_{label}{rng.randint(0, 2)}'], ['>>', label]]


def build_log(traces):
    from pm4py.objects.log.obj import EventLog, Trace, Event
    return EventLog([Trace([Event(dict(event)) for event in trace],
                           attributes={'concept:name': str(index)})
                     for index, trace in enumerate(traces)])


def dump_log(log):
    return [[{key: str(value) for key, value in event.items()} for event in trace]
            for trace in log]


def tuples(aligned):
    if aligned is None:
        return None
    return {'alignment': [((m[0][0], m[0][1]), (m[1][0], m[1][1])) for m in aligned['alignment']],
            'cost': aligned['cost']}


def lists(aligned):
    if aligned is None:
        return None
    return {'alignment': [[list(m[0]), list(m[1])] for m in aligned['alignment']], 'cost': 0}


def real_alignments(traces):
    from pm4py.algo.discovery.inductive import algorithm as inductive
    from pm4py.algo.conformance.alignments.petri_net import algorithm as alignments
    from pm4py.objects.conversion.process_tree import converter
    log = build_log(traces)
    discovery = build_log([trace for trace in traces if trace] or [[{'concept:name': 'a'}]])
    net, im, fm = converter.apply(inductive.apply(discovery))
    result = alignments.apply(log, net, im, fm, parameters={'ret_tuple_as_trans_desc': True})
    return [lists(entry) for entry in result]


def call(case, deepcopy):
    from pm4py.algo.conformance.alignments.petri_net.utils import log_enrichment as enrichment
    P = enrichment.Parameters
    log = build_log(case['traces'])
    parameters = {P.ENABLE_DEEPCOPY: deepcopy}
    if case['target'] != '@@default':
        parameters[P.TARGET_ATTRIBUTE] = case['target']
    aligned = [tuples(entry) for entry in case['aligned']]
    try:
        returned = dump_log(enrichment.apply(log, copy.deepcopy(aligned), parameters=parameters))
        error = None
    except Exception as exception:
        returned = None
        error = type(exception).__name__
    return {'error': error, 'returned': returned, 'input_after': dump_log(log)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--count', type=int, default=1500)
    args = parser.parse_args()
    warnings.filterwarnings('ignore')
    import pm4py
    cases = generate(args.count)
    for case in cases:
        if case.pop('real', False):
            case['aligned'] = real_alignments(case['traces'])
        case['deepcopy'] = call(case, True)
        case['in_place'] = call(case, False)
    bank = {'reference': f'pm4py {pm4py.__version__}', 'bank_seed': BANK_SEED, 'cases': cases}
    data = json.dumps(bank, indent=0, sort_keys=True).encode('utf-8')
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0,
                                                      compresslevel=9, filename='') as stream:
        stream.write(data)
    errors = {}
    for case in cases:
        name = case['in_place']['error']
        errors[name] = errors.get(name, 0) + 1
    print(f'{len(cases)} cases, in-place outcomes {errors}')


if __name__ == '__main__':
    main()
