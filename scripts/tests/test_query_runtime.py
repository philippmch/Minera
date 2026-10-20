import importlib.util
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

R=Path(__file__).resolve().parents[2];spec=importlib.util.spec_from_file_location('query_runtime',R/'scripts/query_runtime.py');runtime=importlib.util.module_from_spec(spec);spec.loader.exec_module(runtime)
def cell(value,kind='string'):
    return {'key':'field','value':str(value),'kind':kind,'container':'scalar',**({'typed_value':value} if kind in ('int','float','boolean') else {})}
def payload(sql,limit=1000):
    rows=[[cell('C1'),cell('Open'),cell(10,'int'),cell('2026-10-01T09:00:00Z','date'),cell(0,'int')],[cell('C1'),cell('Review α'),cell(20,'int'),cell('2026-10-01T10:00:00Z','date'),cell(1,'int')],[cell('C2'),cell('Close'),cell(30,'int'),cell('2026-10-02T09:00:00Z','date'),cell(2,'int')]]
    return {'sql':sql,'max_rows':limit,'source_id':'fixture-source','data':{'event_index_column':'__minera_event_index','table':{'columns':['case:concept:name','activity','cost','timestamp','__minera_event_index'],'rows':rows}}}
class QueryRuntimeTests(unittest.TestCase):
    def test_real_duckdb_aggregates_and_unicode(self):
        result=runtime.query(payload('SELECT COUNT(*) AS events,SUM(cost) AS total FROM dataframe'))
        self.assertEqual(result['rows'],[[3,60]])
        selected=runtime.query(payload("SELECT * FROM dataframe WHERE activity='Review α'"),True)
        self.assertEqual(selected,{'indices':[1]})
    def test_windows_joins_and_empty_selection(self):
        result=runtime.query(payload('WITH ranked AS (SELECT activity,ROW_NUMBER() OVER (ORDER BY cost DESC) AS n FROM dataframe) SELECT activity FROM ranked WHERE n=1'))
        self.assertEqual(result['rows'],[['Close']])
        result=runtime.query(payload('SELECT a.__minera_event_index FROM dataframe a JOIN dataframe b ON a.activity=b.activity'))
        self.assertEqual(result['selected_event_indices'],[0,1,2])
        self.assertEqual(runtime.query(payload('SELECT * FROM dataframe WHERE FALSE'),True),{'indices':[]})
    def test_dates_and_precise_numeric_results(self):
        result=runtime.query(payload('SELECT EPOCH(timestamp) FROM dataframe ORDER BY __minera_event_index LIMIT 1'))
        self.assertEqual(result['rows'],[[1790845200.0]])
        result=runtime.query(payload('SELECT 18446744073709551615::HUGEINT AS large,12.345::DECIMAL(8,3) AS exact'))
        self.assertEqual(result['rows'],[[{'type':'integer','value':'18446744073709551615'},{'type':'decimal','value':'12.345'}]])
        result=runtime.query(payload('SELECT timestamp FROM dataframe LIMIT 1'))
        self.assertEqual(result['rows'][0][0]['type'],'date')
    def test_limits_never_produce_a_partial_filter(self):
        result=runtime.query(payload('SELECT * FROM dataframe',1));self.assertTrue(result['output_truncated']);self.assertTrue(result['can_filter']);self.assertEqual(result['selected_event_indices'],[])
        with self.assertRaises(ValueError):runtime.query(payload('SELECT * FROM dataframe',1),True)
        with self.assertRaises(ValueError):runtime.query(payload('SELECT COUNT(*) FROM dataframe'),True)
    def test_mutation_multistatement_and_external_data_are_rejected(self):
        for sql in ('DELETE FROM dataframe','DROP TABLE dataframe','SELECT 1; SELECT 2',"SELECT * FROM read_csv('unapproved.csv')",'INSTALL httpfs'):
            with self.subTest(sql=sql):
                with self.assertRaises(Exception):runtime.query(payload(sql))
    def test_nullable_cells_bool_and_blob_values(self):
        self.assertIsNone(runtime.scalar(None));self.assertIsNone(runtime.scalar({'kind':'na','value':'NA'}));self.assertEqual(runtime.scalar({'kind':'string','value':''}),'')
        self.assertEqual(runtime.scalar(cell(True,'boolean')),True)
        with self.assertRaises(ValueError):runtime.scalar({'kind':'boolean','value':'unknown'})
        self.assertEqual(runtime.value_json(b'\x00\xff'),{'type':'blob','value':'00ff'})
        self.assertEqual(runtime.value_json(float('nan')),{'type':'float','value':'NaN'})
    def test_selection_rejects_fabricated_out_of_range_indices(self):
        result=runtime.query(payload('SELECT 999 AS __minera_event_index'));self.assertFalse(result['can_filter'])
        with self.assertRaises(ValueError):runtime.query(payload('SELECT 999 AS __minera_event_index'),True)
        ambiguous=runtime.query(payload('SELECT __minera_event_index,__minera_event_index FROM dataframe'));self.assertFalse(ambiguous['can_filter']);self.assertEqual(ambiguous['selected_event_indices'],[])
    def test_source_metadata_is_unchanged(self):
        input=payload('SELECT * FROM dataframe');before=json.dumps(input,sort_keys=True);runtime.query(input);self.assertEqual(json.dumps(input,sort_keys=True),before)

    def test_exact_table_alias_and_ordered_native_parameters(self):
        request=payload('SELECT ? AS second,? AS first,? AS again FROM " literal,? table " LIMIT 1')
        request.pop('max_rows');request['exact_options']={'table_name':' literal,? table ','max_rows':'1','parameters':[{'index':1,'value':'9223372036854775807'},{'index':1,'value':'-9223372036854775808'},{'index':1,'value':'9223372036854775807'}]}
        before=json.dumps(request,sort_keys=True)
        result=runtime.query(request)
        self.assertEqual(result['rows'],[[{'type':'integer','value':'9223372036854775807'},{'type':'integer','value':'-9223372036854775808'},{'type':'integer','value':'9223372036854775807'}]])
        self.assertEqual(json.dumps(request,sort_keys=True),before)
    def test_exact_blob_extension_null_string_and_ieee_values(self):
        request=payload('SELECT ?,?,?,?,?');request.pop('max_rows')
        request['exact_options']={'table_name':'dataframe','max_rows':1000,'parameters':[{'index':0,'value':None},{'index':3,'value':' A,?\u0000 '},{'index':4,'value':{'hex':'00ffEDa080'}},{'index':2,'value':'INF'},{'index':2,'value':-0.0}]}
        row=runtime.query(request)['rows'][0]
        self.assertEqual(row[:3],[None,' A,?\u0000 ',{'type':'blob','value':'00ffeda080'}])
        self.assertEqual(row[3],{'type':'float','value':'Infinity'})
        self.assertEqual(runtime.exact_parameter({'index':4,'value':{'hex':''}}),b'')
    def test_exact_budget_inapplicability_and_strict_constructor_types(self):
        base={'table_name':'dataframe','max_rows':1000,'parameters':[]}
        for bad in ({**base,'max_vm_steps':50000000},{**base,'parameters':[{'index':1,'value':True}]},{**base,'parameters':[{'index':4,'value':{'hex':'0 ff'}}]},{**base,'parameters':[{'index':3,'value':{'native_bytes':'hex-v1','data':'ff'}}]},{**base,'max_rows':0}):
            with self.subTest(bad=bad),self.assertRaises((ValueError,UnicodeError)):runtime.exact_options(bad)
    def test_exact_selection_honors_same_row_limit_and_parameters(self):
        request=payload('SELECT * FROM " alias " WHERE cost>=?');request.pop('max_rows');request['exact_options']={'table_name':' alias ','max_rows':1,'parameters':[{'index':1,'value':'10'}]}
        result=runtime.query(request);self.assertTrue(result['output_truncated']);self.assertEqual(result['selected_event_indices'],[])
        with self.assertRaises(ValueError):runtime.query(request,True)
        request['exact_options']['max_rows']=1000
        self.assertEqual(runtime.query(request,True),{'indices':[0,1,2]})

    def test_exact_call_metadata_observes_function_invocation_on_success_and_error(self):
        with patch.object(runtime,'query',return_value={'family':'LOG_QUERY','rows':[]}) as call:
            status,value=runtime.observed_exact_query({'exact_options':{}},False)
            call.assert_called_once_with({'exact_options':{}},False)
            self.assertEqual(status,200);self.assertEqual(value['exact_call'],{'entered':True,'returned':True,'error':None})
        with patch.object(runtime,'query',side_effect=ValueError('fixture rejection')) as call:
            status,value=runtime.observed_exact_query({'exact_options':{}},True)
            call.assert_called_once_with({'exact_options':{}},True)
            self.assertEqual(status,400);self.assertEqual(value['exact_call'],{'entered':True,'returned':False,'error':{'type':'ValueError','message':'fixture rejection'}})

if __name__=='__main__':unittest.main()
