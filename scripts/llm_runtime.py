"""Minera's optional language-model runtime (Python standard library only).

Credentials and provider base URLs are read from the server environment.
Run with --port 8189, then minera_server --llm-runtime-url http://127.0.0.1:8189.
Use --complete request.json (or - for stdin) for standalone execution.
"""
from __future__ import annotations

import argparse
import base64
import binascii
from dataclasses import dataclass
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import math
import os
from pathlib import Path
import sys
from typing import Any, Mapping
import urllib.error
import urllib.parse
import urllib.request

MAX_INPUT_BYTES = 2 * 1024 * 1024
MAX_RESPONSE_BYTES = 4 * 1024 * 1024
PROVIDERS = ("openai", "anthropic", "google")
IMAGE_TYPES = ("image/png", "image/jpeg", "image/webp", "image/gif")


class ProviderError(RuntimeError):
    pass


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, newurl):
        return None


@dataclass(frozen=True)
class ProviderConfig:
    base_url: str
    api_key: str = ""
    openai_token_field: str = "max_completion_tokens"
    openai_api: str = "chat_completions"

    def __post_init__(self):
        parsed = urllib.parse.urlsplit(self.base_url)
        local = parsed.hostname in ("localhost", "127.0.0.1", "::1")
        if (parsed.scheme not in ("http", "https") or not parsed.hostname
                or parsed.username is not None or parsed.password is not None
                or parsed.query or parsed.fragment or "\\" in self.base_url
                or any(character.isspace() for character in self.base_url)
                or parsed.scheme == "http" and not local):
            raise ValueError("Provider base URL must be HTTPS or local HTTP, without credentials or query parameters.")
        if self.openai_token_field not in ("max_completion_tokens", "max_tokens"):
            raise ValueError("OpenAI token field must be max_completion_tokens or max_tokens.")
        if self.openai_api not in ("responses", "chat_completions"):
            raise ValueError("OpenAI API must be responses or chat_completions.")
        if any(character in self.api_key for character in "\r\n"):
            raise ValueError("Invalid provider credential.")

    @property
    def configured(self) -> bool:
        return bool(self.api_key) or urllib.parse.urlsplit(self.base_url).hostname in ("localhost", "127.0.0.1", "::1")


def provider_configs(environment: Mapping[str, str]) -> dict[str, ProviderConfig]:
    return {
        "openai": ProviderConfig(environment.get("OPENAI_BASE_URL", "https://api.openai.com/v1"), environment.get("OPENAI_API_KEY", ""), environment.get("MINERA_OPENAI_TOKEN_FIELD", "max_completion_tokens"), environment.get("MINERA_OPENAI_API", "chat_completions")),
        "anthropic": ProviderConfig(environment.get("ANTHROPIC_BASE_URL", "https://api.anthropic.com/v1"), environment.get("ANTHROPIC_API_KEY", "")),
        "google": ProviderConfig(environment.get("GOOGLE_BASE_URL", "https://generativelanguage.googleapis.com/v1beta"), environment.get("GEMINI_API_KEY") or environment.get("GOOGLE_API_KEY", "")),
    }


def image_type(raw: bytes) -> str:
    if raw.startswith(b"\x89PNG\r\n\x1a\n"): return "image/png"
    if raw.startswith(b"\xff\xd8\xff"): return "image/jpeg"
    if raw.startswith((b"GIF87a", b"GIF89a")): return "image/gif"
    if raw.startswith(b"RIFF") and raw[8:12] == b"WEBP": return "image/webp"
    raise ValueError("Expected a PNG, JPEG, WebP or GIF image.")


def image_file(path: str) -> dict[str, str]:
    file = Path(path)
    with file.open("rb") as stream:
        raw = stream.read(MAX_INPUT_BYTES + 1)
    if not raw or len(raw) > MAX_INPUT_BYTES:
        raise ValueError("Image exceeds the runtime input bound.")
    return {"name": file.name, "media_type": image_type(raw), "data": base64.b64encode(raw).decode("ascii")}


def validate_request(value: Any) -> dict[str, Any]:
    if not isinstance(value, dict) or set(value) - {"provider", "model", "prompt", "max_tokens", "temperature", "images", "extra_payload", "openai_api"}:
        raise ValueError("Invalid completion request fields.")
    if value.get("provider") not in PROVIDERS:
        raise ValueError("Unsupported language-model provider.")
    if not isinstance(value.get("model"), str) or not value["model"].strip() or len(value["model"]) > 512:
        raise ValueError("Enter a model name.")
    if not isinstance(value.get("prompt"), str) or not value["prompt"].strip():
        raise ValueError("The prompt must not be empty.")
    # Encoding rejects lone surrogates before constructing a provider request.
    if len(value["prompt"].encode("utf-8")) > MAX_INPUT_BYTES:
        raise ValueError("The prompt exceeds the runtime input bound.")
    tokens = value.get("max_tokens", 4096)
    if type(tokens) is not int or not 1 <= tokens <= 1_000_000:
        raise ValueError("max_tokens must be an integer from 1 to 1000000.")
    temperature = value.get("temperature")
    maximum = 1 if value["provider"] == "anthropic" else 2
    if temperature is not None and (type(temperature) not in (int, float) or not math.isfinite(temperature) or not 0 <= temperature <= maximum):
        raise ValueError(f"temperature must be from 0 to {maximum}, or omitted for the model default.")
    if "openai_api" in value and (value["provider"] != "openai" or value["openai_api"] not in ("responses", "chat_completions")):
        raise ValueError("openai_api applies to OpenAI and must be responses or chat_completions.")
    images = value.get("images", [])
    if not isinstance(images, list) or len(images) > 8:
        raise ValueError("images must be a list of at most eight attachments.")
    for image in images:
        if not isinstance(image, dict) or set(image) - {"name", "media_type", "data", "detail"}:
            raise ValueError("Invalid image attachment.")
        if image.get("media_type") not in IMAGE_TYPES or not isinstance(image.get("data"), str):
            raise ValueError("Invalid image type or base64 data.")
        try: raw = base64.b64decode(image["data"], validate=True)
        except (ValueError, binascii.Error): raise ValueError("Invalid image base64 data.") from None
        if not raw or image_type(raw) != image["media_type"]:
            raise ValueError("Image type does not match its bytes.")
        if "name" in image and (not isinstance(image["name"], str) or len(image["name"]) > 512):
            raise ValueError("Invalid image name.")
        if image.get("detail", "auto") not in ("auto", "low", "high", "original"):
            raise ValueError("Invalid OpenAI image detail.")
    extra = value.get("extra_payload", {})
    if isinstance(extra, str): extra = json.loads(extra)
    if not isinstance(extra, dict):
        raise ValueError("extra_payload must be a JSON object.")
    if extra.get("stream") or extra.get("background"):
        raise ValueError("Streaming and background responses require separate consumers.")
    normalized = {**value, "model": value["model"].strip(), "max_tokens": tokens, "images": images, "extra_payload": extra}
    if len(json.dumps(normalized, ensure_ascii=False, allow_nan=False).encode("utf-8")) > MAX_INPUT_BYTES:
        raise ValueError("Request exceeds the runtime input bound.")
    return normalized


def token_count(value: Any) -> int | None:
    if value is None:
        return None
    if type(value) is not int or value < 0:
        raise ProviderError("Provider returned an invalid token count.")
    return value


def parse_completion(provider: str, response: Any) -> tuple[str, str, dict[str, int | None]]:
    try:
        if not isinstance(response, dict) or response.get("error"):
            raise ProviderError("Provider returned an error response.")
        if provider == "openai":
            usage = response.get("usage") or {}
            if "output" in response:
                parts = [part for item in response["output"] if item.get("type") == "message" for part in item["content"]]
                answers = [part["text"] for part in parts if part.get("type") == "output_text"]
                refusals = [part["refusal"] for part in parts if part.get("type") == "refusal"]
                if not answers and not refusals: raise ProviderError("Provider returned no answer text.")
                text = "".join(answers or refusals)
                finish = "refusal" if refusals and not answers else (response.get("incomplete_details") or {}).get("reason") or response.get("status", "completed")
                counts = {"input_tokens": token_count(usage.get("input_tokens")), "output_tokens": token_count(usage.get("output_tokens"))}
            else:
                choice = response["choices"][0]
                message = choice["message"]
                text = message.get("content")
                refusal = message.get("refusal")
                if text is None and isinstance(refusal, str): text = refusal
                finish = "refusal" if refusal else choice.get("finish_reason", "")
                counts = {"input_tokens": token_count(usage.get("prompt_tokens")), "output_tokens": token_count(usage.get("completion_tokens"))}
        elif provider == "anthropic":
            parts = response["content"]
            if not isinstance(parts, list):
                raise ProviderError("Provider returned invalid content blocks.")
            text = "".join(part["text"] for part in parts if part.get("type") == "text")
            if not any(part.get("type") == "text" for part in parts):
                raise ProviderError("Provider returned no text content.")
            finish = response.get("stop_reason", "")
            usage = response.get("usage") or {}
            input_tokens = token_count(usage.get("input_tokens"))
            if input_tokens is not None:
                input_tokens += sum(token_count(usage.get(key)) or 0 for key in ("cache_creation_input_tokens", "cache_read_input_tokens"))
            counts = {"input_tokens": input_tokens, "output_tokens": token_count(usage.get("output_tokens"))}
        else:
            candidate = response["candidates"][0]
            parts = candidate["content"]["parts"]
            text = "".join(part["text"] for part in parts if "text" in part and not part.get("thought", False))
            if not any("text" in part and not part.get("thought", False) for part in parts):
                raise ProviderError("Provider returned no answer text.")
            finish = candidate.get("finishReason", "")
            usage = response.get("usageMetadata") or {}
            counts = {"input_tokens": token_count(usage.get("promptTokenCount")), "output_tokens": token_count(usage.get("candidatesTokenCount"))}
        if not isinstance(text, str) or not isinstance(finish, str):
            raise ProviderError("Provider returned invalid text or finish reason.")
        return text, finish, counts
    except (KeyError, IndexError, TypeError, AttributeError) as error:
        raise ProviderError("Provider returned an invalid completion schema.") from error


class ProviderRuntime:
    def __init__(self, configs: Mapping[str, ProviderConfig], *, timeout: float = 60):
        if not math.isfinite(timeout) or not 0 < timeout <= 600:
            raise ValueError("Runtime timeout must be from 0 to 600 seconds.")
        self.configs = dict(configs)
        self.timeout = timeout
        self.opener = urllib.request.build_opener(NoRedirect)

    def status(self) -> dict[str, Any]:
        return {"family": "LLM_RUNTIME", "ready": True, "providers": [{"id": name, "configured": self.configs[name].configured, "base_url": self.configs[name].base_url} for name in PROVIDERS], "max_input_bytes": MAX_INPUT_BYTES}

    def complete(self, value: Any) -> dict[str, Any]:
        request = validate_request(value)
        provider, model, prompt = (request[key] for key in ("provider", "model", "prompt"))
        config = self.configs[provider]
        if not config.configured:
            raise ValueError("Configure the selected provider credential in the runtime environment.")
        headers = {"Content-Type": "application/json", "Accept": "application/json"}
        images = request["images"]
        if provider == "openai":
            api = request.get("openai_api", config.openai_api)
            if api == "responses":
                url = config.base_url.rstrip("/") + "/responses"
                content = [{"type": "input_text", "text": prompt}] + [{"type": "input_image", "image_url": f'data:{image["media_type"]};base64,{image["data"]}', **({"detail": image["detail"]} if "detail" in image else {})} for image in images]
                payload = {"model": model, "input": [{"role": "user", "content": content}], "max_output_tokens": request["max_tokens"]}
            else:
                url = config.base_url.rstrip("/") + "/chat/completions"
                content = prompt if not images else [{"type": "text", "text": prompt}] + [{"type": "image_url", "image_url": {"url": f'data:{image["media_type"]};base64,{image["data"]}', **({"detail": image["detail"]} if "detail" in image else {})}} for image in images]
                payload = {"model": model, "messages": [{"role": "user", "content": content}], config.openai_token_field: request["max_tokens"]}
            if config.api_key: headers["Authorization"] = "Bearer " + config.api_key
            if request.get("temperature") is not None: payload["temperature"] = request["temperature"]
        elif provider == "anthropic":
            url = config.base_url.rstrip("/") + "/messages"
            content = prompt if not images else [{"type": "text", "text": prompt}] + [{"type": "image", "source": {"type": "base64", "media_type": image["media_type"], "data": image["data"]}} for image in images]
            payload = {"model": model, "messages": [{"role": "user", "content": content}], "max_tokens": request["max_tokens"], "stream": False}
            headers["anthropic-version"] = "2023-06-01"
            if config.api_key: headers["x-api-key"] = config.api_key
            if request.get("temperature") is not None: payload["temperature"] = request["temperature"]
        else:
            name = model.removeprefix("models/")
            url = config.base_url.rstrip("/") + "/models/" + urllib.parse.quote(name, safe="") + ":generateContent"
            generation = {"maxOutputTokens": request["max_tokens"]}
            if request.get("temperature") is not None: generation["temperature"] = request["temperature"]
            payload = {"contents": [{"role": "user", "parts": [{"text": prompt}] + [{"inlineData": {"mimeType": image["media_type"], "data": image["data"]}} for image in images]}], "generationConfig": generation}
            if config.api_key: headers["x-goog-api-key"] = config.api_key
        payload.update(request["extra_payload"])
        raw = json.dumps(payload, ensure_ascii=False, allow_nan=False).encode("utf-8")
        outbound = urllib.request.Request(url, data=raw, headers=headers, method="POST")
        try:
            with self.opener.open(outbound, timeout=self.timeout) as response:
                body = response.read(MAX_RESPONSE_BYTES + 1)
                if len(body) > MAX_RESPONSE_BYTES:
                    raise ProviderError("Provider response exceeds the runtime bound.")
                parsed = json.loads(body)
        except urllib.error.HTTPError as error:
            # Upstream URLs/error bodies can contain credentials. Do not echo them.
            status = error.code
            error.close()
            raise ProviderError(f"{provider} returned HTTP {status}.") from None
        except (urllib.error.URLError, TimeoutError, OSError):
            raise ProviderError("Could not reach the selected language-model provider.") from None
        except (ValueError, UnicodeError):
            raise ProviderError("Provider returned invalid JSON.") from None
        text, finish, usage = parse_completion(provider, parsed)
        effective_model = parsed.get("model") or (payload.get("model") if provider != "google" else model)
        if not isinstance(effective_model, str): effective_model = model
        return {"family": "LLM_COMPLETION", "provider": provider, "model": effective_model, "text": text, "finish_reason": finish, "usage": usage, "response": parsed}


def handler_for(runtime: ProviderRuntime):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, format, *args):
            pass

        def send_json(self, status: int, value: Any):
            encoded = json.dumps(value, ensure_ascii=False, allow_nan=False).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(encoded)))
            self.end_headers()
            try: self.wfile.write(encoded)
            except (BrokenPipeError, ConnectionResetError): pass

        def do_GET(self):
            self.send_json(200, runtime.status()) if self.path == "/health" else self.send_json(404, {"error": "Unknown runtime route."})

        def do_POST(self):
            if self.path != "/complete":
                self.send_json(404, {"error": "Unknown runtime route."}); return
            try:
                if self.headers.get("Origin"):
                    self.send_json(403, {"error": "Use the native Minera application bridge."}); return
                length = int(self.headers.get("Content-Length", "0"))
                if not 0 < length <= MAX_INPUT_BYTES:
                    self.send_json(413, {"error": "Request exceeds the runtime input bound."}); return
                if self.headers.get_content_type() != "application/json":
                    self.send_json(415, {"error": "Expected application/json."}); return
                result = runtime.complete(json.loads(self.rfile.read(length)))
                self.send_json(200, result)
            except (ValueError, UnicodeError) as error:
                self.send_json(400, {"error": str(error)})
            except ProviderError as error:
                self.send_json(502, {"error": str(error)})
    return Handler


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1", choices=("127.0.0.1", "localhost"))
    parser.add_argument("--port", type=int, default=8189)
    parser.add_argument("--timeout", type=float, default=60)
    parser.add_argument("--complete", metavar="JSON_FILE")
    parser.add_argument("--image", action="append", default=[], metavar="IMAGE_FILE", help="Attach an image to --complete; repeat for multiple images.")
    args = parser.parse_args(argv)
    try:
        runtime = ProviderRuntime(provider_configs(os.environ), timeout=args.timeout)
        if args.complete:
            raw = sys.stdin.buffer.read(MAX_INPUT_BYTES+1) if args.complete == "-" else Path(args.complete).read_bytes()
            if len(raw) > MAX_INPUT_BYTES: raise ValueError("Request exceeds the runtime input bound.")
            value = json.loads(raw)
            if args.image:
                value["images"] = [*value.get("images", []), *(image_file(path) for path in args.image)]
            result = runtime.complete(value)
            print(json.dumps(result, ensure_ascii=False, allow_nan=False)); return 0
        if not 1 <= args.port <= 65535: raise ValueError("Invalid runtime port.")
        server = ThreadingHTTPServer((args.host, args.port), handler_for(runtime))
        print(f"Minera LLM runtime listening on {args.host}:{args.port}", flush=True)
        try: server.serve_forever()
        except KeyboardInterrupt: pass
        finally: server.server_close()
        return 0
    except (ValueError, UnicodeError, OSError, ProviderError) as error:
        print(f"error: {error}", file=sys.stderr); return 1


if __name__ == "__main__":
    raise SystemExit(main())
