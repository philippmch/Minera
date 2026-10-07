import importlib.util
import http.client
import json
from pathlib import Path
import threading
import unittest

spec = importlib.util.spec_from_file_location("minera_embedding_runtime", Path(__file__).resolve().parents[1] / "embedding_runtime.py")
runtime_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime_module)


class Vector(list):
    def tolist(self):
        return list(self)


class Encoder:
    def __init__(self):
        self.calls = []

    def encode(self, text, **options):
        self.calls.append((text, options))
        return Vector([0.1, 0.2, 0.3][:options["truncate_dim"]])


class EmbeddingRuntimeTests(unittest.TestCase):
    def setUp(self):
        self.encoder = Encoder()
        self.runtime = runtime_module.EncoderRuntime(self.encoder, "sentence-transformers/test-model", cache_items=1)

    def request(self, text="hello", model="test-model", dimensions=0):
        return {"text": text, "model": model, "dimensions": dimensions}

    def test_real_model_contract_and_dimension_inference(self):
        self.assertEqual(self.runtime.dimensions, 3)
        self.assertEqual(self.runtime.embed(self.request()), [0.1,0.2,0.3])
        self.assertEqual(self.runtime.embed(self.request(dimensions=2)), [0.1,0.2])
        self.assertFalse(self.encoder.calls[-1][1]["normalize_embeddings"])

    def test_model_requests_never_fall_back_to_hashing(self):
        for model in ("hashing-v1", "mock_transformer", "not-loaded"):
            with self.assertRaisesRegex(ValueError, "not loaded"):
                self.runtime.embed(self.request(model=model))

    def test_rejects_bad_requests_and_nonfinite_model_outputs(self):
        for value in (True, -1, 4, 0.5):
            with self.assertRaises(ValueError):
                self.runtime.embed(self.request(dimensions=value))
        with self.assertRaises(ValueError):
            self.runtime.embed({**self.request(), "extra": 1})
        self.encoder.encode = lambda *_args, **_kwargs: Vector([float("nan")])
        with self.assertRaisesRegex(ValueError, "non-finite"):
            self.runtime.embed(self.request())

    def test_cache_is_bounded_and_returns_independent_values(self):
        a = self.runtime.embed(self.request());a[0] = 9
        self.assertEqual(self.runtime.embed(self.request())[0], 0.1)
        self.assertEqual(len(self.encoder.calls), 2)
        self.runtime.embed(self.request(text="other"))
        self.assertEqual(len(self.runtime.cache), 1)

    def test_http_health_embed_and_error_envelopes(self):
        server = runtime_module.create_server(self.runtime, "127.0.0.1", 0)
        worker = threading.Thread(target=server.serve_forever, daemon=True);worker.start()
        try:
            c = http.client.HTTPConnection("127.0.0.1", server.server_port, timeout=3)
            c.request("GET", "/health");r=c.getresponse();self.assertEqual(r.status,200);self.assertEqual(json.loads(r.read())["dimensions"],3)
            c.request("POST", "/embed", json.dumps(self.request()), {"Content-Type":"application/json"});r=c.getresponse();self.assertEqual(r.status,200);self.assertEqual(json.loads(r.read()),{"embedding":[0.1,0.2,0.3]})
            c.request("POST", "/embed", json.dumps(self.request(model="bad")), {"Content-Type":"application/json"});r=c.getresponse();self.assertEqual(r.status,400);self.assertIn("not loaded",json.loads(r.read())["error"])
            c.close()
        finally:
            server.shutdown();server.server_close();worker.join(timeout=3)


if __name__ == "__main__":
    unittest.main()
