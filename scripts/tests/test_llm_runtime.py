import importlib.util
import base64
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import sys
from threading import Thread
import unittest
from unittest.mock import patch
from concurrent.futures import ThreadPoolExecutor
from contextlib import redirect_stdout, redirect_stderr
import argparse
import io
import tempfile
import urllib.error
import urllib.request

R = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('minera_llm_runtime', R/'scripts/llm_runtime.py')
llm = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = llm
spec.loader.exec_module(llm)

RESPONSES = {
 'responses': {'output':[{'type':'message','content':[{'type':'output_text','text':'Summary α\nDone'}]}],'status':'completed','usage':{'input_tokens':9,'output_tokens':3}},
 'openai': {'choices':[{'message':{'content':'Summary α\nDone'},'finish_reason':'stop'}],'usage':{'prompt_tokens':9,'completion_tokens':3}},
 'anthropic': {'content':[{'type':'thinking','thinking':'hidden'},{'type':'text','text':'Summary α'},{'type':'text','text':'\nDone'}],'stop_reason':'end_turn','usage':{'input_tokens':2,'cache_creation_input_tokens':3,'cache_read_input_tokens':4,'output_tokens':3}},
 'google': {'candidates':[{'content':{'parts':[{'text':'hidden','thought':True},{'text':'Summary α'},{'text':'\nDone'}]},'finishReason':'STOP'}],'usageMetadata':{'promptTokenCount':9,'candidatesTokenCount':3}},
}


class RuntimeTests(unittest.TestCase):
    def setUp(self):
        self.calls = []
        self.status = 200
        self.response_override = None
        case = self
        class ProviderHandler(BaseHTTPRequestHandler):
            def log_message(self, *args): pass
            def do_POST(self):
                raw=self.rfile.read(int(self.headers['Content-Length']))
                case.calls.append({'path':self.path,'headers':dict(self.headers),'body':json.loads(raw)})
                provider = 'responses' if self.path.endswith('/responses') else 'google' if ':generateContent' in self.path else 'anthropic' if self.path.endswith('/messages') else 'openai'
                body = case.response_override if case.response_override is not None else json.dumps(RESPONSES[provider],ensure_ascii=False).encode('utf-8')
                self.send_response(case.status)
                if case.status == 302: self.send_header('Location',f'http://127.0.0.1:{self.server.server_port}/redirected')
                self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
        self.provider = ThreadingHTTPServer(('127.0.0.1',0),ProviderHandler)
        self.thread = Thread(target=self.provider.serve_forever,daemon=True);self.thread.start()
        base=f'http://127.0.0.1:{self.provider.server_port}/v1'
        self.configs={name:llm.ProviderConfig(base,'fixture-secret') for name in llm.PROVIDERS}
        self.runtime=llm.ProviderRuntime(self.configs)

    def tearDown(self):
        self.provider.shutdown();self.provider.server_close();self.thread.join()

    def payload(self,provider='openai',**extra):
        return {'provider':provider,'model':'model-fixture','prompt':'Process → review\n東京','max_tokens':256,**extra}

    def test_all_three_providers_make_actual_http_requests_and_retain_results(self):
        for provider in llm.PROVIDERS:
            with self.subTest(provider=provider):
                result=self.runtime.complete(self.payload(provider,temperature=0.25))
                self.assertEqual(result['text'],'Summary α\nDone')
                self.assertEqual(result['usage'],{'input_tokens':9,'output_tokens':3})
                self.assertEqual(result['response'],RESPONSES[provider])
        calls=self.calls
        self.assertEqual(calls[0]['path'],'/v1/chat/completions')
        self.assertEqual(calls[0]['headers']['Authorization'],'Bearer fixture-secret')
        self.assertEqual(calls[0]['body']['max_completion_tokens'],256)
        self.assertEqual(calls[0]['body']['messages'][0]['content'],'Process → review\n東京')
        self.assertEqual(calls[1]['headers']['X-Api-Key'],'fixture-secret')
        self.assertEqual(calls[1]['headers']['Anthropic-Version'],'2023-06-01')
        self.assertEqual(calls[1]['body']['max_tokens'],256)
        self.assertEqual(calls[2]['path'],'/v1/models/model-fixture:generateContent')
        self.assertEqual(calls[2]['headers']['X-Goog-Api-Key'],'fixture-secret')
        self.assertNotIn('fixture-secret',calls[2]['path'])
        self.assertEqual(calls[2]['body']['generationConfig'],{'maxOutputTokens':256,'temperature':0.25})

    def test_model_defaults_omit_temperature_and_legacy_compatible_limits_are_configurable(self):
        self.runtime.complete(self.payload())
        self.assertNotIn('temperature',self.calls[-1]['body'])
        self.configs['openai']=llm.ProviderConfig(self.configs['openai'].base_url,'fixture-secret','max_tokens')
        llm.ProviderRuntime(self.configs).complete(self.payload())
        self.assertEqual(self.calls[-1]['body']['max_tokens'],256)
        self.assertNotIn('max_completion_tokens',self.calls[-1]['body'])

    def test_refusals_empty_text_and_unknown_usage_remain_explicit(self):
        self.assertEqual(llm.parse_completion('openai',{'choices':[{'message':{'content':None,'refusal':'Declined'},'finish_reason':'stop'}]})[:2],('Declined','refusal'))
        text,finish,usage=llm.parse_completion('anthropic',{'content':[{'type':'text','text':''}],'stop_reason':'max_tokens'})
        self.assertEqual((text,finish),('','max_tokens'));self.assertIsNone(usage['input_tokens'])
        with self.assertRaises(llm.ProviderError):llm.parse_completion('google',{'promptFeedback':{'blockReason':'SAFETY'}})

    def test_bad_requests_never_reach_a_provider(self):
        for extra in ({'provider':'bad'},{'model':''},{'prompt':' '},{'api_key':True},{'temperature':float('nan')},{'temperature':2.1},{'max_tokens':True},{'max_tokens':0}):
            with self.assertRaises((ValueError,UnicodeError)):self.runtime.complete(self.payload(**extra))
        self.assertEqual(self.calls,[])

    def test_images_reach_all_four_provider_protocols_without_byte_changes(self):
        raw=b'\x89PNG\r\n\x1a\nowned-image-fixture'
        image={'name':'diagram α.png','media_type':'image/png','data':base64.b64encode(raw).decode()}
        variants=[('openai','responses'),('openai','chat_completions'),('anthropic',None),('google',None)]
        for provider,api in variants:
            result=self.runtime.complete(self.payload(provider,images=[image],**({'openai_api':api} if api else {})))
            self.assertEqual(result['text'],'Summary α\nDone')
            body=self.calls[-1]['body']
            if api=='responses':
                self.assertTrue(self.calls[-1]['path'].endswith('/responses'))
                data=body['input'][0]['content'][1]['image_url'].split(',',1)[1]
                self.assertEqual(body['max_output_tokens'],256)
            elif api=='chat_completions':data=body['messages'][0]['content'][1]['image_url']['url'].split(',',1)[1]
            elif provider=='anthropic':data=body['messages'][0]['content'][1]['source']['data']
            else:data=body['contents'][0]['parts'][1]['inlineData']['data']
            self.assertEqual(base64.b64decode(data),raw)

    def test_responses_text_refusal_incomplete_and_usage(self):
        response={'output':[{'type':'reasoning','summary':[]},{'type':'message','content':[{'type':'output_text','text':'Answer α'},{'type':'output_text','text':'\nDone'}]}],'status':'incomplete','incomplete_details':{'reason':'max_output_tokens'},'usage':{'input_tokens':12,'output_tokens':4}}
        self.assertEqual(llm.parse_completion('openai',response),('Answer α\nDone','max_output_tokens',{'input_tokens':12,'output_tokens':4}))
        response['output']=[{'type':'message','content':[{'type':'refusal','refusal':'Declined'}]}]
        self.assertEqual(llm.parse_completion('openai',response)[:2],('Declined','refusal'))
        response['output']=[]
        with self.assertRaises(llm.ProviderError):llm.parse_completion('openai',response)

    def test_extra_payload_overrides_fields_and_accepts_nested_provider_controls(self):
        for provider in llm.PROVIDERS:
            extra={'temperature':0.7,'model':'override-model','metadata':{'source':'owned α'}}
            self.runtime.complete(self.payload(provider,extra_payload=extra))
            self.assertEqual({k:self.calls[-1]['body'][k] for k in extra},extra)
        self.runtime.complete(self.payload('google',extra_payload='{"generationConfig":{"topP":0.8,"maxOutputTokens":123}}'))
        self.assertEqual(self.calls[-1]['body']['generationConfig'],{'topP':0.8,'maxOutputTokens':123})

    def test_invalid_images_payloads_and_provider_ranges_never_send(self):
        for extra in ({'images':[{'media_type':'image/png','data':'not-base64'}]},{'images':[{'media_type':'image/jpeg','data':base64.b64encode(b'\x89PNG\r\n\x1a\n').decode()}]},{'images':[{}]*9},{'extra_payload':[]},{'extra_payload':'[]'},{'extra_payload':{'stream':True}},{'provider':'anthropic','temperature':1.5},{'openai_api':'bad'}):
            with self.assertRaises(ValueError):self.runtime.complete(self.payload(**extra))
        self.assertEqual(self.calls,[])
        self.runtime.complete(self.payload(temperature=2))
        self.runtime.complete(self.payload('google',temperature=2))

    def test_error_bodies_and_redirects_do_not_leak_credentials_or_retry(self):
        self.response_override=b'fixture-secret credentials and upstream internal data'
        for status in (401,429,500,302):
            self.status=status
            with self.assertRaises(llm.ProviderError) as raised:self.runtime.complete(self.payload())
            self.assertNotIn('fixture-secret',str(raised.exception))
            self.assertIn(str(status),str(raised.exception))
        self.assertEqual(len(self.calls),4)

    def test_malformed_and_missing_text_responses_are_errors(self):
        for raw in (b'not json',b'{}',b'{"choices":[]}',b'{"choices":[{"message":{"content":3}}]}'):
            self.response_override=raw
            with self.assertRaises(llm.ProviderError):self.runtime.complete(self.payload())
        with self.assertRaises(llm.ProviderError):llm.parse_completion('openai',{'choices':[{'message':{'content':'x'},'finish_reason':'stop'}],'usage':{'prompt_tokens':True}})

    def test_provider_config_has_no_embedded_credentials_and_missing_keys_are_honest(self):
        for url in ('http://example.com/v1','https://key:secret@example.com/v1','https://example.com/v1?key=secret','https://example.com/#fragment'):
            with self.assertRaises(ValueError):llm.ProviderConfig(url)
        runtime=llm.ProviderRuntime(llm.provider_configs({}))
        self.assertTrue(all(not item['configured'] for item in runtime.status()['providers']))
        self.assertNotIn('fixture-secret',json.dumps(self.runtime.status()))
        with self.assertRaises(ValueError):runtime.complete(self.payload())
        self.assertEqual(self.calls,[])

    def test_runtime_http_health_completion_origin_and_body_guards(self):
        server=ThreadingHTTPServer(('127.0.0.1',0),llm.handler_for(self.runtime))
        thread=Thread(target=server.serve_forever,daemon=True);thread.start()
        url=f'http://127.0.0.1:{server.server_port}'
        try:
            with urllib.request.urlopen(url+'/health') as response:self.assertTrue(json.load(response)['ready'])
            request=urllib.request.Request(url+'/complete',data=json.dumps(self.payload()).encode(),headers={'Content-Type':'application/json'})
            with urllib.request.urlopen(request) as response:self.assertEqual(json.load(response)['text'],'Summary α\nDone')
            for headers,status in (({'Origin':'https://unrelated.example','Content-Type':'application/json'},403),({'Content-Type':'text/plain'},415)):
                request=urllib.request.Request(url+'/complete',data=b'{}',headers=headers)
                with self.assertRaises(urllib.error.HTTPError) as raised:urllib.request.urlopen(request)
                self.assertEqual(raised.exception.code,status)
                raised.exception.close()
        finally:server.shutdown();server.server_close();thread.join()

    def test_call_local_config_reaches_all_protocols_without_changing_defaults(self):
        original=dict(self.runtime.configs)
        status=self.runtime.status()
        base=self.configs['openai'].base_url+'/call-local'
        protocols=[('openai','responses'),('openai','chat_completions'),('anthropic',None),('google',None)]
        for provider,api in protocols:
            with self.subTest(provider=provider,api=api):
                extra={'api_key':'request-key-'+provider,'api_url':base}
                if api:extra.update(openai_api=api,openai_token_field='max_tokens')
                result=self.runtime.complete(self.payload(provider,**extra))
                call=self.calls[-1]
                self.assertTrue(call['path'].startswith('/v1/call-local/'))
                header={'openai':'Authorization','anthropic':'X-Api-Key','google':'X-Goog-Api-Key'}[provider]
                prefix='Bearer ' if provider=='openai' else ''
                self.assertEqual(call['headers'][header],prefix+'request-key-'+provider)
                self.assertEqual(set(result),{'family','provider','model','text','finish_reason','usage','response'})
                self.assertEqual(result['response'],RESPONSES[api if api=='responses' else provider])
                self.assertNotIn('request-key-'+provider,json.dumps(result))
                self.assertNotIn(base,json.dumps(result))
                if api=='chat_completions':
                    self.assertIn('max_tokens',call['body'])
                    self.assertNotIn('max_completion_tokens',call['body'])
        self.assertEqual(self.runtime.configs,original)
        self.assertEqual(self.runtime.status(),status)
        self.runtime.complete(self.payload())
        self.assertEqual(self.calls[-1]['path'],'/v1/chat/completions')
        self.assertEqual(self.calls[-1]['headers']['Authorization'],'Bearer fixture-secret')
        self.assertIn('max_completion_tokens',self.calls[-1]['body'])

    def test_missing_null_and_explicit_empty_are_distinct_per_call(self):
        for provider in llm.PROVIDERS:
            self.runtime.complete(self.payload(provider,api_key=None,api_url=None,openai_token_field=None))
            header={'openai':'Authorization','anthropic':'X-Api-Key','google':'X-Goog-Api-Key'}[provider]
            self.assertEqual(self.calls[-1]['headers'][header],('Bearer ' if provider=='openai' else '')+'fixture-secret')
        with patch.dict(llm.os.environ,{'OPENAI_API_KEY':'never-fallback-key'}):
            self.runtime.complete(self.payload(api_key=''))
            self.assertNotIn('Authorization',self.calls[-1]['headers'])
            self.assertEqual(llm.os.environ['OPENAI_API_KEY'],'never-fallback-key')
        remote=llm.ProviderRuntime({name:llm.ProviderConfig('https://synthetic.invalid/v1','startup-key') for name in llm.PROVIDERS})
        count=len(self.calls)
        with self.assertRaises(ValueError):remote.complete(self.payload(api_key=''))
        self.assertEqual(len(self.calls),count)
        self.assertEqual(remote.configs['openai'].api_key,'startup-key')

    def test_invalid_call_configs_never_reach_provider_or_echo_values(self):
        cases=[]
        for name in llm.PROVIDER_CALL_FIELDS:
            cases.extend({name:value} for value in (True,7,[],{}))
        cases.extend({'api_url':value} for value in ('','http://external.invalid/v1','https://user:secret-sentinel@synthetic.invalid/v1','https://synthetic.invalid/v1?key=secret-sentinel','https://synthetic.invalid/v1#secret-sentinel','https://synthetic.invalid/ bad','https://[secret-sentinel]/v1'))
        cases.extend([{'api_key':'secret-sentinel\r\nheader'}, {'openai_token_field':''}, {'openai_token_field':'secret-sentinel'}, {'provider':'anthropic','openai_token_field':'max_tokens'}, {'provider':'google','openai_token_field':'max_completion_tokens'}, {'openai_api':None}, {'provider':'anthropic','openai_api':None}])
        for extra in cases:
            with self.subTest(fields=list(extra)):
                with self.assertRaises(ValueError) as raised:self.runtime.complete(self.payload(**extra))
                self.assertNotIn('secret-sentinel',str(raised.exception))
        self.assertEqual(self.calls,[])

    def test_concurrent_configs_and_following_default_call_are_isolated(self):
        second_calls=[]
        class SecondProvider(BaseHTTPRequestHandler):
            def log_message(self,*args):pass
            def do_POST(self):
                body=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
                second_calls.append({'path':self.path,'headers':dict(self.headers),'body':body})
                raw=json.dumps(RESPONSES['openai']).encode()
                self.send_response(200);self.send_header('Content-Length',str(len(raw)));self.end_headers();self.wfile.write(raw)
        second=ThreadingHTTPServer(('127.0.0.1',0),SecondProvider)
        worker=Thread(target=second.serve_forever,daemon=True);worker.start()
        original=dict(self.runtime.configs)
        try:
            with patch.dict(llm.os.environ,{'OPENAI_API_KEY':'environment-unchanged'}):
                with ThreadPoolExecutor(max_workers=2) as executor:
                    futures=[executor.submit(self.runtime.complete,self.payload(api_key='call-A',api_url=self.configs['openai'].base_url+'/A')),
                             executor.submit(self.runtime.complete,self.payload(api_key='call-B',api_url=f'http://127.0.0.1:{second.server_port}/B',openai_token_field='max_tokens'))]
                    for future in futures:self.assertEqual(future.result()['text'],'Summary α\nDone')
                self.assertEqual(llm.os.environ['OPENAI_API_KEY'],'environment-unchanged')
            self.assertEqual(self.calls[0]['headers']['Authorization'],'Bearer call-A')
            self.assertEqual(self.calls[0]['path'],'/v1/A/chat/completions')
            self.assertEqual(second_calls[0]['headers']['Authorization'],'Bearer call-B')
            self.assertEqual(second_calls[0]['path'],'/B/chat/completions')
            self.assertIn('max_tokens',second_calls[0]['body'])
            self.assertNotIn('max_completion_tokens',second_calls[0]['body'])
            self.assertEqual(self.runtime.configs,original)
            self.runtime.complete(self.payload())
            self.assertEqual(self.calls[-1]['path'],'/v1/chat/completions')
            self.assertEqual(self.calls[-1]['headers']['Authorization'],'Bearer fixture-secret')
        finally:second.shutdown();second.server_close();worker.join()

    def test_shared_cli_helpers_preserve_empty_and_sanitize_only_own_request(self):
        parser=argparse.ArgumentParser(add_help=False);llm.add_provider_call_arguments(parser)
        self.assertEqual(llm.provider_call_overrides(parser.parse_args([])),{})
        args=parser.parse_args(['--api-key','','--api-url','http://127.0.0.1:1/v1','--openai-token-field','max_tokens'])
        overrides=llm.provider_call_overrides(args)
        self.assertEqual(overrides,{'api_key':'','api_url':'http://127.0.0.1:1/v1','openai_token_field':'max_tokens'})
        request={**self.payload(),**overrides,'extra_payload':{'metadata':{'source':'owned'}}}
        report=llm.provider_call_report_request(request)
        self.assertTrue(all(name not in report for name in llm.PROVIDER_CALL_FIELDS))
        self.assertTrue(all(name in request for name in llm.PROVIDER_CALL_FIELDS))
        self.assertEqual(report['extra_payload'],request['extra_payload'])

    def test_runtime_complete_cli_flags_override_json_without_entering_output(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'request.json'
            path.write_text(json.dumps(self.payload(api_key='json-key',api_url=None)),encoding='utf8')
            stdout,stderr=io.StringIO(),io.StringIO()
            with patch.object(llm,'provider_configs',return_value=self.configs),redirect_stdout(stdout),redirect_stderr(stderr):
                code=llm.main(['--complete',str(path),'--api-key','','--api-url',self.configs['openai'].base_url+'/cli','--openai-token-field','max_tokens'])
            self.assertEqual(code,0)
            self.assertNotIn('Authorization',self.calls[-1]['headers'])
            self.assertEqual(self.calls[-1]['path'],'/v1/cli/chat/completions')
            self.assertIn('max_tokens',self.calls[-1]['body'])
            result=json.loads(stdout.getvalue());self.assertEqual(result['response'],RESPONSES['openai'])
            self.assertNotIn('json-key',stdout.getvalue()+stderr.getvalue())
            self.assertNotIn('/v1/cli',stdout.getvalue()+stderr.getvalue())
        stderr=io.StringIO()
        with patch.object(llm,'ProviderRuntime') as constructor,redirect_stderr(stderr):
            self.assertEqual(llm.main(['--api-key','hosted-secret-sentinel']),1)
        constructor.assert_not_called()
        self.assertNotIn('hosted-secret-sentinel',stderr.getvalue())


if __name__=='__main__':unittest.main()
