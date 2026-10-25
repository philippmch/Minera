"""Public PM4Py 2.7.22 LLM knowledge-injection capture; no native calls.

Every combination of event-data source (pandas dataframe, SQLite connection,
OCEL, traditional EventLog, an unsupported integer), parameter set (none, each
key alone, all keys with unusual characters) and variant (automatic and every
explicit variant) goes through the public
`algo.querying.llm.injection.algorithm.apply`,
`algo.querying.llm.injection.db_knowledge.algorithm.apply` and
`algo.querying.llm.injection.pm_knowledge.algorithm.apply`. Each call records
the returned string or the error class. Every call runs in fresh interpreters
under four PYTHONHASHSEED values and keeps every distinct outcome.

    python -I -B scripts/capture-llm-injection-bank.py \\
        --output docs/llm-injection-bank.json.gz
"""
import argparse
import concurrent.futures
import gzip
import json
import os
import subprocess
import sys

HASH_SEEDS = (0, 1, 2, 3)
SOURCES = ('dataframe', 'sqlite', 'ocel', 'event_log', 'other')
PARAMETERS = {
    'none': {},
    'activity': {'pm4py:param:activity_key': 'Activity'},
    'timestamp': {'pm4py:param:timestamp_key': 'When'},
    'case': {'pm4py:param:case_id_key': 'Case ID'},
    'resource': {'pm4py:param:resource_key': 'who'},
    'table': {'table_name': 'events_2024'},
    'all': {'pm4py:param:activity_key': "it's", 'pm4py:param:timestamp_key': 'ts "utc"',
            'pm4py:param:case_id_key': 'cäse', 'pm4py:param:resource_key': '',
            'table_name': 'my table'},
}
DB_VARIANTS = (None, 'pandas_duckdb', 'sqlite3_traditional')
PM_VARIANTS = (None, 'traditional', 'ocel20')


def calls():
    for source in SOURCES:
        for parameters in PARAMETERS:
            yield {'function': 'injection', 'source': source, 'parameters': parameters, 'variant': None}
            for variant in DB_VARIANTS:
                yield {'function': 'db_knowledge', 'source': source, 'parameters': parameters, 'variant': variant}
            for variant in PM_VARIANTS:
                yield {'function': 'pm_knowledge', 'source': source, 'parameters': parameters, 'variant': variant}


def run():
    import warnings
    warnings.filterwarnings('ignore')
    import sqlite3
    import pandas as pd
    import pm4py
    from pm4py.algo.querying.llm.injection import algorithm as injection
    from pm4py.algo.querying.llm.injection.db_knowledge import algorithm as db_knowledge
    from pm4py.algo.querying.llm.injection.db_knowledge.variants import pandas_duckdb, sqlite3_traditional
    from pm4py.algo.querying.llm.injection.pm_knowledge import algorithm as pm_knowledge
    from pm4py.algo.querying.llm.injection.pm_knowledge.variants import ocel20, traditional
    frame = pd.DataFrame({'case:concept:name': ['1', '1', '2'], 'concept:name': ['a', 'b', 'a'],
                          'time:timestamp': pd.date_range('2024-01-01', periods=3, freq='h', tz='UTC')})
    connection = sqlite3.connect(':memory:')
    frame.to_sql('events', connection)
    sources = {'dataframe': frame, 'sqlite': connection, 'ocel': pm4py.objects.ocel.obj.OCEL(),
               'event_log': pm4py.convert_to_event_log(frame), 'other': 5}
    modules = {'pandas_duckdb': pandas_duckdb, 'sqlite3_traditional': sqlite3_traditional,
               'traditional': traditional, 'ocel20': ocel20}
    functions = {'injection': injection, 'db_knowledge': db_knowledge, 'pm_knowledge': pm_knowledge}
    out = []
    for call in calls():
        function = functions[call['function']]
        arguments = {'parameters': dict(PARAMETERS[call['parameters']])}
        if call['variant']:
            arguments['variant'] = modules[call['variant']]
        try:
            out.append({'result': function.apply(sources[call['source']], **arguments)})
        except Exception as error:
            out.append({'error': type(error).__name__})
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
    parser.add_argument('--output', type=os.path.abspath)
    parser.add_argument('--child', action='store_true')
    args = parser.parse_args()
    if args.child:
        print(json.dumps(run()))
        return
    import pm4py
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        runs = list(pool.map(capture, HASH_SEEDS))
    records = []
    summary = {}
    for index, call in enumerate(calls()):
        seen = []
        for run_result in runs:
            if run_result[index] not in seen:
                seen.append(run_result[index])
        outcome = seen[0] if len(seen) == 1 else {'observed': seen}
        kind = 'observed' if len(seen) > 1 else 'error' if 'error' in outcome else 'value'
        summary[kind] = summary.get(kind, 0) + 1
        records.append(dict(call, **outcome))
    bank = {'reference': f'pm4py {pm4py.__version__}', 'hash_seeds': list(HASH_SEEDS),
            'parameters': PARAMETERS, 'calls': records}
    data = json.dumps(bank, indent=0, sort_keys=True, ensure_ascii=False).encode('utf-8')
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0,
                                                      compresslevel=9, filename='') as stream:
        stream.write(data)
    print(f'{len(records)} calls, by outcome {summary}')


if __name__ == '__main__':
    main()
