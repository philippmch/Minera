import importlib.util,json
from pathlib import Path
import unittest
from unittest.mock import patch
from contextlib import redirect_stdout
import io
import sys
import tempfile

R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'scripts'));spec=importlib.util.spec_from_file_location('llm_study',R/'scripts/llm_study.py');study=importlib.util.module_from_spec(spec);spec.loader.exec_module(study)
cases=[{'text':'Open -> Review α','indices':[0,1]},{'text':'Close','indices':[2]},{'text':'Open -> Close','indices':[3,4]}]
class LlmStudyTests(unittest.TestCase):
    def test_public_cluster_format_and_duplicate_names(self):
        response='```json\n[{"name":"α","regex":"Review"},{"name":"α","regex":"Close"}]\n```'
        self.assertEqual(study.cluster_definitions(response),[{'name':'α','regex':'Review'},{'name':'α','regex':'Close'}])
        self.assertEqual(study.cluster_definitions('```json\n[]\n```'),[])
        for text in ('[]','```json\n{}\n```','```json\n[{"name":"α"}]\n```'):
            with self.assertRaises(ValueError):study.cluster_definitions(text)
    def test_reference_regex_search_sequences_and_overlap(self):
        for pattern,expected in (('.*Review α.*',[0,1]),('Open',[0,1,3,4]),('Close',[2,3,4]),('Open,Review α',[]),('Open -> Review α',[0,1]),('^Open',[0,1,3,4]),('(?i).*review.*',[0,1]),('(?<=Open -> )Review α',[0,1]),('^$',[]),('',[0,1,2,3,4])):
            with self.subTest(pattern=pattern):self.assertEqual(study.select_cluster({'regex':pattern,'cases':cases}),{'indices':expected})
    def test_actual_isolated_regex_worker_and_invalid_pattern(self):
        self.assertEqual(study.select_cluster_bounded({'regex':'(?i)review','cases':cases}),{'indices':[0,1]})
        with self.assertRaises(ValueError):study.select_cluster_bounded({'regex':'[','cases':cases})
    def test_worker_input_validation(self):
        for payload in ({'regex':'.*','cases':[{'text':'Open','indices':[-1]}]},{'regex':'.*','cases':[{'text':None,'indices':[]}]},{'regex':'.*','cases':cases,'extra':True}):
            with self.assertRaises(ValueError):study.select_cluster(payload)
    def test_description_query_pairs_and_restored_columns(self):
        response='<description>Total α</description>\n```sql\nSELECT COUNT(*) FROM dataframe\n```\n<description>Cases</description>\n```sql\nSELECT "caseordabcchrordabc58ordabcchrconceptordabcchrordabc58ordabcchrname" FROM dataframe\n```'
        self.assertEqual(study.hypothesis_definitions(response),[{'description':'Total α','sql':'SELECT COUNT(*) FROM dataframe'},{'description':'Cases','sql':'SELECT "case:concept:name" FROM dataframe'}])
        for text in ('<description>No SQL</description>','```sql\nSELECT 1\n```','<description>Empty</description>```sql\n\n```'):
            with self.assertRaises(ValueError):study.hypothesis_definitions(text)
    def test_bounded_prompts_use_real_variants_and_schema(self):
        prompt=study.study_prompt('clusters',[],[('Open -> Review α',2)],'sqlite');self.assertIn('frequency = 2',prompt);self.assertIn("'regex'",prompt)
        prompt=study.study_prompt('hypotheses',['case:concept:name','cost'],[],'DuckDB','Events: 5');self.assertIn('<description>',prompt);self.assertIn('"cost"',prompt)
        self.assertIn("'regex'",study.study_prompt('clusters',[],[('Open',1)],'sqlite',max_len=40))
    def test_completion_only_flags_pass_exactly_and_never_enter_native_arguments(self):
        source={'columns':['case:concept:name','concept:name','__minera_event_index'],
                'rows':[['c1','A',0]],'output_truncated':False,
                'event_index_column':'__minera_event_index'}
        completion={'family':'LLM_COMPLETION','text':'```json\n[]\n```',
                    'response':{'provider_owned':'preserved'}}
        for credential in ('synthetic-study-key',''):
            with self.subTest(empty=not credential):
                calls=[]
                def reply(request,timeout):
                    calls.append((request,timeout))
                    class Response(io.StringIO):
                        def __enter__(self):return self
                        def __exit__(self,*args):self.close()
                    return Response(json.dumps(completion))
                output=io.StringIO()
                with patch.object(study,'native_json',return_value=source) as native, \
                     patch.object(study.urllib.request,'urlopen',side_effect=reply),redirect_stdout(output):
                    self.assertEqual(study.main(['original.xes','--task','clusters','--model','fixture',
                        '--execute-query','false','--api-key',credential,
                        '--api-url','http://127.0.0.1:9001/v1','--openai-token-field','max_tokens']),0)
                request=json.loads(calls[0][0].data)
                self.assertEqual({key:request[key] for key in ('api_key','api_url','openai_token_field')},
                    {'api_key':credential,'api_url':'http://127.0.0.1:9001/v1','openai_token_field':'max_tokens'})
                self.assertEqual(calls[0][1],90)
                native.assert_called_once_with('minera_query',[
                    'original.xes','--sql','SELECT * FROM dataframe','--limit','1000000'])
                report=json.loads(output.getvalue())
                self.assertEqual(report['completion'],completion)
                self.assertNotIn('request',report)
                self.assertNotIn('http://127.0.0.1:9001/v1',output.getvalue())
                if credential:self.assertNotIn(credential,output.getvalue())

    def test_prompt_only_and_response_file_do_not_make_provider_calls(self):
        source={'columns':['case:concept:name','concept:name','__minera_event_index'],
                'rows':[['c1','A',0]],'output_truncated':False,
                'event_index_column':'__minera_event_index'}
        with tempfile.TemporaryDirectory() as directory:
            response=Path(directory)/'response.txt';response.write_text('```json\n[]\n```',encoding='utf-8')
            for arguments in (['--obtain-query','false'],['--response-file',str(response),'--execute-query','false']):
                output=io.StringIO()
                with patch.object(study,'native_json',return_value=source), \
                     patch.object(study.urllib.request,'urlopen') as provider,redirect_stdout(output):
                    self.assertEqual(study.main(['original.xes','--task','clusters',
                        '--api-key','synthetic-unused-key','--api-url','http://127.0.0.1:9002/v1',
                        '--openai-token-field','max_tokens',*arguments]),0)
                provider.assert_not_called()
                self.assertNotIn('synthetic-unused-key',output.getvalue())
                self.assertNotIn('http://127.0.0.1:9002/v1',output.getvalue())

if __name__=='__main__':unittest.main()
