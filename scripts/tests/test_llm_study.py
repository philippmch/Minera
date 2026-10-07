import importlib.util,json
from pathlib import Path
import unittest

R=Path(__file__).resolve().parents[2];spec=importlib.util.spec_from_file_location('llm_study',R/'scripts/llm_study.py');study=importlib.util.module_from_spec(spec);spec.loader.exec_module(study)
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
if __name__=='__main__':unittest.main()
