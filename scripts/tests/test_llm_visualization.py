import base64
import importlib.util
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
from contextlib import redirect_stdout
import io
import json
import tempfile
import types

R=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(R/"scripts"))
spec=importlib.util.spec_from_file_location("minera_llm_visualization",R/"scripts/llm_visualization.py")
vis=importlib.util.module_from_spec(spec);spec.loader.exec_module(vis)
PNG=base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR4nGP4z8AAAAMBAQDJ/pLvAAAAAElFTkSuQmCC")

class VisualizationTests(unittest.TestCase):
    def test_saver_then_connector_preserve_inputs_options_and_return_value(self):
        calls=[];paths=[]
        def saver(model,path,**kwargs):
            calls.append(("save",model,kwargs));paths.append(path);Path(path).write_bytes(PNG)
        def connector(prompt,**kwargs):
            calls.append(("connect",prompt,kwargs));self.assertEqual(Path(kwargs["image_path"]).read_bytes(),PNG);return {"owned":"explanation α"}
        result=vis.explain_visualization(saver,{"model":"native"},connector=connector,description="Diagram α.",annotated=True)
        self.assertEqual(result,{"owned":"explanation α"});self.assertEqual(calls[0],("save",{"model":"native"},{"annotated":True}))
        self.assertEqual(calls[1][1],"Could you explain the included process mining visualization?\n\nDiagram α.")
        self.assertTrue(calls[1][2]["annotated"]);self.assertFalse(Path(paths[0]).exists())

    def test_prepared_image_keeps_real_bytes_after_temporary_cleanup(self):
        paths=[]
        def saver(path):paths.append(path);Path(path).write_bytes(PNG)
        draft=vis.prepare_visualization(saver)
        self.assertEqual(base64.b64decode(draft["images"][0]["data"]),PNG);self.assertEqual(draft["images"][0]["media_type"],"image/png")
        self.assertFalse(Path(paths[0]).exists())

    def test_failed_saver_does_not_call_connector_and_cleans_owned_temporary_file(self):
        paths=[];connects=[]
        def saver(path):paths.append(path);Path(path).write_bytes(PNG);raise ValueError("Owned saver error")
        with self.assertRaisesRegex(ValueError,"Owned saver error"):vis.explain_visualization(saver,connector=lambda *a,**k:connects.append(k))
        self.assertEqual(connects,[]);self.assertFalse(Path(paths[0]).exists())

    def test_missing_empty_and_nonimage_saver_outputs_are_visible_errors(self):
        for payload in (None,b"",b"not an image"):
            def saver(path):
                if payload is not None:Path(path).write_bytes(payload)
            with self.assertRaises((ValueError,OSError)):vis.prepare_visualization(saver)

    def test_cli_overrides_are_completion_only_and_report_request_is_sanitized(self):
        completion={'family':'LLM_COMPLETION','text':'provider answer',
                    'response':{'api_key':'provider-owned-independent-value','other':['full',1]}}
        for direct in (False,True):
            with self.subTest(direct=direct),tempfile.TemporaryDirectory() as directory:
                root=Path(directory);arguments=root/'args.json';options=root/'options.json'
                arguments.write_text('[{"native":"model"}]',encoding='utf-8')
                options.write_text('{"annotated":true}',encoding='utf-8')
                module=types.ModuleType('owned_visualization_saver');saves=[]
                def saver(model,path,**kwargs):
                    saves.append((model,kwargs));Path(path).write_bytes(PNG)
                module.save=saver
                output=io.StringIO();calls=[]
                def complete(request):calls.append(request);return completion
                def runtime(url,request):
                    self.assertEqual(url,'http://127.0.0.1:8189');return complete(request)
                argv=['--saver','owned_visualization_saver:save','--args-file',str(arguments),
                      '--options-file',str(options),'--model','fixture','--api-key','synthetic-visual-key',
                      '--api-url','http://127.0.0.1:9003/v1','--openai-token-field','max_tokens']
                if not direct:argv+=['--runtime-url','http://127.0.0.1:8189']
                with patch.dict(sys.modules,{'owned_visualization_saver':module}), \
                     patch.object(vis,'runtime_completion',side_effect=runtime) as bridge, \
                     patch.object(vis,'provider_configs',return_value={'fixture':'startup'}) as configs, \
                     patch.object(vis,'ProviderRuntime') as provider,redirect_stdout(output):
                    provider.return_value.complete.side_effect=complete
                    self.assertEqual(vis.main(argv),0)
                    if direct:
                        bridge.assert_not_called();configs.assert_called_once()
                        provider.assert_called_once_with({'fixture':'startup'})
                    else:provider.assert_not_called();configs.assert_not_called()
                self.assertEqual(saves,[({'native':'model'},{'annotated':True})])
                self.assertEqual(len(calls),1)
                request=calls[0]
                self.assertEqual(request['api_key'],'synthetic-visual-key')
                self.assertEqual(request['api_url'],'http://127.0.0.1:9003/v1')
                self.assertEqual(request['openai_token_field'],'max_tokens')
                self.assertEqual(base64.b64decode(request['images'][0]['data']),PNG)
                report=json.loads(output.getvalue())
                self.assertEqual(report['completion'],completion)
                for name in ('api_key','api_url','openai_token_field'):self.assertNotIn(name,report['request'])
                self.assertNotIn('synthetic-visual-key',output.getvalue())
                self.assertNotIn('http://127.0.0.1:9003/v1',output.getvalue())

    def test_prompt_only_output_omits_call_configuration_without_calling_provider(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);arguments=root/'args.json';output=root/'report.json'
            arguments.write_text('[]',encoding='utf-8')
            module=types.ModuleType('owned_visualization_saver')
            def saver(path):Path(path).write_bytes(PNG)
            module.save=saver
            with patch.dict(sys.modules,{'owned_visualization_saver':module}), \
                 patch.object(vis,'runtime_completion') as bridge,patch.object(vis,'ProviderRuntime') as provider:
                self.assertEqual(vis.main(['--saver','owned_visualization_saver:save',
                    '--args-file',str(arguments),'--prompt-only','--output',str(output),
                    '--api-key','','--api-url','http://127.0.0.1:9004/v1',
                    '--openai-token-field','max_tokens']),0)
                bridge.assert_not_called();provider.assert_not_called()
            report=json.loads(output.read_text(encoding='utf-8'))
            self.assertNotIn('completion',report)
            for name in ('api_key','api_url','openai_token_field'):self.assertNotIn(name,report['request'])
            self.assertNotIn('http://127.0.0.1:9004/v1',output.read_text(encoding='utf-8'))

if __name__=="__main__":unittest.main()
