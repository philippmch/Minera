"""Capture the bank for algo.connectors (stubbed sources).

Browser history: seeded random Chrome History and Firefox places.sqlite
databases (urls with and without schemes, several '//', queries, commas,
ports and user info, empty and NULL urls and titles, NULL and out-of-range
visit times, transitions with qualifier bits, durations, dangling from_visit
and url ids, a Chrome visits table without visit_duration, and databases
without the expected tables) go through
pm4py.algo.connectors.variants.chrome_history / firefox_history.apply.

Database connectors: camunda_workflow, sap_accounting and sap_o2c get a
recording DB-API connection over seeded tables.

GitHub: github_repo.apply runs with requests replaced by a stub serving
seeded issue pages and timelines (ISO dates with and without seconds,
fractions, Z and offsets, a few values that are not dates, null users,
actors and logins, missing keys, pull requests, events without created_at,
issues sharing a timeline url, one to three pages); every GET is recorded
with its headers. Each response is a fresh copy, since the reference
reverses the timeline lists it receives.

Outlook and Windows events: win32com and pythoncom are stubbed; Outlook
folders hold seeded items (attributes missing, None or of the wrong type,
naive and aware datetimes, int and float minutes, meeting states, reminders,
senders and recipients without Name, mail classes 43 and 46 and others) and
the WMI query returns rows of seeded Win32_NTLogEvent properties (times with
and without fraction and offset, overlong fractions, out-of-range dates,
missing properties).

The bank is a gzip text file of tagged lines read by
minera_connector_parity_tests:

  C name
  K connector
  S json   the SQLite database as iterdump statements (browser history),
           {"prefix", "sql", "names", "rows", "closed"}: the SQL the
           reference issued, the columns and rows its cursor returned and
           whether it closed the connection (database connectors), or
           {"owner", "repository", "token", "pages", "timelines", "calls"}:
           the issue pages by number, the timelines by url and the GETs as
           [url, headers] (github_repo), {"items"} (outlook_calendar),
           {"sent", "inbox"} (outlook_mail_extractor) or {"rows"}
           (windows_events), {"user", "mailbox"} (util.mail connect):
           COM values as JSON scalars, {"float": x},
           {"dt": isoformat}, {"obj": {attribute: value}} and lists
  F json   {"columns", "dtypes", "index", "rows"} of the returned dataframe
           ({"calls"}: the COM calls of util.mail.connect, as text)
           (values as str of a timestamp, repr of a float, True/False, null
           for a missing value), or {"raises": "ExceptionType: message"}
"""
import argparse
import contextlib
import copy
import datetime
import gzip
import json
import os
from pathlib import Path
import random
import sqlite3
import sys
import tempfile
import types
import warnings

warnings.filterwarnings('ignore')
import pandas as pd
requests = types.ModuleType('requests')
sys.modules['requests'] = requests
win32com = types.ModuleType('win32com')
win32com.client = types.ModuleType('win32com.client')
pythoncom = types.ModuleType('pythoncom')
pythoncom.CoInitialize = lambda: None
sys.modules.update({'win32com': win32com, 'win32com.client': win32com.client, 'pythoncom': pythoncom})
from pm4py.algo.connectors.util import mail
from pm4py.algo.connectors.variants import (camunda_workflow, chrome_history, firefox_history, github_repo,
                                            outlook_calendar, outlook_mail_extractor, sap_accounting, sap_o2c,
                                            windows_events)

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


def github_date():
    if R.random() < 0.03:
        return R.choice([None, 5, 'garbage', '2020-01-01T24:00:00Z', '2021-02-30T00:00:00Z', '2021-04-31', ''])
    s = '20%02d-%02d-%02d' % (R.randint(15, 24), R.randint(1, 12), R.randint(1, 28))
    k = R.random()
    if k < 0.08:
        return s
    s += R.choice(['T', 'T', 'T', ' ']) + '%02d:%02d' % (R.randint(0, 23), R.randint(0, 59))
    if k < 0.85:
        s += ':%02d' % R.randint(0, 59)
        if k > 0.7:
            s += '.' + ''.join(R.choice('0123456789') for _ in range(R.randint(1, 9)))
    return s + R.choice(['Z', 'Z', 'Z', 'Z', '', '+01:00', '-0530', '+00:00'])


def github_user():
    if R.random() < 0.06:
        return R.choice([None, {'login': None}, {}, {'id': 3}])
    return {'login': R.choice(['octo', 'mona', 'ü-dev', 'bot[bot]'])}


def github_drop(d, rate):
    for k in list(d):
        if R.random() < rate:
            del d[k]
    return d


def github_case(index):
    owner, repository = R.choice(['o', 'Owner-1']), R.choice(['r', 're.po'])
    base = f'https://api.github.com/repos/{owner}/{repository}/issues'
    pages, number = {}, 1
    count = R.choice([0, 1, 1, 1, 2, 2, 3])
    for page in range(1, count + 1):
        size = R.randint(1, 5)
        issues = []
        for _ in range(size):
            shared = issues and R.random() < 0.05
            issue = {'url': f'{base}/{number}', 'number': number,
                     'title': R.choice(['Fix it', 'a, b', '', None, 'ü title']), 'user': github_user(),
                     'created_at': github_date(), 'state': R.choice(['open', 'closed']),
                     'author_association': R.choice(['NONE', 'OWNER', 'CONTRIBUTOR', None]),
                     'timeline_url': issues[-1]['timeline_url'] if shared and 'timeline_url' in issues[-1]
                     else f'{base}/{number}/timeline'}
            if R.random() < 0.25:
                issue['pull_request'] = R.choice([{'url': f'https://api.github.com/repos/{owner}/{repository}/pulls/{number}'}] * 8
                                                 + [None, {}])
            issues.append(github_drop(issue, 0.015))
            number += 1
        pages[page] = issues
    timelines = {}
    for issues in pages.values():
        for issue in issues:
            if 'timeline_url' not in issue or issue['timeline_url'] in timelines:
                continue
            events = []
            for _ in range(R.randint(0, 5)):
                event = {'event': R.choice(['commented', 'labeled', 'closed', 'reopened', 'referenced',
                                            'cross-referenced', 'assigned', 'merged', None]),
                         'created_at': github_date(), 'actor': github_user()}
                if R.random() < 0.15:
                    event = {'event': 'committed', 'author': {'date': github_date()}, 'sha': 'abc'}
                if R.random() < 0.2:
                    event['user'] = github_user()
                events.append(github_drop(event, 0.02))
            timelines[issue['timeline_url']] = events
    token = R.choice([None, None, 'tok', ''])
    calls = []

    def get(url, *args, **kwargs):
        calls.append([url, kwargs.get('headers')])
        prefix = base + '?state=all&per_page=100&page='
        if url.startswith(prefix):
            return Response(copy.deepcopy(pages.get(int(url[len(prefix):]), [])))
        return Response(copy.deepcopy(timelines.get(url, [])))

    requests.get = get
    parameters = {'owner': owner, 'repository': repository}
    if token is not None:
        parameters['auth_token'] = token
    try:
        with open(os.devnull, 'w') as sink, contextlib.redirect_stderr(sink):
            result = ser(github_repo.apply(parameters))
    except Exception as e:
        result = {'raises': f'{type(e).__name__}: {e}'}
    source = {'owner': owner, 'repository': repository, 'token': token, 'pages': {str(k): v for k, v in pages.items()},
              'timelines': timelines, 'calls': calls}
    return f'github{index}', 'github_repo', source, result


class ComObject:
    """A COM object: reading a missing attribute raises AttributeError."""

    def __init__(self, attributes):
        self.attributes = attributes

    def __getattr__(self, name):
        try:
            return self.__dict__['attributes'][name]
        except KeyError:
            raise AttributeError(name) from None


class WmiValue:
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return str(self.value)


class WmiRow:
    def __init__(self, properties):
        self.properties = properties

    def Properties_(self, name):
        return WmiValue(self.properties[name])


COM = {}


def com_dispatch(name):
    if name == 'Outlook.Application':
        namespace = types.SimpleNamespace(GetDefaultFolder=lambda n: types.SimpleNamespace(Items=COM.get(n, [])))
        return types.SimpleNamespace(GetNamespace=lambda kind: namespace)
    service = types.SimpleNamespace(ExecQuery=lambda query: COM.get('wmi', []))
    return types.SimpleNamespace(ConnectServer=lambda *args: service)


win32com.client.Dispatch = com_dispatch


def com_json(value):
    if isinstance(value, datetime.datetime):
        return {'dt': value.isoformat()}
    if isinstance(value, ComObject):
        return {'obj': {k: com_json(v) for k, v in value.attributes.items()}}
    if isinstance(value, list):
        return [com_json(v) for v in value]
    if isinstance(value, float):
        return {'float': value}
    return value


def com_time():
    value = datetime.datetime(2024, R.randint(1, 3), R.randint(1, 28), R.randint(0, 23), R.randint(0, 59),
                              R.randint(0, 59), R.choice([0, 0, 0, R.randint(0, 999999)]))
    if R.random() < 0.3:
        value = value.replace(tzinfo=datetime.timezone(datetime.timedelta(minutes=R.choice([-300, 0, 120, 330]))))
    return value


def com_item(generators, missing, none):
    attributes = {}
    for name, generate in generators.items():
        r = R.random()
        if r < missing:
            continue
        attributes[name] = None if r < missing + none else generate()
    return attributes


CALENDAR = {'ConversationID': lambda: R.choice(['c1', 'c2', 'c3', '']), 'Subject': lambda: R.choice(['s', 'Team, sync', 'ü', '']),
            'CreationTime': com_time, 'LastModificationTime': com_time, 'Start': com_time,
            'Duration': lambda: R.choice([0, 30, 60, R.randint(0, 500), 1.5, '30', -10, True]),
            'MeetingStatus': lambda: R.choice([0, 1, 3, 5, 5, 7, '5', 5.0, None]), 'ReminderSet': lambda: R.choice([True, False, 1, 0]),
            'ReminderMinutesBeforeStart': lambda: R.choice([0, 15, 30, 2.5, '15', 2.0, True, -5])}


def com_person():
    r = R.random()
    if r < 0.05:
        return None
    if r < 0.1:
        return ComObject({})
    return ComObject({'Name': R.choice(['Ann', 'Bob', 'Lee, Kim', None, ''])})


def com_recipients():
    r = R.random()
    if r < 0.05:
        return None
    if r < 0.07:
        return ''
    return [com_person() for _ in range(R.randint(0, 3))]


MAIL = {'Class': lambda: R.choice([43, 43, 43, 43, 46, 0, '43', '46', 43.0, True]),
        'Subject': lambda: R.choice(['s', 'RE: s', 'ü', '', None]), 'CreationTime': com_time, 'Sender': com_person,
        'Recipients': com_recipients, 'ConversationID': lambda: R.choice(['c1', 'c2', 'c3', '', None]),
        'ConversationTopic': lambda: R.choice(['t', 'topic, x', '', None])}


def wmi_time():
    r = R.random()
    base = '2024%02d%02d%02d%02d%02d' % (R.randint(1, 12), R.randint(1, 28), R.randint(0, 23), R.randint(0, 59), R.randint(0, 59))
    if r < 0.03:
        return R.choice([None, '', 'garbage', '20241302030405.000000+000', '2024', '20240231030405.1', '20240102030460.1',
                         '2024012030405.123', '20240102030405.1_x+000'])
    fraction = R.choice(['.000000', '.123456', '.5', '.1234567', ''] if r < 0.12 else ['.000000', '.%06d' % R.randint(0, 999999)])
    offset = R.choice(['+000', '-060', '+060', '-300', '', '+06'] if r < 0.3 else ['+000', '-060', '+120'])
    return base + fraction + offset


WMI = ['Category', 'CategoryString', 'ComputerName', 'EventCode', 'EventIdentifier', 'EventType', 'LogFile', 'Message',
       'RecordNumber', 'SourceName', 'TimeGenerated', 'TimeWritten', 'Type', 'User']


def wmi_value(name):
    if name in ('TimeGenerated', 'TimeWritten'):
        return wmi_time()
    if name in ('Category', 'EventCode', 'EventIdentifier', 'EventType', 'RecordNumber'):
        return R.choice([0, 1, 7, R.randint(0, 70000), None])
    if name == 'ComputerName':
        return R.choice(['pc', 'PC-2', '', None])
    if name == 'SourceName':
        return R.choice(['src', 'Service Control Manager', '', None])
    if name == 'User':
        return R.choice([None, None, 'NT AUTHORITY\\SYSTEM', 'bob', ''])
    if name == 'Message':
        return R.choice(['m', 'a, b\nc', '', None])
    return R.choice(['x', '', None, 'Information', 'Error'])


def com_apply(module, parameters=None):
    try:
        with open(os.devnull, 'w') as sink, contextlib.redirect_stderr(sink), contextlib.redirect_stdout(sink):
            return ser(module.apply(parameters))
    except Exception as e:
        return {'raises': f'{type(e).__name__}: {e}'}


def com_cases(count):
    cases = []
    for index in range(count):
        items = [com_item(CALENDAR, 0.03, 0.03) for _ in range(R.randint(0, 4))]
        reminders = R.choice([None, None, True, False])
        COM.clear()
        COM[9] = [ComObject(a) for a in items]
        parameters = {} if reminders is None else {'include_reminders': reminders}
        cases.append((f'calendar{index}', 'outlook_calendar',
                      {'items': [com_json(ComObject(a)) for a in items], 'include_reminders': reminders},
                      com_apply(outlook_calendar, parameters)))
    for index in range(count):
        inbox = [com_item(MAIL, 0.03, 0.02) for _ in range(R.randint(0, 4))]
        sent = [com_item(MAIL, 0.03, 0.02) for _ in range(R.randint(0, 3))]
        COM.clear()
        COM[6], COM[5] = [ComObject(a) for a in inbox], [ComObject(a) for a in sent]
        cases.append((f'mail{index}', 'outlook_mail_extractor',
                      {'sent': [com_json(ComObject(a)) for a in sent], 'inbox': [com_json(ComObject(a)) for a in inbox]},
                      com_apply(outlook_mail_extractor)))
    for index in range(count):
        rows = []
        for _ in range(R.randint(0, 5)):
            rows.append({name: wmi_value(name) for name in WMI if R.random() >= 0.005})
        COM.clear()
        COM['wmi'] = [WmiRow(p) for p in rows]
        cases.append((f'wmi{index}', 'windows_events', {'rows': [com_json(ComObject(p)) for p in rows]},
                      com_apply(windows_events)))
    for index in range(count // 10):
        user, mailbox = R.choice([None, None, '', 'me@example.org', 'Ann Lee']), R.choice([5, 6, 9, 3, 0])
        cases.append((f'connect{index}', 'mail_connect', {'user': user, 'mailbox': mailbox},
                      {'calls': mail_connect_calls(user, mailbox)}))
    return cases


class Recorder:
    """Records the COM calls made through it, as text."""

    def __init__(self, calls, name):
        self.calls, self.name = calls, name

    def __getattr__(self, attribute):
        return Recorder(self.calls, attribute)

    def __call__(self, *args):
        text = self.name + '(' + ', '.join(a.name if isinstance(a, Recorder) else repr(a) for a in args) + ')'
        self.calls.append(text)
        return Recorder(self.calls, text)


def mail_connect_calls(user, mailbox):
    calls = []
    dispatch, initialize = win32com.client.Dispatch, pythoncom.CoInitialize
    win32com.client.Dispatch = Recorder(calls, 'Dispatch')
    pythoncom.CoInitialize = Recorder(calls, 'CoInitialize')
    try:
        mail.connect(user, mailbox)
    finally:
        win32com.client.Dispatch, pythoncom.CoInitialize = dispatch, initialize
    return calls


class Response:
    def __init__(self, data):
        self.data = data
        self.status_code = 200

    def json(self):
        return self.data


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--count', type=int, default=600)
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/connector-bank.txt.gz')
    args = parser.parse_args()
    lines, summary = [], {}
    with tempfile.TemporaryDirectory() as directory:
        cases = browser_cases(args.count, directory)
    cases += database_cases(args.count // 2)
    cases += [github_case(index) for index in range(args.count)]
    cases += com_cases(args.count)
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
