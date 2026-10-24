"""Synthetic closed LOG timestamp integrity only; no reference/native imports."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('log_timestamp_receipt_index', Path(__file__).parents[1]/'retained_public_receipt_index.py')
M = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(M)
MODULE = 'pm4py.algo.filtering.log.timestamp.timestamp_filter'
ADAPTER = 'log-timestamp-packed-v1'
SCHEMA = 'minera-log-timestamp-observer-receipt-v1'


def scalar(value):
    return {'type':'float','hex':value.hex()} if type(value) is float else {'type':type(value).__name__,'value':value}


def sequence(*values,kind='list'): return {'type':kind,'values':list(values)}
def dictionary(*pairs): return {'type':'dict','items':[list(pair) for pair in pairs]}


def snapshot():
    event = dictionary((scalar('concept:name'),scalar('OWN\0Ω')),(scalar('wide'),scalar(9007199254740993)))
    return {'attributes':dictionary((scalar('bank'),scalar('synthetic'))),'extensions':dictionary(),
        'classifiers':dictionary(),'omni_present':dictionary(),'properties':dictionary(),
        'traces':[{'attributes':dictionary(),'properties':dictionary(),'events':[event]}]}


def spec(role='observations',adapter=ADAPTER):
    return {'adapter':adapter,'receipt_schema':SCHEMA,'role':role,'provenance':'own-public-observation',
            'reference':{'commit':'synthetic'},'runtime':{'python':'synthetic'}}


def rebind(row):
    row['receipt_sha256'] = M.sha(M.canonical(row['receipt']))
    return row


def row_for(function='apply_events',error=False):
    source = snapshot();predicate = function in M.LOG_TIMESTAMP_PREDICATES
    first = {'type':'trace','snapshot':copy.deepcopy(source['traces'][0]),'source_trace_index':0} if predicate else {'type':'log','snapshot':copy.deepcopy(source)}
    args = [first]
    if function not in {'apply','apply_auto_filter'}:
        if function in {'has_attribute_in_timeframe','filter_traces_attribute_in_timeframe'}:
            args += [scalar('kind'),scalar('keep')]
        args += [scalar('2026-01-01 00:00:00'),scalar('2026-01-02 00:00:00')]
        if predicate: args += [scalar('time:timestamp')]
    receipt = {'module':MODULE,'function':function,'args':sequence(*args),'kwargs':dictionary(),
               'call_shape':'positional_args_keyword_controls'}
    row = {'index':1,'module':MODULE,'case':{'name':'OWN synthetic','function':function},'receipt':receipt,'receipt_sha256':'',
        'input_before':copy.deepcopy(source),'input_after':copy.deepcopy(source),
        'controls_before':{'args':copy.deepcopy(receipt['args']),'kwargs':copy.deepcopy(receipt['kwargs'])},
        'controls_after':{'args':copy.deepcopy(receipt['args']),'kwargs':copy.deepcopy(receipt['kwargs'])},
        'input_unchanged':True,'controls_unchanged':True,'warnings':[]}
    if error:
        row['error'] = {'type':'builtins.TypeError','message':'full\0Ω\nerror','args':sequence(scalar('full\0Ω'),kind='tuple')}
    elif predicate:
        row.update(result_kind='value',value_result=scalar(True))
    else:
        row.update(result_kind='log',result=copy.deepcopy(source),returned_input_identity=False,
                   trace_aliases_source=[False],event_source_positions=[[]])
    return rebind(row)


def mutate_source(row):
    row = copy.deepcopy(row)
    row['input_after']['traces'][0]['events'][0]['items'].append([scalar('case:concept:name'),scalar('actual mutation')])
    first = row['controls_after']['args']['values'][0]
    first['snapshot'] = copy.deepcopy(row['input_after']['traces'][0] if first['type']=='trace' else row['input_after'])
    row['input_unchanged'] = row['controls_unchanged'] = False
    return row


def intent(row):
    return {'index':row['index'],'module':MODULE,'function':row['receipt']['function'],
            'case':row['case']['name'],'receipt_sha256':row['receipt_sha256']}


class LogTimestampAdapterTests(unittest.TestCase):
    def test_all13_functions_exact_canonical_and_outcomes(self):
        for function in M.LOG_TIMESTAMP_ARITIES:
            for error in (False,True):
                with self.subTest(function=function,error=error):
                    row = row_for(function,error)
                    module,actual,raw,digest,status = M.adapt(row,spec())
                    self.assertEqual((module,actual,digest,status),(MODULE,function,row['receipt_sha256'],'actual_outcome'))
                    self.assertEqual(raw,M.canonical(row['receipt']))
                    self.assertEqual(set(json.loads(raw)),{'module','function','args','kwargs','call_shape'})

    def test_predicate_is_real_indexed_trace(self):
        row = row_for('is_contained');row['receipt']['args']['values'][0]['source_trace_index'] = 2;rebind(row)
        with self.assertRaises(ValueError): M.adapt(row,spec())

    def test_predicate_requires_source_snapshot_equality(self):
        row = row_for('is_contained');row['input_before']['traces'][0]['properties'] = dictionary((scalar('changed'),scalar(True)))
        with self.assertRaises(ValueError): M.adapt(row,spec())

    def test_predicate_none_index_is_not_invented(self):
        row = row_for('is_contained');row['receipt']['args']['values'][0]['source_trace_index'] = None;rebind(row)
        with self.assertRaises(ValueError): M.adapt(row,spec())

    def test_filter_cannot_be_trace_or_wrong_arity(self):
        for change in ('trace','arity'):
            row = row_for()
            if change=='trace': row['receipt']['args']['values'][0] = row_for('is_contained')['receipt']['args']['values'][0]
            else: row['receipt']['args']['values'].pop()
            rebind(row)
            with self.assertRaises(ValueError): M.adapt(row,spec())

    def test_mutations_on_full_error_are_actual_outcomes(self):
        self.assertEqual(M.adapt(mutate_source(row_for(error=True)),spec())[-1],'actual_outcome')

    def test_full_after_controls_cannot_be_replaced_by_before(self):
        row = mutate_source(row_for(error=True));row['controls_after'] = copy.deepcopy(row['controls_before'])
        with self.assertRaises(ValueError): M.adapt(row,spec())

    def test_false_source_and_control_flags_block(self):
        for flag in ('input_unchanged','controls_unchanged'):
            row = mutate_source(row_for(error=True));row[flag] = True
            with self.assertRaises(ValueError): M.adapt(row,spec())

    def test_value_results_are_bool_predicates_only(self):
        row = row_for('is_contained');row['value_result'] = scalar(1)
        with self.assertRaises(ValueError): M.adapt(row,spec())
        row = row_for();row.pop('result');row.pop('returned_input_identity');row.pop('trace_aliases_source');row.pop('event_source_positions');row.update(result_kind='value',value_result=scalar(True))
        with self.assertRaises(ValueError): M.adapt(row,spec())

    def test_full_qualified_error_args_are_required(self):
        for field in ('args','type','message'):
            row = row_for(error=True);row['error'].pop(field)
            with self.assertRaises(ValueError): M.adapt(row,spec())

    def test_mixed_or_unknown_terminals_never_downgrade(self):
        for field in ('error','result_value','returned','exception','bogus_return'):
            row = row_for();row[field] = {}
            with self.assertRaises(ValueError): M.adapt(row,spec())

    def test_sparse_aliases_for_copied_events_are_valid(self):
        self.assertEqual(M.adapt(row_for(),spec())[-1],'actual_outcome')

    def test_aliased_trace_requires_compatible_event_positions(self):
        row = row_for();row['trace_aliases_source'] = [True];row['event_source_positions'] = [[[0,0]]]
        self.assertEqual(M.adapt(row,spec())[-1],'actual_outcome')
        row['event_source_positions'] = [[]]
        with self.assertRaises(ValueError): M.adapt(row,spec())

    def test_multiple_identity_matches_can_exceed_event_count(self):
        row = row_for()
        for field in ('input_before','input_after'):
            row[field]['traces'][0]['events'].append(copy.deepcopy(row[field]['traces'][0]['events'][0]))
        row['receipt']['args']['values'][0]['snapshot'] = copy.deepcopy(row['input_before'])
        row['controls_before']['args'] = copy.deepcopy(row['receipt']['args'])
        row['controls_after']['args'] = copy.deepcopy(row['receipt']['args'])
        row['event_source_positions'] = [[[0,0],[0,1]]];rebind(row)
        self.assertEqual(M.adapt(row,spec())[-1],'actual_outcome')

    def test_alias_bool_indices_bounds_and_value_conflicts(self):
        for pair in ([True,0],[0,-1],[1,0]):
            row = row_for();row['event_source_positions'] = [[pair]]
            with self.assertRaises(ValueError): M.adapt(row,spec())
        row = row_for();row['event_source_positions'] = [[[0,0]]];row['result']['traces'][0]['events'][0] = dictionary()
        with self.assertRaises(ValueError): M.adapt(row,spec())

    def test_returned_input_identity_requires_full_aliases(self):
        row = row_for();row['returned_input_identity'] = True
        with self.assertRaises(ValueError): M.adapt(row,spec())

    def test_all18_producer_tags_and_container_kinds(self):
        values = [scalar(None),scalar('Ω\0'),scalar(True),scalar(2**80),scalar(float('nan')),
            {'type':'bytes','hex':'00ff'},{'type':'nat'},{'type':'native_na'},
            {'type':'uuid','value':'00000000-0000-0000-0000-000000000001'},
            {'type':'datetime','iso':'2026-01-01T00:00:00+02:00','fold':1,'offset_seconds':7200},
            {'type':'enum','module':MODULE,'name':'TIMESTAMP_KEY','value':scalar('pm4py:param:timestamp_key')},
            sequence(scalar(1)),sequence(scalar(1),kind='tuple'),sequence(scalar(1),kind='set'),sequence(scalar(1),kind='frozenset'),
            dictionary((scalar(3),scalar('ordered nonstring key'))),{'type':'log','snapshot':snapshot()},
            {'type':'trace','snapshot':snapshot()['traces'][0],'source_trace_index':0}]
        for value in values:
            M._log_timestamp_packed(value,snapshot())

    def test_unknown_tag_enum_or_field_is_closed(self):
        for value in ({'type':'string','value':'x'},{'type':'enum','module':'wrong','name':'TIMESTAMP_KEY','value':scalar('pm4py:param:timestamp_key')},
                      {'type':'nat','value':None},{'type':'int','value':True}):
            with self.assertRaises(ValueError): M._log_timestamp_packed(value,snapshot())

    def test_datetime_and_set_order_validation(self):
        with self.assertRaises(ValueError): M._log_timestamp_packed({'type':'datetime','iso':'2026-01-01T00:00:00+02:00','fold':0,'offset_seconds':0})
        with self.assertRaises(ValueError): M._log_timestamp_packed(sequence(scalar(2),scalar(1),kind='set'))

    def test_parameter_controls_preserved_including_invalid_runtime_values(self):
        row = row_for(error=True);row['receipt']['kwargs'] = dictionary((scalar('parameters'),scalar(7)))
        row['controls_before']['kwargs'] = copy.deepcopy(row['receipt']['kwargs']);row['controls_after']['kwargs'] = copy.deepcopy(row['receipt']['kwargs']);rebind(row)
        self.assertEqual(M.adapt(row,spec())[-1],'actual_outcome')
        row['receipt']['kwargs'] = dictionary((scalar('unreviewed'),scalar(7)));rebind(row)
        with self.assertRaises(ValueError): M.adapt(row,spec())

    def test_full_warning_metadata_and_missing_warning(self):
        row = row_for(error=True);row['warnings'] = [{'category':'builtins.UserWarning','message':'full\0Ω'}]
        self.assertEqual(M.adapt(row,spec())[-1],'actual_outcome')
        row['warnings'][0]['extra'] = True
        with self.assertRaises(ValueError): M.adapt(row,spec())

    def test_digest_plan_remains_module_free_and_rejects_outcome(self):
        row = row_for();plan = {'case':row['case']['name'],'receipt_sha256':row['receipt_sha256']}
        self.assertEqual(M.adapt(plan,spec('plans','digest-plan-v1')),(None,None,None,row['receipt_sha256'],'digest_only_plan'))
        plan['error'] = row_for(error=True)['error']
        with self.assertRaises(ValueError): M.adapt(plan,spec('plans','digest-plan-v1'))

    def test_exact_light_intents_no_fabricated_operands(self):
        row = row_for('is_contained');entry = intent(row)
        self.assertEqual(M.adapt(entry,spec('intents')),(MODULE,'is_contained',None,row['receipt_sha256'],'entered_without_outcome'))
        entry['module'] = 'pm4py.other'
        with self.assertRaises(ValueError): M.adapt(entry,spec('intents'))

    def test_partial_entry_and_unknown_fields_are_conservative(self):
        row = row_for(error=True);row.pop('error')
        self.assertEqual(M.adapt(row,spec())[-1],'entered_without_outcome')
        row['unknown'] = True
        with self.assertRaises(ValueError): M.adapt(row,spec())

    def test_canonical_hash_and_outer_identity_conflicts_block(self):
        row = row_for();row['receipt_sha256'] = '0'*64
        with self.assertRaises(ValueError): M.adapt(row,spec())
        row = row_for();row['module'] = 'pm4py.other'
        with self.assertRaises(ValueError): M.adapt(row,spec())

    def test_digest_wrapper_and_exact_raw_slices(self):
        row = row_for();plan = {'case':row['case']['name'],'receipt_sha256':row['receipt_sha256']}
        audit = {'planned':1,'prior_paths':[],'prior_receipt_count':0,'duplicates':[],'prior_overlaps':[],'receipts':[plan]}
        source = {**spec('plans','digest-plan-v1'),'records_key':'receipts'}
        raw = json.dumps(audit,indent=2).encode();M._log_timestamp_audit(raw,source)
        value,element = list(M.records(raw,'json','receipts'))[0]
        self.assertEqual(value,plan);self.assertIn(element,raw)
        audit['receipts'][0]['receipt'] = row['receipt']
        with self.assertRaises(ValueError): M._log_timestamp_audit(json.dumps(audit).encode(),source)

    def test_index_partition_resolution_cache_and_no_absence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory);row = row_for();entry = intent(row)
            sources = []
            for name,role,value in [('records.jsonl','observations',row),('intents.jsonl','intents',entry)]:
                raw = json.dumps(value,ensure_ascii=True).encode()+b'\n';(root/name).write_bytes(raw)
                sources.append({**spec(role),'path':name,'format':'jsonl','bytes':len(raw),'sha256':M.sha(raw)})
            manifest = {'schema':M.MANIFEST,'root':str(root),'inventory_complete':True,'scope':'OWN synthetic bounded two-source test','modules':[MODULE],'sources':sources}
            index = M.Index(root/'index.sqlite')
            try:
                first = index.run(manifest);state = first['per_module'][MODULE]
                self.assertEqual((state['distinct_actual_receipts'],state['intents_with_terminal_outcome'],state['blocking_gaps']),(1,1,0))
                self.assertFalse(state['no_prior_calls_within_declared_inventory']);self.assertFalse(first['catalogue_audit_complete'])
                second = index.run(manifest);self.assertEqual(second['cache_hits'],2)
                (root/'records.jsonl').write_bytes(b'corrupted\n')
                third = index.run(manifest);self.assertGreater(third['per_module'][MODULE]['blocking_gaps'],0)
            finally: index.db.close()


if __name__ == '__main__': unittest.main()
