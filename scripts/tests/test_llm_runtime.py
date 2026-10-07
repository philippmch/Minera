import importlib.util
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import sys
from threading import Thread
import unittest
import urllib.error
import urllib.request

R = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('minera_llm_runtime', R/'scripts/llm_runtime.py')
llm = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = llm
spec.loader.exec_module(llm)

RESPONSES = {
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
                provider = 'google' if ':generateContent' in self.path else 'anthropic' if self.path.endswith('/messages') else 'openai'
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
        for extra in ({'provider':'bad'},{'model':''},{'prompt':' '},{'api_key':'not-accepted'},{'temperature':float('nan')},{'temperature':2},{'max_tokens':True},{'max_tokens':0}):
            with self.assertRaises((ValueError,UnicodeError)):self.runtime.complete(self.payload(**extra))
        self.assertEqual(self.calls,[])

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


if __name__=='__main__':unittest.main()
