"""Optional DuckDB engine for native Minera log queries.

Install scripts/requirements-query.txt, run this service, and start Minera with
--query-runtime-url http://127.0.0.1:8192. SQL runs on the supplied event table.
"""
from __future__ import annotations
import argparse
import datetime
import decimal
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
import json
import math
import re
from pathlib import Path
import sys
from threading import Timer
from typing import Any

MAX_INPUT=32*1024*1024
MAX_OUTPUT=16*1024*1024

def scalar(cell:Any):
    if cell is None or cell.get('null') or cell.get('kind')=='na':return None
    if cell.get('container','scalar')!='scalar':return json.dumps(cell,ensure_ascii=False,separators=(',',':'))
    value=cell.get('typed_value',cell.get('value',''))
    kind=cell.get('kind','string')
    if kind=='int':return int(value)
    if kind=='float':return float(value)
    if kind=='boolean':
        if type(value) is bool:return value
        if str(value).lower() in ('true','1'):return True
        if str(value).lower() in ('false','0'):return False
        raise ValueError('Invalid boolean query cell.')
    if kind=='bytes':return str(cell.get('value','')).encode('utf-8')
    return str(value)

def value_json(value:Any):
    if value is None or type(value) in (str,bool):return value
    if type(value) is int:return value if abs(value)<=9007199254740991 else {'type':'integer','value':str(value)}
    if type(value) is float:return value if math.isfinite(value) else {'type':'float','value':'NaN' if math.isnan(value) else '-Infinity' if value<0 else 'Infinity'}
    if isinstance(value,bytes):return {'type':'blob','value':value.hex()}
    if isinstance(value,decimal.Decimal):return {'type':'decimal','value':str(value)}
    if isinstance(value,(datetime.datetime,datetime.date,datetime.time)):return {'type':'date','value':value.isoformat()}
    if isinstance(value,datetime.timedelta):return {'type':'interval','value':str(value)}
    if isinstance(value,(list,tuple)):return [value_json(item) for item in value]
    if isinstance(value,dict):return {str(key):value_json(item) for key,item in value.items()}
    raise ValueError('Unsupported DuckDB result type.')

def column_type(cells):
    present=[cell for cell in cells if cell is not None and not cell.get('null') and cell.get('kind')!='na']
    kinds={cell.get('kind','string') for cell in present}
    if any(cell.get('container','scalar')!='scalar' for cell in present):return 'VARCHAR'
    if not kinds:return 'VARCHAR'
    if kinds<={'int'}:return 'HUGEINT'
    if kinds<={'int','float'}:return 'DOUBLE'
    if kinds<={'boolean'}:return 'BOOLEAN'
    if kinds<={'date'}:
        values=[str(cell.get('typed_value',cell.get('value',''))) for cell in present]
        return 'TIMESTAMPTZ' if any(re.search(r'(Z|[+-]\d{2}:\d{2})$',value) for value in values) else 'TIMESTAMP_NS'
    if kinds<={'bytes'}:return 'BLOB'
    return 'VARCHAR'

def identifier(name):
    if not isinstance(name,str) or '\x00' in name:raise ValueError('Invalid query column name.')
    return '"'+name.replace('"','""')+'"'

def query(payload,select_only=False):
    import duckdb
    if not isinstance(payload,dict) or set(payload)-{'sql','max_rows','source_id','data'}:raise ValueError('Invalid query request fields.')
    sql=payload.get('sql')
    if not isinstance(sql,str) or not sql.strip() or len(sql.encode('utf-8'))>1024*1024:raise ValueError('Enter SQL within the input bound.')
    limit=payload.get('max_rows',1000)
    if type(limit) is not int or not 1<=limit<=1000000:raise ValueError('Invalid query row limit.')
    data=payload['data'];table=data['table'];columns=table['columns'];rows=table['rows'];index_name=data['event_index_column']
    if not isinstance(columns,list) or not columns or len(columns)!=len(set(columns)) or any(not isinstance(row,list) or len(row)!=len(columns) for row in rows):raise ValueError('Invalid source event table.')
    connection=duckdb.connect(':memory:',config={'enable_external_access':False,'autoload_known_extensions':False,'autoinstall_known_extensions':False,'threads':1,'memory_limit':'512MB','temp_directory':''})
    timer=None
    try:
        types=[column_type([row[column] for row in rows]) for column in range(len(columns))]
        connection.execute('CREATE TABLE dataframe ('+','.join(identifier(name)+' '+kind for name,kind in zip(columns,types))+')')
        if rows:
            values=[[scalar(cell) for cell in row] for row in rows]
            connection.executemany('INSERT INTO dataframe VALUES ('+','.join('?' for _ in columns)+')',values)
        statements=connection.extract_statements(sql)
        if len(statements)!=1 or statements[0].type!=duckdb.StatementType.SELECT:raise ValueError('Expected one read-only result query.')
        timer=Timer(20,connection.interrupt);timer.daemon=True;timer.start()
        cursor=connection.execute(sql)
        result_columns=[item[0] for item in cursor.description]
        records=cursor.fetchmany(limit+1);truncated=len(records)>limit;records=records[:limit]
        index_column=result_columns.index(index_name) if result_columns.count(index_name)==1 else None
        indices=set();can_filter=index_column is not None
        for row in records:
            if index_column is not None:
                index=row[index_column]
                if type(index) is int and 0<=index<len(rows):indices.add(index)
                else:can_filter=False
        if select_only:
            if not can_filter or truncated:raise ValueError('Filter SQL must return valid event indices for a complete selection.')
            return {'indices':sorted(indices)}
        return {'family':'LOG_QUERY','engine':'duckdb','sql':sql,'source_id':payload.get('source_id',''),'event_index_column':index_name,'columns':result_columns,'rows':[[value_json(value) for value in row] for row in records],'output_truncated':truncated,'can_filter':can_filter,'selected_event_indices':sorted(indices) if can_filter and not truncated else []}
    finally:
        if timer:timer.cancel()
        connection.close()

class Handler(BaseHTTPRequestHandler):
    def log_message(self,*args):pass
    def send_json(self,status,value):
        body=json.dumps(value,ensure_ascii=False,allow_nan=False).encode('utf-8')
        if len(body)>MAX_OUTPUT:status=413;body=b'{"error":"Query output exceeds 16 MiB; reduce the row limit."}'
        self.send_response(status);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(body)));self.end_headers()
        try:self.wfile.write(body)
        except (BrokenPipeError,ConnectionResetError):pass
    def do_GET(self):
        if self.path!='/health':self.send_json(404,{'error':'Unknown query route.'});return
        try:
            import duckdb
            self.send_json(200,{'family':'LOG_QUERY_RUNTIME','ready':True,'duckdb_version':duckdb.__version__})
        except ImportError:self.send_json(503,{'error':'Install scripts/requirements-query.txt.'})
    def do_POST(self):
        if self.path not in ('/query','/select','/cluster-select'):self.send_json(404,{'error':'Unknown query route.'});return
        if self.headers.get('Origin'):self.send_json(403,{'error':'Use the native Minera query bridge.'});return
        try:
            length=int(self.headers.get('Content-Length','0'))
            if not 0<length<=MAX_INPUT:self.send_json(413,{'error':'Query input exceeds 32 MiB.'});return
            if self.headers.get_content_type()!='application/json':self.send_json(415,{'error':'Expected JSON.'});return
            payload=json.loads(self.rfile.read(length))
            if self.path=='/cluster-select':
                from llm_study import select_cluster_bounded
                self.send_json(200,select_cluster_bounded(payload))
            else:self.send_json(200,query(payload,self.path=='/select'))
        except Exception as error:self.send_json(400,{'error':str(error)})

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--port',type=int,default=8192);parser.add_argument('--execute',metavar='JSON_FILE');args=parser.parse_args()
    if args.execute:
        print(json.dumps(query(json.loads(Path(args.execute).read_bytes())),ensure_ascii=False,allow_nan=False));return
    if not 1<=args.port<=65535:raise ValueError('Invalid query runtime port.')
    server=ThreadingHTTPServer(('127.0.0.1',args.port),Handler);print(f'Minera DuckDB query runtime listening on 127.0.0.1:{args.port}',flush=True)
    try:server.serve_forever()
    except KeyboardInterrupt:pass
    finally:server.server_close()

if __name__=='__main__':main()
