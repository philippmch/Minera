"""Save a visualization and prepare or execute its image explanation.

CLI savers receive JSON positional arguments followed by the output image path.
Use --saver package.module:function and --args-file input.json.
"""
from __future__ import annotations
import argparse
import importlib
import json
from pathlib import Path
import tempfile
import urllib.error
import urllib.parse
import urllib.request
if __package__:
    from .llm_runtime import MAX_INPUT_BYTES, MAX_RESPONSE_BYTES, NoRedirect, ProviderError, ProviderRuntime, image_file, provider_configs
else:
    from llm_runtime import MAX_INPUT_BYTES, MAX_RESPONSE_BYTES, NoRedirect, ProviderError, ProviderRuntime, image_file, provider_configs
import os


def visualization_prompt(description: str = "") -> str:
    return "Could you explain the included process mining visualization?\n\n" + description


def prepare_visualization(vis_saver, *args, description="", **kwargs):
    with tempfile.TemporaryDirectory(prefix="minera-visualization-") as directory:
        path = str(Path(directory) / "visualization.png")
        vis_saver(*args, path, **kwargs)
        return {"prompt": visualization_prompt(description), "images": [image_file(path)]}


def explain_visualization(vis_saver, *args, connector, description="", **kwargs):
    """Call a saver, then the selected connector with the image path and options."""
    with tempfile.TemporaryDirectory(prefix="minera-visualization-") as directory:
        path = str(Path(directory) / "visualization.png")
        vis_saver(*args, path, **kwargs)
        return connector(visualization_prompt(description), image_path=path, **kwargs)


def runtime_completion(url, request):
    address=urllib.parse.urlsplit(url)
    if address.scheme not in ("http","https") or not address.hostname or address.username or address.password or address.query or address.fragment or address.path not in ("","/"):
        raise ValueError("Runtime URL must be HTTP(S) host and port without credentials or path.")
    raw = json.dumps(request, ensure_ascii=False).encode("utf-8")
    if len(raw) > MAX_INPUT_BYTES: raise ValueError("Visualization request exceeds 2 MiB.")
    outbound = urllib.request.Request(url.rstrip("/") + "/complete", data=raw, headers={"Content-Type": "application/json"})
    try:
        with urllib.request.build_opener(NoRedirect).open(outbound, timeout=90) as response:
            raw=response.read(MAX_RESPONSE_BYTES+1)
            if len(raw)>MAX_RESPONSE_BYTES:raise ProviderError("Visualization response exceeds 4 MiB.")
            result = json.loads(raw)
    except urllib.error.HTTPError as error:
        status = error.code; error.close()
        raise ProviderError(f"Visualization runtime returned HTTP {status}.") from None
    if not isinstance(result, dict) or result.get("family") != "LLM_COMPLETION":
        raise ProviderError("Visualization runtime returned an invalid completion.")
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--saver", required=True)
    parser.add_argument("--args-file", required=True)
    parser.add_argument("--options-file")
    parser.add_argument("--description", default="")
    parser.add_argument("--provider", default="openai", choices=("openai", "anthropic", "google"))
    parser.add_argument("--model")
    parser.add_argument("--runtime-url")
    parser.add_argument("--openai-api", default="responses", choices=("responses", "chat_completions"))
    parser.add_argument("--max-tokens", type=int, default=4096)
    parser.add_argument("--prompt-only", action="store_true")
    parser.add_argument("--output")
    args = parser.parse_args(argv)
    try:
        module, name = args.saver.rsplit(":", 1)
        saver = getattr(importlib.import_module(module), name)
        values = json.loads(Path(args.args_file).read_bytes())
        options = json.loads(Path(args.options_file).read_bytes()) if args.options_file else {}
        if not isinstance(values, list) or not isinstance(options, dict):
            raise ValueError("Saver arguments must be a JSON list and options a JSON object.")
        draft = prepare_visualization(saver, *values, description=args.description, **options)
        request = {**draft, "provider": args.provider, "model": args.model or "", "max_tokens": args.max_tokens}
        if args.provider == "openai": request["openai_api"] = args.openai_api
        report = {"family": "LLM_VISUALIZATION", "saver": args.saver, "request": request}
        if not args.prompt_only:
            if not args.model: raise ValueError("Enter a model name or use --prompt-only.")
            report["completion"] = runtime_completion(args.runtime_url, request) if args.runtime_url else ProviderRuntime(provider_configs(os.environ)).complete(request)
        encoded = json.dumps(report, ensure_ascii=False, allow_nan=False) + "\n"
        if args.output: Path(args.output).write_text(encoded, encoding="utf-8")
        else: print(encoded, end="")
        return 0
    except (ValueError, AttributeError, ImportError, OSError, ProviderError) as error:
        print("error: " + str(error), file=__import__("sys").stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
