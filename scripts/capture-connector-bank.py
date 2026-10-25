"""Capture the bank for algo.connectors (stubbed sources).

Browser history: seeded random Chrome History and Firefox places.sqlite
databases (urls with and without schemes, several '//', queries, commas,
ports and user info, empty and NULL urls and titles, NULL and out-of-range
visit times, transitions with qualifier bits, durations, dangling from_visit
and url ids, a Chrome visits table without visit_duration, and databases
without the expected tables) go through
pm4py.algo.connectors.variants.chrome_history / firefox_history.apply.

The bank is a gzip text file of tagged lines read by
minera_connector_parity_tests:

  C name
  K connector
  S json   the SQLite database as iterdump statements (browser history), or
           {"prefix", "sql", "names", "rows", "closed"}: the SQL the
           reference issued, the columns and rows its cursor returned and
           whether it closed the connection (database connectors)
  F json   {"columns", "dtypes", "index", "rows"} of the returned dataframe
           (values as str of a timestamp, repr of a float, True/False, null
           for a missing value), or {"raises": "ExceptionType: message"}
"""
import argparse
import gzip
import json
import os
from pathlib import Path
import random
import sqlite3
import sys
import tempfile
import warnings

warnings.filterwarnings('ignore')
import pandas as pd
from pm4py.algo.connectors.variants import camunda_workflow, chrome_history, firefox_history, sap_accounting, sap_o2c

BANK_SEED = 20261030
ROOT = Path(__file__).resolve().parent.parent
R = random.Random(BANK_SEED)


def url():
    k=R.random()
    if k<0.05: return None
    if k<0.08: return ''
    sch=R.choice(['https://','http://','ftp://','','file:///','chrome://','HTTPS://'])
    host=R.choice(['a.com','b.org','www.c.net','d.co.uk:8080','user:pw@e.io','localhost','','xn--f.de','ü.com'])
    path=''.join(R.choice(['/','/x','/y,z','/a?b=1','?q=2&r=3','#frag','/%20',',','/p/q/'] ) for _ in range(R.randint(0,3)))
    return sch+host+path
def val(kind):
    if R.random()<0.08: return None
    if kind=='t': return R.choice([0, R.randint(13000000000000000,13400000000000000), R.randint(-10**6,10**18), 11644473600000000, R.choice([-11644473600000000, -11644473600000001, 0, 2**63-1, -2**63, 315537897599999999+11644473600000000])])
    if kind=='ft': return R.choice([0, R.randint(1500000000000000,1700000000000000), R.randint(-10**6,10**17), R.choice([3*10**17,-10**17,-62*10**15,253402300799999999,253402300800000000, 2**63-1, -2**63])])
    if kind=='i': return R.randint(-3,20)
    if kind=='tr': return R.choice([0,1,8,R.randint(0,15),R.randint(0,2**32-1),805306368,-1])
    if kind=='s': return R.choice(['A','b title','', 'x,y', 'ü'])
def chrome(path):
    c=sqlite3.connect(path)
    if R.random()<0.02:
        c.close(); return
    dur=R.random()<0.85
    c.execute("create table urls(id integer primary key, url text, title text, visit_count int, typed_count int, last_visit_time int, hidden int)")
    c.execute("create table visits(id integer primary key, url int, visit_time int, from_visit int, transition int, segment_id int"+(", visit_duration int" if dur else "")+")")
    nu=R.randint(0,6); nv=R.randint(0,8)
    for i in range(1,nu+1): c.execute("insert into urls values(?,?,?,?,?,?,0)",(i,url(),val('s'),val('i'),val('i'),val('t')))
    for i in range(1,nv+1):
        row=[i,R.randint(0,nu+1),val('t'),R.choice([0,None,R.randint(0,nv+2)]),val('tr'),0]+([val('i') if R.random()<0.5 else R.randint(0,10**8)] if dur else [])
        c.execute("insert into visits values("+",".join("?"*len(row))+")",row)
    c.commit(); c.close()
def firefox(path):
    c=sqlite3.connect(path)
    if R.random()<0.02:
        c.close(); return
    c.execute("create table moz_places(id integer primary key, url text, title text, rev_host text, visit_count int, hidden int, typed int, frecency int, last_visit_date int, guid text)")
    c.execute("create table moz_historyvisits(id integer primary key, from_visit int, place_id int, visit_date int, visit_type int, session int)")
    nu=R.randint(0,6); nv=R.randint(0,8)
    for i in range(1,nu+1): c.execute("insert into moz_places values(?,?,?,'',?,0,?,?,0,?)",(i,url(),val('s'),val('i'),R.choice([0,1,None,2]),val('i'),val('s')))
    for i in range(1,nv+1): c.execute("insert into moz_historyvisits values(?,?,?,?,?,?)",(i,R.choice([0,None,R.randint(0,nv)]),R.randint(0,nu+1),val('ft'),R.choice([R.randint(0,12),None,-1]),R.choice([0,None,R.randint(0,5)])))
    c.commit(); c.close()
def ser(df):
    rows=[]
    for _,r in df.iterrows():
        row=[]
        for c in df.columns:
            v=r[c]
            if v is None or (not isinstance(v,str) and pd.isna(v)): row.append(None)
            elif isinstance(v,pd.Timestamp): row.append(str(v))
            elif isinstance(v,float): row.append(repr(v))
            else: row.append(str(v))
        rows.append(row)
    return {'columns':list(df.columns),'dtypes':[str(t) for t in df.dtypes],'index':[int(i) for i in df.index],'rows':rows}


def browser_cases(count, directory):
    cases = []
    for k in range(count):
        for name, make, module in (('chrome_history', chrome, chrome_history), ('firefox_history', firefox, firefox_history)):
            path = os.path.join(directory, f'{name}{k}.db')
            make(path)
            dump = list(sqlite3.connect(path).iterdump())
            try:
                result = ser(module.apply({'history_db_path': path}))
            except Exception as error:
                result = {'raises': f'{type(error).__name__}: {error}'}
            cases.append((f'{name}{k}', name, dump, result))
    return cases


# Database connectors: the reference runs its SQL through a DB-API
# connection. The capture hands it a SQLite connection whose cursor records the
# SQL text, the column names and the rows it returns (for order-to-cash, whose
# Oracle SQL SQLite cannot run, the cursor answers with staged rows instead),
# and whether the connection is closed.
class Recorder:
    stage = None
    log = {}


class RecordingCursor(sqlite3.Cursor):
    def execute(self, sql, *args):
        Recorder.log['sql'] = sql
        return super().execute(Recorder.stage or sql, *args)

    def _record(self, rows):
        Recorder.log.setdefault('rows', []).extend([list(r) for r in rows])
        Recorder.log['names'] = [d[0] for d in self.description]
        return rows

    def fetchall(self):
        return self._record(super().fetchall())

    def fetchmany(self, *args):
        return self._record(super().fetchmany(*args))


class RecordingConnection(sqlite3.Connection):
    def cursor(self, factory=RecordingCursor):
        return super().cursor(factory)

    def close(self):
        Recorder.log['closed'] = True
        return super().close()


def maybe(values, none=0.08):
    return None if R.random() < none else R.choice(values)


def camunda_time():
    k = R.random()
    if k < 0.06:
        return None
    if k < 0.08:
        return R.choice(['', 'x', '2023-13-01 00:00:00'])
    y, mo, d, h, mi, s = R.randint(2000, 2030), R.randint(1, 12), R.randint(1, 28), R.randint(0, 23), R.randint(0, 59), R.randint(0, 59)
    return R.choice(['%04d-%02d-%02d %02d:%02d:%02d', '%04d-%02d-%02d %02d:%02d:%02d.123', '%04d-%02d-%02dT%02d:%02d:%02d',
                     '%04d-%02d-%02d %02d:%02d:%02d.000456']) % (y, mo, d, h, mi, s)


def sap_date():
    return None if R.random() < 0.05 else '%04d%02d%02d' % (R.randint(2018, 2024), R.randint(1, 12), R.randint(1, 28))


def sap_time():
    return None if R.random() < 0.05 else '%02d%02d%02d' % (R.randint(0, 23), R.randint(0, 59), R.randint(0, 59))


def database_case(kind):
    con = sqlite3.connect(':memory:', factory=RecordingConnection)
    run = sqlite3.Connection.execute
    params = {}
    Recorder.stage = None
    Recorder.log = {}
    # Rows are kept consistent within a run so that formatting sees one timestamp shape per column.
    if kind == 'camunda_workflow':
        con.executescript('create table act_hi_procinst(PROC_INST_ID_ text, PROC_DEF_KEY_ text);'
                          'create table act_hi_actinst(PROC_INST_ID_ text, EXECUTION_ID_ text, ACT_NAME_ text, START_TIME_ text, ASSIGNEE_ text);')
        uniform = R.random() < 0.6
        shape = camunda_time()
        for p in range(R.randint(0, 3)):
            run(con, 'insert into act_hi_procinst values(?,?)', ('p%d' % p, maybe(['k1', 'k2'])))
        for a in range(R.randint(0, 8)):
            t = camunda_time()
            if uniform and t and shape:
                t = shape[:10] + t[10:len(shape)] if len(t) >= len(shape) else t
            run(con, 'insert into act_hi_actinst values(?,?,?,?,?)',
                ('p%d' % R.randint(0, 3), maybe(['e1', 'e2', 'e3']), maybe(['A', 'B', 'C', '']), t, maybe(['u', 'v'], 0.3)))
        module = camunda_workflow
    elif kind == 'sap_accounting':
        prefix = R.choice(['', '', 'SAPSR3.'])
        if prefix:
            params['prefix'] = prefix
            con.execute("attach ':memory:' as SAPSR3")
        con.executescript(f'create table {prefix}BKPF(MANDT,BUKRS,GJAHR,BELNR,BLART,CPUDT,CPUTM,USNAM);'
                          f'create table {prefix}BSAD(MANDT,BUKRS,GJAHR,AUGBL,AUGDT);'
                          f'create table {prefix}BSAK(MANDT,BUKRS,GJAHR,AUGBL,AUGDT);')
        for k in range(R.randint(0, 5)):
            run(con, f'insert into {prefix}BKPF values(?,?,?,?,?,?,?,?)',
                ('100', maybe(['1000', '2000'], 0.05), '2023', maybe(['D1', 'D2', 'D3'], 0.05), maybe(['RV', 'KR', 'DZ'], 0.05),
                 sap_date(), sap_time(), maybe(['U1', 'U2'])))
        for table in ('BSAD', 'BSAK'):
            for k in range(R.randint(0, 3)):
                run(con, f'insert into {prefix}{table} values(?,?,?,?,?)',
                    ('100', maybe(['1000', '2000'], 0.05), '2023', maybe(['D1', 'D2', 'D3'], 0.05), sap_date()))
        module = sap_accounting
    else:
        if R.random() < 0.3:
            params['prefix'] = 'SAPSR3.'
        Recorder.stage = 'SELECT * FROM staged'
        con.executescript('create table staged("case:concept:name","concept:name","time:timestamp","org:resource")')
        for k in range(R.randint(0, 8)):
            date, time = sap_date(), sap_time()
            stamp = None if date is None else (date + ' ' + time if time and R.random() < 0.85 else date)
            run(con, 'insert into staged values(?,?,?,?)',
                (maybe(['0001', '0002', '0003'], 0.05), maybe(['Create Sales Document', 'Create Delivery', 'Create Billing', 'Change Order'], 0.05),
                 stamp, maybe(['U1', 'U2'])))
        module = sap_o2c
    con.commit()
    try:
        result = ser(module.apply(con, params))
    except Exception as error:
        result = {'raises': f'{type(error).__name__}: {error}'}
    source = {'prefix': params.get('prefix', ''), 'sql': Recorder.log.get('sql'), 'names': Recorder.log.get('names'),
              'rows': Recorder.log.get('rows', []), 'closed': Recorder.log.get('closed', False)}
    return source, result


def database_cases(count):
    cases = []
    for k in range(count):
        for kind in ('camunda_workflow', 'sap_accounting', 'sap_o2c'):
            source, result = database_case(kind)
            cases.append((f'{kind}{k}', kind, source, result))
    return cases


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--count', type=int, default=600)
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/connector-bank.txt.gz')
    args = parser.parse_args()
    lines, summary = [], {}
    with tempfile.TemporaryDirectory() as directory:
        cases = browser_cases(args.count, directory)
    cases += database_cases(args.count // 2)
    for name, kind, dump, result in cases:
        lines += [f'C {name}', f'K {kind}', 'S ' + json.dumps(dump, ensure_ascii=False),
                  'F ' + json.dumps(result, ensure_ascii=False, separators=(',', ':'))]
        key = kind + (' raising' if 'raises' in result else '')
        summary[key] = summary.get(key, 0) + 1
    with open(args.output, 'wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0, filename='') as stream:
        stream.write(('\n'.join(lines) + '\n').encode())
    print(json.dumps(dict(sorted(summary.items()))))


if __name__ == '__main__':
    main()
