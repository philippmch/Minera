import re, sys
o = open('src/backend/tests/test_pandas_timestamp_filter.cpp').read()
t = open('src/backend/tests/pandas_timestamp_workbench_public_vectors.inc').read()
oc = re.split(r'\nMINERA_NOINLINE void retained_case_(\d+)\(', o)[1:]
tc = re.split(r'\nMINERA_NOINLINE void pandas_timestamp_transport_(\d+)\(', t)[1:]
oc = dict(zip(oc[0::2], oc[1::2])); tc = dict(zip(tc[0::2], tc[1::2]))
bad = 0; errs = 0; tables = 0
for k in oc:
    a, b = oc[k], tc[k]
    ain = re.search(r'input=snapshot_(\d+)\(\)', a).group(1); bin_ = re.search(r'source=snapshot_(\d+)\(\)', b).group(1)
    aexp = re.search(r'expected=snapshot_(\d+)', a); bexp = re.search(r'make_optional\(snapshot_(\d+)\(\)\)', b)
    aerr = re.search(r'e\.type==(bytes\("[0-9a-f]*"\)) && e\.message==(bytes\("[0-9a-f]*"\))', a)
    berr = re.findall(r'std::make_optional\((bytes\("[0-9a-f]*"\))\)', b)
    ok = ain == bin_ and (aexp.group(1) if aexp else None) == (bexp.group(1) if bexp else None)
    if aerr: ok = ok and list(aerr.groups()) == berr; errs += 1
    else: ok = ok and berr == []; tables += 1
    bad += not ok
print('pandas cases', len(oc), len(tc), 'mismatch', bad, 'errors', errs, 'tables', tables)
o = open('src/backend/tests/polars_timestamp_filter_public_vectors.inc').read()
t = open('src/backend/tests/polars_timestamp_workbench_public_vectors.inc').read()
oc = re.split(r'\nMINERA_TIMESTAMP_TEST_NOINLINE void retained_public_timestamp_(\d+)\(', o)[1:]
tc = re.split(r'\nMINERA_POLARS_TRANSPORT_NOINLINE void polars_timestamp_transport_(\d+)\(', t)[1:]
oc = dict(zip(oc[0::2], oc[1::2])); tc = dict(zip(tc[0::2], tc[1::2]))
bad = 0; errs = 0
for k in oc:
    a, b = oc[k], tc[k]
    ain = re.search(r'source=public_timestamp_frame_(\d+)\(\)', a).group(1); bin_ = re.search(r'source=public_timestamp_frame_(\d+)\(\)', b).group(1)
    aexp = re.search(r'same_frame\([^;]*?public_timestamp_frame_(\d+)\(\),std::string', a)
    bexp = re.search(r'make_optional\(public_timestamp_frame_(\d+)\(\)\)', b)
    m = re.search(r'error_is\(\[&\]\{\(void\).*?\},(std::string\("[^"]*",\d+\)),(std::string\("[^"]*",\d+\)),(std::string\("[^"]*",\d+\))', a)
    ok = ain == bin_ and (aexp.group(1) if aexp else None) == (bexp.group(1) if bexp else None)
    if m:
        errs += 1; ty, ph, msg = m.groups()
        ok = ok and ('std::make_optional(' + ty + ')') in b and ('std::make_optional(' + msg + ')') in b and (',' + ph + ',') in b
    bad += not ok
print('polars cases', len(oc), len(tc), 'mismatch', bad, 'errors', errs)
