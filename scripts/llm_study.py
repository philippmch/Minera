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


def study_prompt(task,columns,variants,engine,context='',max_len=10000):
    if task=='clusters':
        lines=['If I have a process with the following process variants:',*[' '+sequence+' ( frequency = '+str(count)+' )' for sequence,count in variants],"Identify clusters of traces. Return a JSON list inside ```json tags, with string 'name' and 'regex' fields for every cluster. Regex searches activity sequences joined with ' -> '. Cases may match more than one cluster."]
    else:
        lines=['Formulate hypotheses over the event data.',context,'The database engine is '+engine+'. Table dataframe has one row per event. Columns (quote literal names):',*[json.dumps(column,ensure_ascii=False) for column in columns],'Return each hypothesis in <description> and </description> tags, with its read-only SELECT query between ```sql and ``` tags. Use only dataframe. Do not modify data or access files.']
    return '\n\n'.join(lines[:-1])[:max_len]+'\n\n'+lines[-1]


def native_json(binary,args):
    process=subprocess.run([binary,*args],stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=90)
    if process.returncode:raise ValueError(process.stderr.decode('utf-8',errors='replace').strip())
    return json.loads(process.stdout)


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
    add_provider_call_arguments(parser)
    args=parser.parse_args(argv)
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
    prompt=study_prompt(args.task,[name for name in columns if name!=source['event_index_column']],variants,args.engine,'Cases: '+str(len(cases))+'; events: '+str(len(source['rows'])),args.max_len)
    report={'family':'LLM_STUDY','task':args.task,'prompt':prompt,'results':[]}
    if args.obtain_query=='false':print(json.dumps(report,ensure_ascii=False));return 0
    if args.response_file:response=Path(args.response_file).read_text(encoding='utf-8')
    else:
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
                command=[args.log,'--sql',plan['sql'],'--limit',str(args.row_limit)]
                if args.engine=='duckdb':command+=['--engine','duckdb','--runtime-url',args.query_runtime_url]
                item['result']=native_json(args.query_cli,command)
        except Exception as error:item['error']=str(error)
    print(json.dumps(report,ensure_ascii=False));return 0


if __name__=='__main__':
    try:raise SystemExit(main())
    except Exception as error:print('error: '+str(error),file=sys.stderr);raise SystemExit(1)
