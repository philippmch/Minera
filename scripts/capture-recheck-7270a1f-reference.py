#!/usr/bin/env python3
"""Fuzz cases and public PM4Py 2.7.22 results for the recheck of 7270a1f.

Run with the pinned reference environment (pm4py 2.7.22, pandas 3.0.5,
numpy 2.4.6). Only public calls are made.

  fuzz-pnml OUT SEED N       PNML models whose transitions carry toolspecific
                             elements of many tools, tags and namespaces, with
                             stochastic properties of every key, type and
                             parameter text (O3, O4)
  pnml CASES OUT             public PNML imports of those models, with and
                             without the stochastic map (.jsonl)
  fuzz-run-keys OUT SEED N   consecutive activity/case grouping whose object
                             or float64 key columns hold NaN, None, pd.NA,
                             NaT, infinities and integers too large for a
                             float, next to typed int64/uint64/float64/bool
                             columns (O1, O2), in the token format of
                             src/backend/tests/test_recheck_7270a1f.cpp
  fuzz-dtypes OUT SEED N     the same grouping over two typed key columns of
                             numpy's bool, intN, uintN and floatN dtypes and
                             pandas' masked boolean, IntN, UIntN and FloatN
                             dtypes, with wrapping, rounding and pd.NA keys (O1)
  filters CASES OUT          public results for those cases (.jsonl), as
                             scripts/capture-recheck-d5b33ef-reference.py
                             computes them
  fuzz-typed OUT SEED N MODE the pandas sequence and case filters over typed
                             columns: MODE case types the case column (str
                             activities), activity types both, mixed also
                             mixes str with other objects in object activity
                             columns
  typed-probes OUT           fixed cases for the rules those cases found:
                             int64 case ids that wrap as a range, float16 case
                             columns per filter and activity dtype, masked
                             integer ids one double holds
  typed CASES OUT            public results for typed cases (.jsonl); a case
                             whose error names a numpy uint64 scalar type is
                             rerun in a fresh process, as numpy names that
                             type (uint64 or ulonglong) by what the process
                             converted before
"""
import contextlib
import io
import json
import random
import subprocess
import sys
import tempfile
import warnings
from pathlib import Path


# ---------------------------------------------------------------- PNML

TOOLS = ['StochasticPetriNet'] * 6 + ['ProM', 'ProM', 'Other', 'ProMStochasticPetriNet', ' StochasticPetriNet ',
                                      'stochasticpetrinet', None, '', 'WoPeD', 'xStochasticPetriNetx', 'StochasticPetriNe']
TAGS = ['toolspecific'] * 8 + ['x:toolspecific', 'mytoolspecific', 'toolSpecific', 'toolspecificx']
ACTIVITIES = [None, '$invisible$', 'a', 'x$invisible$y']
TYPES = ['IMMEDIATE', 'DETERMINISTIC', 'NORMAL', 'UNIFORM', 'LOGNORMAL', 'GAMMA', 'EXPONENTIAL'] * 3 + [
    'EXPONENTIAL ', 'exponential', 'BETA', 'UNDEFINED', '']
PARAMETERS = ['', '1.0', '2;3', '2;3;4', '2;3;4;5', '2,3', '2 3', '2;', ';2', 'junk', 'nan', 'inf', '-inf', '1e999', '-1',
              '0', '-0.0', '1_0', '٣', ' 2 ; 3 ', '0.5;0.25', '1;nan', '1;2;inf', '5e-324', '1e-320',
              '1.7976931348623157e308', '49', '3;1', '-2;3', '2;-3', '1;2;-3', '0;1;1', '1e308;1e308', '2.5', '1;2;3',
              '0.25;2;1', '4;1;2', '2;0;1', '0.5;0;1']
VALUES = {'priority': ['0', '3', '-1', '', 'x', '1_0', '٣', '99999999999', ' 7 '],
          'weight': ['1.0', '7', '0', '-1', '', 'x', 'nan', 'inf', '1_0.5', '2.5'],
          'invisible': ['true', 'false', 'TRUE', ' true', '', 'yes'],
          'other': ['x', '']}


def escape(text):
    return text.replace('&', '&amp;').replace('<', '&lt;').replace('"', '&quot;')


def pnml_property(rnd):
    key = rnd.choice(['distributionType'] * 3 + ['distributionParameters'] * 3 +
                     ['priority', 'weight', 'invisible', 'other', '', None])
    if key == 'distributionType': value = rnd.choice(TYPES)
    elif key == 'distributionParameters': value = rnd.choice(PARAMETERS)
    elif key in VALUES: value = rnd.choice(VALUES[key])
    else: value = 'x'
    tag = rnd.choice(['property'] * 6 + ['foo', 'x:property'])
    attribute = '' if key is None else f' key="{escape(key)}"'
    element = f'<{tag}{attribute}>{escape(value)}</{tag}>' if value else f'<{tag}{attribute}/>'
    return f'<wrap>{element}</wrap>' if rnd.random() < 0.05 else element


def toolspecific(rnd, typical):
    if typical:
        tool, tag = 'StochasticPetriNet', 'toolspecific'
        properties = [f'<property key="distributionType">{escape(rnd.choice(TYPES))}</property>']
        if rnd.random() < 0.9:
            properties.append(f'<property key="distributionParameters">{escape(rnd.choice(PARAMETERS))}</property>')
        for key in ['priority', 'invisible', 'weight']:
            if rnd.random() < 0.7:
                properties.append(f'<property key="{key}">{escape(rnd.choice(VALUES[key][:3]))}</property>')
        if rnd.random() < 0.3: rnd.shuffle(properties)
        body = ''.join(properties)
    else:
        tool, tag = rnd.choice(TOOLS), rnd.choice(TAGS)
        body = ''.join(pnml_property(rnd) for _ in range(rnd.randint(0, 6)))
    attributes = ''
    if tool is not None: attributes += f' tool="{escape(tool)}"'
    if tool and 'ProM' in tool:
        activity = rnd.choice(ACTIVITIES)
        if activity is not None: attributes += f' activity="{escape(activity)}"'
    elif rnd.random() < 0.1:
        attributes += ' activity="$invisible$"'
    attributes += ' version="0.2"'
    return f'<{tag}{attributes}>{body}</{tag}>' if body or rnd.random() < 0.5 else f'<{tag}{attributes}/>'


def pnml_transition(rnd, index):
    children = []
    for _ in range(rnd.choice([1, 1, 1, 2, 2, 3])):
        r = rnd.random()
        if r < 0.55: children.append(toolspecific(rnd, True))
        elif r < 0.93: children.append(toolspecific(rnd, False))
        else: children.append(pnml_property(rnd))
    return f'<transition id="t{index}"><name><text>A{index}</text></name>{"".join(children)}</transition>'


def pnml_model(rnd):
    count = rnd.choice([1, 1, 2])
    transitions = ''.join(pnml_transition(rnd, i) for i in range(count))
    arcs = ''.join(f'<arc id="a{i}" source="p0" target="t{i}"/><arc id="b{i}" source="t{i}" target="p1"/>'
                   for i in range(count))
    return ('<pnml xmlns:x="urn:x"><net id="n" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel">'
            '<page id="page"><place id="p0"><name><text>p0</text></name><initialMarking><text>1</text></initialMarking>'
            '</place><place id="p1"><name><text>p1</text></name></place>' + transitions + arcs +
            '</page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>')


def fuzz_pnml(out, seed, n):
    rnd = random.Random(seed)
    Path(out).write_text(json.dumps([{'id': f'pnml{seed}_{i}', 'model': pnml_model(rnd)} for i in range(n)]))


def import_pnml(xml, stochastic):
    from pm4py.objects.petri_net.importer import importer
    key = importer.Variants.PNML.value.Parameters.RETURN_STOCHASTIC_MAP
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / 'model.pnml'
        path.write_text(xml, encoding='utf-8')
        try:
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()), \
                    warnings.catch_warnings():
                warnings.simplefilter('ignore')
                result = importer.apply(str(path), parameters={key: stochastic})
        except Exception as e:
            return {'ok': False, 'type': type(e).__name__, 'error': str(e)}
    record = {'ok': True, 'labels': sorted([t.name, t.label] for t in result[0].transitions)}
    if stochastic:
        record['smap'] = {t.name: {'type': v.get_distribution_type(), 'params': v.get_distribution_parameters(),
                                   'weight': v.get_weight(), 'priority': v.get_priority(),
                                   'ttype': v.get_transition_type()} for t, v in result[3].items()}
    return record


def pnml(cases_path, out):
    with open(out, 'w') as handle:
        for case in json.loads(Path(cases_path).read_text()):
            record = {'id': case['id'], 'plain': import_pnml(case['model'], False),
                      'stochastic': import_pnml(case['model'], True)}
            handle.write(json.dumps(record, ensure_ascii=False, default=repr) + '\n')


# ---------------------------------------------------------------- run keys

FLOATS = ['Fnan', 'Finf', 'F-inf', 'F1e308', 'F1.7976931348623157e308', 'F-1e308', 'F5e-324', 'F0.0', 'F-0.0', 'F1.5',
          'F-1.5', 'F9007199254740993']
INTS = ['I0', 'I1', 'I-1', 'I9223372036854775807', 'I-9223372036854775808', 'I9007199254740993']
HUGE = ['J' + '9' * 400, 'J-' + '9' * 400, 'J1' + '0' * 308, 'J' + '9' * 309, 'J' + '9' * 308]
TYPED = {'int64': INTS + ['I9223372036854775806', 'I4611686018427387904', 'I2'],
         'uint64': ['U0', 'U1', 'U2', 'U18446744073709551615', 'U9223372036854775808', 'U9223372036854775807'],
         'float64': FLOATS, 'bool': ['B0', 'B1']}
MISSING = ['N', 'Fnan', 'A', 'DNaT']
OBJECTS = INTS + FLOATS[:4] + HUGE + MISSING + ['J9223372036854775808', 'J18446744073709551616', 'B1', 'B0', 'Sx',
                                                 'D2035-09-08T12:00:00Z']


def fuzz_run_keys(out, seed, n):
    rnd = random.Random(seed)
    cases = []
    for i in range(n):
        mode = rnd.choice(['object', 'object', 'huge', 'float64', 'mixed', 'mixed'])
        typed = rnd.choice(list(TYPED))
        object_first = rnd.random() < 0.5
        rows = []
        for j in range(rnd.randint(1, 8)):
            if mode == 'object':
                c, a, cd, ad = rnd.choice(OBJECTS), rnd.choice(OBJECTS), 'object', 'object'
            elif mode == 'huge':
                c = rnd.choice(HUGE + INTS[:2] + MISSING)
                a = rnd.choice(['F1.5', 'Fnan', 'Finf', 'I1', 'F0.0', 'N', 'A', 'DNaT', 'Sx', 'B1'])
                cd, ad = 'object', 'object'
                if rnd.random() < 0.5: c, a = a, c
            elif mode == 'float64':
                c, a, cd, ad = rnd.choice(FLOATS), rnd.choice(TYPED[typed]), 'float64', typed
            else:
                o, v = rnd.choice(OBJECTS), rnd.choice(TYPED[typed])
                c, a, cd, ad = (o, v, 'object', typed) if object_first else (v, o, typed, 'object')
            if rows and rnd.random() < 0.3: c, a = rows[-1][0], rows[-1][1]
            rows.append([c, a, 'D2035-09-08T12:00:%02dZ' % rnd.randint(0, 5), 'Spayload%d' % j, 'Sindex%d' % j])
        cases.append({'id': f'runkey{seed}_{i}_{cd}_{ad}', 'op': rnd.choice(['group_act', 'group_act_last']), 'lo': 'N',
                      'hi': 'N', 'keys': ['ticket', 'step', 'stamp'], 'cdtype': cd, 'adtype': ad, 'rows': rows})
    Path(out).write_text(json.dumps(cases))


RANGES = {'int8': (-128, 127), 'int16': (-2 ** 15, 2 ** 15 - 1), 'int32': (-2 ** 31, 2 ** 31 - 1),
          'int64': (-2 ** 63, 2 ** 63 - 1), 'uint8': (0, 255), 'uint16': (0, 2 ** 16 - 1), 'uint32': (0, 2 ** 32 - 1),
          'uint64': (0, 2 ** 64 - 1)}
NARROW_FLOATS = ['F0.0', 'F1.0', 'F0.1', 'F1.5', 'F16777217.0', 'F3.4028234663852886e38', 'F1e39', 'F65504.0', 'F65520.0',
                 'F-0.0', 'F2.5', 'F0.30000000000000004', 'Fnan', 'Finf', 'F-inf', 'F2049.0', 'F0.000061035']
DTYPES = list(RANGES) + ['float16', 'float32', 'float64', 'bool', 'Int8', 'Int16', 'Int32', 'Int64', 'UInt8', 'UInt16',
                         'UInt32', 'UInt64', 'Float32', 'Float64', 'boolean']


def dtype_token(rnd, dtype):
    if dtype.lower() in RANGES:
        lo, hi = RANGES[dtype.lower()]
        value = rnd.choice([lo, hi, 0, 1, 2, -1 if lo < 0 else 3, rnd.randint(lo, hi), hi - 1, lo + 1])
        token = ('U' if dtype.lower().startswith('u') else 'I') + str(value)
    elif dtype.lower().startswith('float'): token = rnd.choice(NARROW_FLOATS)
    else: token = rnd.choice(['B0', 'B1'])
    return 'A' if dtype[0].isupper() and rnd.random() < 0.15 else token


def fuzz_dtypes(out, seed, n):
    rnd = random.Random(seed)
    cases = []
    for i in range(n):
        cd = rnd.choice(DTYPES)
        ad = rnd.choice(DTYPES) if rnd.random() < 0.6 else cd
        rows = []
        for j in range(rnd.randint(2, 8)):
            c, a = dtype_token(rnd, cd), dtype_token(rnd, ad)
            if rows and rnd.random() < 0.3: c, a = rows[-1][0], rows[-1][1]
            rows.append([c, a, 'D2035-09-08T12:00:%02dZ' % rnd.randint(0, 5), 'Spayload%d' % j, 'Sindex%d' % j])
        cases.append({'id': f'dtype{seed}_{i}_{cd}_{ad}', 'op': rnd.choice(['group_act', 'group_act_last']), 'lo': 'N',
                      'hi': 'N', 'keys': ['ticket', 'step', 'stamp'], 'cdtype': cd, 'adtype': ad, 'rows': rows})
    Path(out).write_text(json.dumps(cases))


# ---------------------------------------------------------------- typed columns

TYPED_OPS = ['rework', 'suffix', 'prefix', 'split', 'size', 'ncases', 'group_time', 'group_time_last', 'group_act',
             'group_act_last', 'performance_us', 'prefix_last', 'suffix_last', 'start', 'end', 'start_nocc', 'end_nocc',
             'trace', 'starts', 'ends']


def typed_case(case_id, op, cdtype, adtype, rows, lo, hi='N'):
    return {'id': case_id, 'op': op, 'lo': lo, 'hi': hi, 'keys': ['ticket', 'step', 'stamp'], 'cdtype': cdtype,
            'adtype': adtype, 'rows': [[c, a, 'D2033-04-02T06:15:%02dZ' % (j % 60), 'Sp%d' % j, 'Srow%d' % j]
                                       for j, (c, a) in enumerate(rows)]}


def typed_bounds(op, lo):
    hi = 'N'
    if op == 'size': lo, hi = 'I1', 'I3'
    if op == 'ncases': hi = 'I5'
    if op == 'performance_us': lo, hi = 'I0', 'I30'
    if op in ('split', 'trace', 'starts', 'ends') and not lo.startswith('L'): lo = 'L' + lo
    if op in ('start_nocc', 'end_nocc'): lo = 'I1'
    return lo, hi


def fuzz_typed(out, seed, n, mode):
    rnd = random.Random(seed)
    cases = []

    # Filter values the native API holds: None for pd.NA, int64 integers.
    def held(token):
        if token == 'A': return 'N'
        if token[0] == 'U': return 'I' + token[1:] if int(token[1:]) < 2 ** 63 else 'I0'
        return token

    for i in range(n):
        op = rnd.choice(TYPED_OPS)
        cd = rnd.choice(DTYPES + ['object'])
        ad = rnd.choice(DTYPES) if mode != 'case' and rnd.random() < 0.7 else 'object'
        pool = [dtype_token(rnd, cd) if cd != 'object' else 'S' + rnd.choice('xyz') for _ in range(rnd.randint(1, 4))]
        apool = [dtype_token(rnd, ad) for _ in range(3)] if ad != 'object' else ['SA', 'SB', 'SC']
        if ad == 'object' and mode == 'mixed' and rnd.random() < 0.5:
            apool = ['SA', 'SB'] + rnd.sample(['I1', 'F1.5', 'B1', 'N', 'Fnan', 'A', 'SC', 'I-3'], 2)
        rows = [[rnd.choice(pool), rnd.choice(apool), 'D2033-04-02T06:15:%02dZ' % rnd.randint(0, 59), 'Sp%d' % j,
                 'Srow%d' % j] for j in range(rnd.randint(0 if rnd.random() < 0.05 else 1, 9))]
        lo, hi = rnd.choice(apool), 'N'
        if op.startswith(('prefix', 'suffix')): hi = rnd.choice(['B1', 'B0', 'N'])
        if op == 'size': lo, hi = 'I1', 'I3'
        if op == 'ncases': hi = 'I' + str(rnd.randint(0, 3))
        if op == 'performance_us': lo, hi = 'I0', 'I30'
        if op in ('split', 'trace', 'starts', 'ends'):
            lo = 'L' + ','.join(rnd.choice(apool) for _ in range(rnd.randint(1, 2)))
        if op in ('start_nocc', 'end_nocc'): lo = 'I' + str(rnd.randint(0, 3))
        lo = 'L' + ','.join(held(x) for x in lo[1:].split(',')) if lo.startswith('L') else held(lo)
        cases.append({'id': f'typed{seed}_{i}_{op}_{cd}_{ad}', 'op': op, 'lo': lo, 'hi': hi,
                      'keys': ['ticket', 'step', 'stamp'], 'cdtype': cd, 'adtype': ad, 'rows': rows})
    Path(out).write_text(json.dumps(cases))


def typed_probes(out):
    cases = []
    m, big = -2 ** 63, 2 ** 64
    # int64 case ids stepping evenly with wraparound, and controls.
    ranges = {'m1': [m, 1], 'm0m': [m, 0, m], 'neg': [2 ** 63 - 1, m], 'same_len': [0, 2 ** 63 - 1, -2],
              'sub': [m, 5, 1], 'ok': [m, -1], 'three': [m, 2, 3]}
    for op in TYPED_OPS:
        for name, ids in ranges.items():
            acts = ['SA', 'SB', 'SA'][:len(ids)]
            lo, hi = typed_bounds(op, 'SA')
            cases.append(typed_case(f'range_{op}_{name}', op, 'int64', 'object', [('I%d' % k, a) for k, a in zip(ids, acts)], lo, hi))
            cases.append(typed_case(f'range_{op}_{name}_object', op, 'object', 'object',
                                    [('I%d' % k, a) for k, a in zip(ids, acts)], lo, hi))
    # float16 case columns per filter and activity dtype, empty and not.
    tokens = {'object': ['SA', 'SB'], 'int64': ['I1', 'I2'], 'Int16': ['I1', 'A'], 'float64': ['F1.5', 'F2.5'],
              'bool': ['B1', 'B0'], 'boolean': ['B1', 'A'], 'float16': ['F1.5', 'F2.5'], 'UInt8': ['I1', 'I2']}
    for op in TYPED_OPS:
        for ad, pair in tokens.items():
            for n in [0, 2]:
                lo, hi = typed_bounds(op, pair[0] if pair[0] != 'A' else 'N')
                rows = [('F1.5' if j == 0 else 'F2.5', pair[j % 2]) for j in range(n)]
                cases.append(typed_case(f'float16_{op}_{ad}_{n}', op, 'float16', ad, rows, lo, hi))
    # Masked integer ids that one double holds, beside pd.NA.
    one = {'both': [('A', 'SA'), ('U%d' % (big - 1), 'SB'), ('U%d' % (big - 2), 'SB')],
           'both2': [('A', 'SA'), ('U%d' % (big - 1), 'SA'), ('U%d' % (big - 1), 'SB'), ('U%d' % (big - 2), 'SB')],
           'one': [('A', 'SA'), ('U%d' % (big - 1), 'SA'), ('U%d' % (big - 2), 'SB')],
           'one_rev': [('A', 'SA'), ('U%d' % (big - 2), 'SA'), ('U%d' % (big - 1), 'SB')],
           'no_na': [('U%d' % (big - 1), 'SA'), ('U%d' % (big - 2), 'SB')],
           'int': [('A', 'SA'), ('I%d' % (2 ** 63 - 1), 'SA'), ('I%d' % (2 ** 63 - 2), 'SB')]}
    for op in ['prefix', 'suffix', 'prefix_last', 'suffix_last']:
        for name, rows in one.items():
            cases.append(typed_case(f'double_{op}_{name}', op, 'Int64' if name == 'int' else 'UInt64', 'object', rows,
                                    'SB', 'B0'))
    Path(out).write_text(json.dumps(cases))


def typed(cases_path, out):
    import numpy as np
    import pandas as pd
    from pm4py.algo.filtering.pandas.activity_split import activity_split_filter
    from pm4py.algo.filtering.pandas.cases import case_filter
    from pm4py.algo.filtering.pandas.consecutive_act_case_grouping import consecutive_act_case_grouping_filter
    from pm4py.algo.filtering.pandas.end_activities import end_activities_filter
    from pm4py.algo.filtering.pandas.ends_with import ends_with_filter
    from pm4py.algo.filtering.pandas.prefixes import prefix_filter
    from pm4py.algo.filtering.pandas.rework import rework_filter
    from pm4py.algo.filtering.pandas.start_activities import start_activities_filter
    from pm4py.algo.filtering.pandas.starts_with import starts_with_filter
    from pm4py.algo.filtering.pandas.suffixes import suffix_filter
    from pm4py.algo.filtering.pandas.timestamp_case_grouping import timestamp_case_grouping_filter
    from pm4py.algo.filtering.pandas.traces import trace_filter
    sys.path.insert(0, str(Path(__file__).parent))
    d5 = __import__('capture-recheck-d5b33ef-reference')
    decode, simple = d5.decode, d5.simple

    def value(token):
        if token != 'N' and token[:1] in 'LT':
            items = [decode(x) for x in token[1:].split(',')] if token[1:] else []
            return items if token[0] == 'L' else tuple(items)
        return decode(token)

    def run(case):
        op, (ck, ak, tk), rows = case['op'], case['keys'], case['rows']
        df = pd.DataFrame({ck: pd.Series([decode(r[0]) for r in rows], dtype=case['cdtype']),
                           ak: pd.Series([decode(r[1]) for r in rows], dtype=case['adtype']),
                           tk: pd.Series([decode(r[2]) for r in rows], dtype=object),
                           'payload': pd.Series([decode(r[3]) for r in rows], dtype=object),
                           'row_id': pd.Series(range(len(rows)), dtype='int64')})
        unit = 'us' if op == 'performance_us' else 'ns'
        df[tk] = pd.to_datetime(df[tk], utc=True).astype('datetime64[' + unit + ', UTC]')
        df.index = pd.Index([decode(row[4]) for row in rows], dtype=object, name='custom_idx')
        lo, hi = value(case['lo']), value(case['hi'])
        params = {'pm4py:param:case_id_key': ck, 'pm4py:param:activity_key': ak, 'pm4py:param:timestamp_key': tk}
        if case['hi'] != 'N' and op.startswith(('prefix', 'suffix')): params['strict'] = hi
        if op.endswith('_last'): params['first_or_last'] = 'last'
        if op in ('group_act_last', 'group_time_last'): params['filter_type'] = 'last'
        if op == 'rework': o = rework_filter.apply(df, lo, parameters=params)
        elif op == 'split': o = activity_split_filter.apply(df, lo, parameters=params)
        elif op == 'size': o = case_filter.filter_on_case_size(df, case_id_glue=ck, min_case_size=lo, max_case_size=hi)
        elif op == 'ncases': o = case_filter.filter_on_ncases(df, case_id_glue=ck, max_no_cases=hi)
        elif op == 'performance_us':
            o = case_filter.filter_on_case_performance(df, case_id_glue=ck, timestamp_key=tk, min_case_performance=lo,
                                                       max_case_performance=hi)
        elif op.startswith('prefix'): o = prefix_filter.apply(df, lo, parameters=params)
        elif op.startswith('suffix'): o = suffix_filter.apply(df, lo, parameters=params)
        elif op.startswith('group_time'): o = timestamp_case_grouping_filter.apply(df, parameters=params)
        elif op.startswith('group_act'): o = consecutive_act_case_grouping_filter.apply(df, parameters=params)
        elif op == 'start': o = start_activities_filter.apply(df, [lo], parameters=params)
        elif op == 'end': o = end_activities_filter.apply(df, [lo], parameters=params)
        elif op == 'start_nocc':
            o = start_activities_filter.filter_df_on_start_activities_nocc(df, lo, case_id_glue=ck, activity_key=ak)
        elif op == 'end_nocc':
            o = end_activities_filter.filter_df_on_end_activities_nocc(df, lo, case_id_glue=ck, activity_key=ak)
        elif op == 'trace': o = trace_filter.apply(df, [lo], parameters=params)
        elif op == 'starts': o = starts_with_filter.apply(df, [lo], parameters=params)
        else: o = ends_with_filter.apply(df, [lo], parameters=params)
        if isinstance(o, tuple): o = o[0]
        return {'columns': list(o.columns), 'row_ids': list(o['row_id']), 'index': [simple(x) for x in o.index],
                'index_name': o.index.name, 'rows': [[simple(x) for x in row] for row in o.itertuples(index=False, name=None)],
                'dtypes': [str(x) for x in o.dtypes]}

    def result(case):
        try:
            return {'id': case['id'], 'status': 'OK', 'result': run(case)}
        except Exception as e:
            return {'id': case['id'], 'status': type(e).__name__, 'error': str(e)}

    cases = json.loads(Path(cases_path).read_text())
    if out == '-':
        # One case in a fresh process (see the command list).
        with warnings.catch_warnings():
            warnings.simplefilter('ignore')
            print(json.dumps(result(cases[0]), ensure_ascii=False))
        return
    with open(out, 'w') as handle, warnings.catch_warnings(), tempfile.TemporaryDirectory() as scratch:
        warnings.simplefilter('ignore')
        for case in cases:
            record = result(case)
            if 'numpy.uint64 found' in record.get('error', '') or 'numpy.ulonglong found' in record.get('error', ''):
                single = Path(scratch) / 'case.json'
                single.write_text(json.dumps([case]))
                record = json.loads(subprocess.run([sys.executable, __file__, 'typed', str(single), '-'], check=True,
                                                   capture_output=True, text=True).stdout)
            handle.write(json.dumps(record, ensure_ascii=False) + '\n')


def filters(cases_path, out):
    sys.path.insert(0, str(Path(__file__).parent))
    __import__('capture-recheck-d5b33ef-reference').filters(cases_path, out)


def main():
    command, args = sys.argv[1], sys.argv[2:]
    if command == 'fuzz-pnml': fuzz_pnml(args[0], int(args[1]), int(args[2]))
    elif command == 'pnml': pnml(*args)
    elif command == 'fuzz-run-keys': fuzz_run_keys(args[0], int(args[1]), int(args[2]))
    elif command == 'fuzz-dtypes': fuzz_dtypes(args[0], int(args[1]), int(args[2]))
    elif command == 'filters': filters(*args)
    elif command == 'fuzz-typed': fuzz_typed(args[0], int(args[1]), int(args[2]), args[3])
    elif command == 'typed-probes': typed_probes(args[0])
    elif command == 'typed': typed(*args)
    else: raise SystemExit(__doc__)


if __name__ == '__main__':
    main()
