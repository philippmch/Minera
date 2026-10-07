"""Run a local SentenceTransformer encoder for Minera's /embed bridge.

Install scripts/requirements-transformers.txt in a virtual environment, then run:
  python scripts/embedding_runtime.py --model sentence-transformers/all-MiniLM-L6-v2
  minera_server --embedding-runtime-url http://127.0.0.1:8188
Use transformer mode, that model name, and dimension 0 in Minera to infer width.
Models are loaded before the service becomes ready; document text stays local.
"""
from __future__ import annotations

import argparse
from collections import OrderedDict
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import math
from pathlib import Path
import socket
import sys
from threading import Lock
from typing import Any

MAX_BODY_BYTES = 1024 * 1024
MAX_DIMENSIONS = 4096
DEFAULT_MODEL = "sentence-transformers/all-MiniLM-L6-v2"


class EncoderRuntime:
    def __init__(self, model: Any, model_name: str, *, normalize: bool = False,
                 cache_items: int = 1024):
        self.model = model
        self.model_name = model_name
        self.aliases = {model_name, model_name.rsplit("/", 1)[-1]}
        self.normalize = normalize
        self.cache_items = cache_items
        self.lock = Lock()
        self.cache: OrderedDict[tuple[str, int], list[float]] = OrderedDict()
        self.dimensions = len(self._encode("Minera encoder warmup", 0))

    def _encode(self, text: str, dimensions: int) -> list[float]:
        result = self.model.encode(text, convert_to_numpy=True,
                                   show_progress_bar=False,
                                   normalize_embeddings=self.normalize,
                                   truncate_dim=dimensions or None)
        values = result.tolist()
        if not isinstance(values, list) or not values or len(values) > MAX_DIMENSIONS:
            raise ValueError("The model returned an empty or oversized vector.")
        if any(type(x) not in (int, float) or not math.isfinite(x) for x in values):
            raise ValueError("The model returned non-finite embedding values.")
        if dimensions and len(values) != dimensions:
            raise ValueError("The model cannot provide the requested dimension.")
        return [float(x) for x in values]

    def embed(self, payload: Any) -> list[float]:
        if not isinstance(payload, dict) or set(payload) != {"text", "model", "dimensions"}:
            raise ValueError("Expected exactly text, model and dimensions fields.")
        text, name, dimensions = (payload[k] for k in ("text", "model", "dimensions"))
        if not isinstance(text, str):
            raise ValueError("text must be a string.")
        try:
            size = len(text.encode("utf-8"))
        except UnicodeError as error:
            raise ValueError("text must contain valid Unicode.") from error
        if size > MAX_BODY_BYTES:
            raise ValueError("text is too large.")
        if not isinstance(name, str) or name not in self.aliases:
            raise ValueError("The requested model is not loaded in this runtime.")
        if type(dimensions) is not int or not 0 <= dimensions <= self.dimensions:
            raise ValueError(f"dimensions must be 0 through {self.dimensions}.")
        key = (text, dimensions)
        with self.lock:
            if key in self.cache:
                self.cache.move_to_end(key)
                return self.cache[key].copy()
            values = self._encode(text, dimensions)
            self.cache[key] = values
            while len(self.cache) > self.cache_items:
                self.cache.popitem(last=False)
            return values.copy()

    def health(self) -> dict[str, Any]:
        return {"ready": True, "model": self.model_name,
                "dimensions": self.dimensions, "normalize": self.normalize}


def create_server(runtime: EncoderRuntime, host: str, port: int) -> ThreadingHTTPServer:
    class Handler(BaseHTTPRequestHandler):
        server_version = "MineraEncoder/1.0"

        def setup(self):
            super().setup()
            self.connection.settimeout(10)

        def log_message(self, *_args):
            # Do not log document text or request bodies.
            pass

        def send_json(self, status: int, payload: dict[str, Any]):
            body = json.dumps(payload, ensure_ascii=True, allow_nan=False).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            if self.path == "/health":
                self.send_json(200, runtime.health())
            else:
                self.send_json(404, {"error": "Route not found."})

        def do_POST(self):
            if self.path != "/embed":
                self.send_json(404, {"error": "Route not found."})
                return
            try:
                if self.headers.get("Content-Type", "").split(";", 1)[0].strip().lower() != "application/json":
                    self.send_json(415, {"error": "Use application/json."})
                    return
                length = int(self.headers.get("Content-Length", "0"))
                if not 0 < length <= MAX_BODY_BYTES:
                    self.send_json(413, {"error": "Request body is empty or too large."})
                    return
                payload = json.loads(self.rfile.read(length).decode("utf-8"))
                self.send_json(200, {"embedding": runtime.embed(payload)})
            except (ValueError, UnicodeError) as error:
                self.send_json(400, {"error": str(error)})
            except Exception:
                self.send_json(500, {"error": "Model inference failed."})

    class Server(ThreadingHTTPServer):
        daemon_threads = True
        address_family = socket.AF_INET6 if host == "::1" else socket.AF_INET

    return Server((host, port), Handler)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--model", default=DEFAULT_MODEL, help="Model ID or local model directory to preload")
    parser.add_argument("--revision", help="Optional pinned model revision")
    parser.add_argument("--host", choices=("127.0.0.1", "localhost", "::1"), default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8188)
    parser.add_argument("--device", default="cpu")
    parser.add_argument("--threads", type=int, default=2)
    parser.add_argument("--cache-folder", type=Path)
    parser.add_argument("--local-files-only", action="store_true")
    parser.add_argument("--normalize", action="store_true")
    args = parser.parse_args(argv)
    if not 0 <= args.port <= 65535 or not 1 <= args.threads <= 64:
        parser.error("port must be 0 through 65535 and threads 1 through 64")
    try:
        import torch
        from sentence_transformers import SentenceTransformer
    except ImportError:
        print("Install scripts/requirements-transformers.txt in this Python environment.", file=sys.stderr)
        return 1
    torch.set_num_threads(args.threads)
    try:
        model = SentenceTransformer(args.model, device=args.device,
                                    cache_folder=str(args.cache_folder) if args.cache_folder else None,
                                    revision=args.revision, local_files_only=args.local_files_only,
                                    token=False, trust_remote_code=False,
                                    model_kwargs={"use_safetensors": True})
        runtime = EncoderRuntime(model, args.model, normalize=args.normalize)
        server = create_server(runtime, args.host, args.port)
    except Exception as error:
        print(f"Could not start the local encoder: {error}", file=sys.stderr)
        return 1
    host = f"[{args.host}]" if ":" in args.host else args.host
    print(json.dumps({**runtime.health(), "url": f"http://{host}:{server.server_port}"}), flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
