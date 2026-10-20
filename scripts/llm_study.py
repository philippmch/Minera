"""Case clustering and executable hypotheses using public response formats.

The optional query runtime uses --cluster-worker internally. Standalone studies
use native minera_query/minera_cluster consumers to retain the original log.
"""
from __future__ import annotations
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import subprocess
import tempfile
import sys
import urllib.error
import urllib.request
if __package__:
    from .llm_runtime import add_provider_call_arguments, provider_call_overrides
else:
    from llm_runtime import add_provider_call_arguments, provider_call_overrides


def restore_sql(sql):
    return re.sub(r'ordabcchrordabc(\d+)ordabcchr',lambda match:chr(int(match[1])),sql)


def cluster_definitions(response):
    match=re.search(r'```json\s*([\s\S]*?)```',response)
    if not match:raise ValueError('The cluster response must contain a ```json block.')
    definitions=json.loads(match[1])
    if not isinstance(definitions,list) or len(definitions)>100:raise ValueError('Expected a JSON list of at most 100 clusters.')
    if any(not isinstance(item,dict) or not isinstance(item.get('name'),str) or not isinstance(item.get('regex'),str) for item in definitions):raise ValueError('Every cluster requires string name and regex fields.')
    return [{'name':item['name'],'regex':item['regex']} for item in definitions]


def hypothesis_definitions(response):
    descriptions=re.findall(r'<description>([\s\S]*?)</description>',response)
    queries=re.findall(r'```sql\s*([\s\S]*?)```',response)
    if len(descriptions)!=len(queries) or not queries or len(queries)>100:raise ValueError('Provide matching <description> tags and ```sql blocks for 1 through 100 hypotheses.')
    if any(not sql.strip() for sql in queries):raise ValueError('A hypothesis SQL block is empty.')
    return [{'description':description.strip(),'sql':restore_sql(sql.strip())} for description,sql in zip(descriptions,queries)]


def select_cluster(payload):
    if not isinstance(payload,dict) or set(payload)!={'regex','cases'} or not isinstance(payload['regex'],str) or not isinstance(payload['cases'],list):raise ValueError('Invalid cluster selection request.')
    pattern=re.compile(payload['regex'])
    selected=set()
    for case in payload['cases']:
        if not isinstance(case,dict) or set(case)!={'text','indices'} or not isinstance(case['text'],str) or not isinstance(case['indices'],list) or any(type(index) is not int or index<0 for index in case['indices']):raise ValueError('Invalid case sequence or event identity.')
        if pattern.search(case['text']):selected.update(case['indices'])
    return {'indices':sorted(selected)}


def select_cluster_bounded(payload):
    try:
        process=subprocess.run([sys.executable,'-B','-X','utf8',str(Path(__file__).resolve()),'--cluster-worker'],input=json.dumps(payload,ensure_ascii=False).encode('utf-8'),stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=15)
    except subprocess.TimeoutExpired:raise ValueError('Cluster matching exceeded 15 seconds.') from None
    if process.returncode:raise ValueError(process.stderr.decode('utf-8',errors='replace').strip() or 'Could not match the cluster expression.')
    return json.loads(process.stdout)


def study_prompt(task,columns,variants,engine,context='',max_len=10000,table_name='dataframe'):
    if task=='clusters':
        lines=['If I have a process with the following process variants:',*[' '+sequence+' ( frequency = '+str(count)+' )' for sequence,count in variants],"Identify clusters of traces. Return a JSON list inside ```json tags, with string 'name' and 'regex' fields for every cluster. Regex searches activity sequences joined with ' -> '. Cases may match more than one cluster."]
    else:
        lines=['Formulate hypotheses over the event data.',context,'The database engine is '+engine+'. Table '+table_name+' has one row per event. Columns (quote literal names):',*[json.dumps(column,ensure_ascii=False) for column in columns],'Return each hypothesis in <description> and </description> tags, with its read-only SELECT query between ```sql and ``` tags. Use only '+table_name+'. Do not modify data or access files.']
    return '\n\n'.join(lines[:-1])[:max_len]+'\n\n'+lines[-1]


def native_json(binary,args):
    process=subprocess.run([binary,*args],stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=90)
    if process.returncode:raise ValueError(process.stderr.decode('utf-8',errors='replace').strip())
    return json.loads(process.stdout)


def exact_query_json(value):
    # The native binary64 writer may emit -0 without a fractional suffix.
    # Keep that sign only in exact projections; legacy native_json is unchanged.
    return json.loads(value,parse_int=lambda token:-0.0 if token=='-0' else int(token))


def hypothesis_query_exact(binary,args,report_directory=None,index=0):
    def execute(directory):
        directory=Path(directory);directory.mkdir(parents=True,exist_ok=True)
        report_path=directory/f'hypothesis-{index+1}-query.json'
        if report_path.exists():raise ValueError('Exact query report already exists: '+str(report_path))
        process=subprocess.run([binary,*args,'--exact-query-report',str(report_path)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=90)
        report_bytes=None
        try:
            report_bytes=report_path.read_bytes();report=exact_query_json(report_bytes)
            if not isinstance(report,dict) or report.get('family')!='EXACT_LOG_QUERY':raise ValueError('Invalid exact query report.')
        except (OSError,ValueError,UnicodeError) as error:
            # Report I/O/projection failure is not evidence of a failed native
            # query. Preserve physical streams and any legacy result while the
            # authoritative query outcome remains explicitly unobserved.
            result={'query_report':None,'query_report_bytes':None if report_bytes is None else {'native_bytes':'hex-v1','data':report_bytes.hex()},
                    'query_exit_code':process.returncode,'query_report_error':{'phase':'report_io','message':str(error)},
                    'query_stdout':{'native_bytes':'hex-v1','data':process.stdout.hex()},'query_stderr':{'native_bytes':'hex-v1','data':process.stderr.hex()},
                    'legacy_projection_error':None,'result':None,'error':{'phase':'report_io','message':'The canonical query outcome is unavailable; this is not a native-query failure assertion.'}}
            try:result['result']=exact_query_json(process.stdout)
            except (ValueError,UnicodeError) as projection_error:result['legacy_projection_error']=str(projection_error)
            return result
        result={'query_report':report,'query_report_bytes':{'native_bytes':'hex-v1','data':report_bytes.hex()},'query_exit_code':process.returncode,'legacy_projection_error':None,'result':None,'error':None}
        query=report.get('calls',{}).get('query',{})
        if query.get('returned'):
            try:result['result']=exact_query_json(process.stdout)
            except (ValueError,UnicodeError) as error:
                result['legacy_projection_error']=str(error)
                result['result']=report.get('result')
        else:result['result']=report.get('result')
        if report.get('error') is not None:result['error']=report['error']
        elif process.returncode:result['error']={'phase':'cli','message':'Query process failed; canonical report and original stdout/stderr retained.'}
        return result
    if report_directory is not None:return execute(report_directory)
    with tempfile.TemporaryDirectory(prefix='minera-hypothesis-query-') as directory:return execute(directory)


def hypothesis_table_name(options_json,options_file):
    # Prompt preview consumes a separate copy. It never rewrites either the
    # original options File or the raw JSON passed to the native consumer.
    if options_json is None and options_file is None:return 'dataframe'
    text=options_json if options_json is not None else Path(options_file).read_bytes()
    value=json.loads(text);name=value['table_name']
    if isinstance(name,dict):
        if set(name)!={'native_bytes','data'} or name['native_bytes']!='hex-v1':raise ValueError('Invalid prompt table NativeString.')
        name=bytes.fromhex(name['data']).decode('utf-8',errors='strict')
    if not isinstance(name,str):raise ValueError('The table name cannot be represented in the provider prompt.')
    name.encode('utf-8',errors='strict')
    return '"'+name.replace('"','""')+'"'


def main(argv=None):
    if argv is None:argv=sys.argv[1:]
    if argv==['--cluster-worker']:
        try:print(json.dumps(select_cluster(json.load(sys.stdin)),ensure_ascii=False));return 0
        except Exception as error:print(str(error),file=sys.stderr);return 1
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log');parser.add_argument('--task',choices=('clusters','hypotheses'),required=True)
    parser.add_argument('--query-cli',default='minera_query');parser.add_argument('--cluster-cli',default='minera_cluster')
    parser.add_argument('--query-runtime-url',default='http://127.0.0.1:8192');parser.add_argument('--llm-runtime-url',default='http://127.0.0.1:8189')
    parser.add_argument('--response-file');parser.add_argument('--obtain-query',choices=('true','false'),default='true');parser.add_argument('--execute-query',choices=('true','false'),default='true')
    parser.add_argument('--provider',choices=('openai','anthropic','google'),default='openai');parser.add_argument('--model');parser.add_argument('--engine',choices=('sqlite','duckdb'),default='sqlite')
    parser.add_argument('--case-attribute',default='case:concept:name');parser.add_argument('--activity-attribute');parser.add_argument('--max-len',type=int,default=10000);parser.add_argument('--row-limit',type=int,default=1000);parser.add_argument('--output-dir')
    parser.add_argument('--query-options-json');parser.add_argument('--query-options-file');parser.add_argument('--exact-query-report-dir')
    add_provider_call_arguments(parser)
    args=parser.parse_args(argv)
    exact_query=args.query_options_json is not None or args.query_options_file is not None
    if args.query_options_json is not None and args.query_options_file is not None:raise ValueError('Query options JSON/File conflict by presence.')
    if args.task!='hypotheses' and (exact_query or args.exact_query_report_dir is not None):raise ValueError('Query options/report directory apply only to hypotheses.')
    if args.exact_query_report_dir is not None and not exact_query:raise ValueError('Exact query report directory requires full query options.')
    if not 1<=args.max_len<=2*1024*1024 or not 1<=args.row_limit<=1000000:raise ValueError('Invalid prompt or row bound.')
    source=native_json(args.query_cli,[args.log,'--sql','SELECT * FROM dataframe','--limit','1000000'])
    if source['output_truncated']:raise ValueError('The source exceeds the complete study bound.')
    columns=source['columns'];case_column=columns.index(args.case_attribute)
    activity=args.activity_attribute or next((name for name in ('activity','concept:name') if name in columns),None)
    if not activity:raise ValueError('Specify --activity-attribute for this source.')
    activity_column=columns.index(activity);cases={}
    for row in source['rows']:
        if not isinstance(row[case_column],str) or not isinstance(row[activity_column],str):raise ValueError('Case and activity cells must be strings for the study prompt.')
        cases.setdefault(row[case_column],[]).append(row[activity_column])
    variants=Counter(' -> '.join(events) for events in cases.values()).most_common()
    prompt_error=None
    try:
        prompt_table=hypothesis_table_name(args.query_options_json,args.query_options_file) if exact_query else 'dataframe'
        prompt=study_prompt(args.task,[name for name in columns if name!=source['event_index_column']],variants,args.engine,'Cases: '+str(len(cases))+'; events: '+str(len(source['rows'])),args.max_len,table_name=prompt_table)
    except (OSError,ValueError,UnicodeError,KeyError,TypeError) as error:
        if not exact_query:raise
        prompt=None;prompt_error=str(error)
    report={'family':'LLM_STUDY','task':args.task,'prompt':prompt,'results':[]}
    if exact_query:report['prompt_error']=prompt_error
    if args.obtain_query=='false':print(json.dumps(report,ensure_ascii=False));return 0
    if args.response_file:response=Path(args.response_file).read_text(encoding='utf-8')
    else:
        if prompt_error:raise ValueError('Cannot generate a provider prompt for these exact options: '+prompt_error)
        if not args.model:raise ValueError('Enter --model or supply --response-file.')
        request=urllib.request.Request(args.llm_runtime_url.rstrip('/')+'/complete',data=json.dumps({'provider':args.provider,'model':args.model,'prompt':prompt,'max_tokens':4096,**provider_call_overrides(args)}).encode(),headers={'Content-Type':'application/json'})
        with urllib.request.urlopen(request,timeout=90) as reply:completion=json.load(reply)
        report['completion']=completion;response=completion['text']
    report['response']=response
    plans=cluster_definitions(response) if args.task=='clusters' else hypothesis_definitions(response)
    for index,plan in enumerate(plans):
        item={**plan,'result':None,'error':None};report['results'].append(item)
        if args.execute_query=='false':continue
        try:
            if args.task=='clusters':
                command=[args.log,'--regex',plan['regex'],'--name',plan['name'],'--runtime-url',args.query_runtime_url]
                if args.activity_attribute:command+=['--activity-attribute',args.activity_attribute]
                if args.output_dir:
                    directory=Path(args.output_dir);directory.mkdir(parents=True,exist_ok=True);output=directory/f'cluster-{index+1}.xes'
                    if output.exists():raise ValueError('Cluster export already exists: '+str(output))
                    command+=['--filter-output',str(output)]
                item['result']=native_json(args.cluster_cli,command)
            else:
                command=[args.log,'--sql',plan['sql']]
                if exact_query:
                    command+=['--query-options-json',args.query_options_json] if args.query_options_json is not None else ['--query-options-file',args.query_options_file]
                else:command+=['--limit',str(args.row_limit)]
                if args.engine=='duckdb':command+=['--engine','duckdb','--runtime-url',args.query_runtime_url]
                if exact_query:item.update(hypothesis_query_exact(args.query_cli,command,args.exact_query_report_dir,index))
                else:item['result']=native_json(args.query_cli,command)
        except Exception as error:item['error']=str(error)
    print(json.dumps(report,ensure_ascii=False));return 0


if __name__=='__main__':
    try:raise SystemExit(main())
    except Exception as error:print('error: '+str(error),file=sys.stderr);raise SystemExit(1)
