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
  S json   the SQLite database as iterdump statements (browser history)
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
from pm4py.algo.connectors.variants import chrome_history, firefox_history

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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--count', type=int, default=600)
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/connector-bank.txt.gz')
    args = parser.parse_args()
    lines, summary = [], {}
    with tempfile.TemporaryDirectory() as directory:
        cases = browser_cases(args.count, directory)
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
