"""Derive Rust debug escaping ranges exclusively from public RUSTXES outputs."""
import argparse
import hashlib
import json
import os
import subprocess
import tempfile
from importlib.metadata import version
from pathlib import Path
import warnings


REFERENCE_VERSION = '2.7.22'
BACKEND_VERSION = '0.6.2'
REFERENCE_COMMIT = '0715c31f3f1d85924107d6aa5329806db69df899'
REFERENCE_COMMIT_SCOPE = 'configured_reference_pin'
pm4py = importer = None


def _load_public_reference_api():
    global pm4py, importer
    warnings.filterwarnings('ignore')
    import pm4py as pm4py_module
    from pm4py.objects.log.importer.xes import importer as importer_module
    pm4py, importer = pm4py_module, importer_module
    return pm4py, importer


def _require_version(pm4py_module):
    actual = getattr(pm4py_module, '__version__', None)
    if actual != REFERENCE_VERSION:
        raise RuntimeError(f'This oracle requires pm4py=={REFERENCE_VERSION}; got {actual!r}')


def _require_backend(actual):
    if actual != BACKEND_VERSION:
        raise RuntimeError(f'This oracle requires r4pm=={BACKEND_VERSION}; got {actual!r}')


def _environment():
    return {'PYTHONHASHSEED': os.environ.get('PYTHONHASHSEED'), 'TZ': os.environ.get('TZ')}


def _file_metadata(path):
    resolved = Path(path).resolve()
    digest = hashlib.sha256()
    try:
        with resolved.open('rb') as source:
            for chunk in iter(lambda: source.read(1024 * 1024), b''):
                digest.update(chunk)
    except OSError as error:
        raise RuntimeError(f'Unable to hash {resolved}: {error}') from error
    return {'path': str(resolved), 'sha256': digest.hexdigest()}


def _module_metadata(module):
    filename = getattr(module, '__file__', None)
    return _file_metadata(filename) if filename else None


def _native_metadata(path):
    try:
        return _file_metadata(path)
    except (OSError, RuntimeError) as error:
        return {'path': str(Path(path).resolve()), 'sha256': None, 'error': str(error)}


def _native_integrity(before, after):
    metadata = {
        'path': before.get('path', after.get('path')),
        'sha256_before': before.get('sha256'),
        'sha256_after': after.get('sha256'),
    }
    errors = []
    if after.get('sha256') is not None:
        metadata['sha256'] = after['sha256']
    if before.get('error'):
        metadata['error'] = before['error']
        metadata['error_before'] = before['error']
        errors.append({'kind': 'native_hash_error', 'phase': 'before',
                       'error': before['error']})
    if after.get('error'):
        metadata['error'] = after['error']
        metadata['error_after'] = after['error']
        errors.append({'kind': 'native_hash_error', 'phase': 'after',
                       'error': after['error']})
    if (before.get('sha256') is not None and after.get('sha256') is not None
            and before['sha256'] != after['sha256']):
        metadata['changed'] = True
        errors.append({'kind': 'native_executable_changed',
                       'sha256_before': before['sha256'],
                       'sha256_after': after['sha256']})
    return metadata, errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--header', type=Path)
    parser.add_argument('--native', type=Path)
    args = parser.parse_args()
    pm4py, importer = _load_public_reference_api()
    _require_version(pm4py)
    backend = version('r4pm')
    _require_backend(backend)
    escaped = []
    checked = 0
    errors = []
    native_before = _native_metadata(args.native) if args.native else None
    with tempfile.TemporaryDirectory(prefix='minera-rust-unicode-') as directory:
        path = Path(directory) / 'input.xes'
        for start in range(128, 0x110000, 2048):
            points = [p for p in range(start, min(start + 2048, 0x110000)) if not 0xD800 <= p <= 0xDFFF and p not in (0xFFFE, 0xFFFF)]
            if not points:
                continue
            value = '|'.join(chr(p) for p in points)
            path.write_text('<log><trace><string key="concept:name" value="c"/><event><string key="concept:name" value="A"/><list key="probe"><string key="u" value="' + value + '"/></list></event></trace></log>', encoding='utf-8')
            try:
                log = importer.apply(str(path), variant=importer.Variants.RUSTXES, parameters={'show_progress_bar': False})
                actual = log[0][0]['probe']
            except BaseException as error:
                errors.append({'chunk_start': start, 'kind': 'public_capture_error',
                               'error': type(error).__name__, 'message': str(error)})
                continue
            prefix, suffix = '[Attribute { key: "u", value: String("', '"), own_attributes: None }]'
            if not isinstance(actual, str) or not actual.startswith(prefix) or not actual.endswith(suffix):
                errors.append({'chunk_start': start, 'kind': 'public_shape_mismatch',
                               'actual': repr(actual)})
                continue
            if args.native:
                try:
                    native = subprocess.run([str(args.native.resolve()), str(path), '--xes-import-variant', 'rustxes', '--log-convert', 'to_event_log'], capture_output=True, text=True, encoding='utf-8')
                    if native.returncode:
                        raise RuntimeError(f'native process exited {native.returncode}: {native.stderr}')
                    attributes = json.loads(native.stdout)['log']['traces'][0]['events'][0]['attributes']
                    result = next(a for a in attributes if a['key'] == 'probe')
                    if result['value'] != actual or result['container'] != 'scalar' or result['kind'] != 'string':
                        errors.append({'chunk_start': start, 'kind': 'native_mismatch',
                                       'expected': actual, 'actual': result,
                                       'stdout': native.stdout, 'stderr': native.stderr})
                except (OSError, subprocess.TimeoutExpired) as error:
                    errors.append({'chunk_start': start, 'kind': 'native_process_error',
                                   'error': type(error).__name__, 'message': str(error)})
                except (KeyError, IndexError, TypeError, ValueError, StopIteration, json.JSONDecodeError) as error:
                    errors.append({'chunk_start': start, 'kind': 'native_malformed_output',
                                   'error': type(error).__name__, 'message': str(error),
                                   'stdout': locals().get('native').stdout if 'native' in locals() else '',
                                   'stderr': locals().get('native').stderr if 'native' in locals() else ''})
                except RuntimeError as error:
                    errors.append({'chunk_start': start, 'kind': 'native_process_error',
                                   'error': type(error).__name__, 'message': str(error),
                                   'stdout': native.stdout, 'stderr': native.stderr})
            segments = actual[len(prefix):-len(suffix)].split('|')
            if len(segments) != len(points):
                errors.append({'chunk_start': start, 'kind': 'public_segment_count_mismatch',
                               'expected': len(points), 'actual': len(segments)})
                continue
            for point, segment in zip(points, segments):
                if segment == f'\\u{{{point:x}}}':
                    escaped.append(point)
                else:
                    if segment != chr(point):
                        errors.append({'chunk_start': start, 'kind': 'public_escape_mismatch',
                                       'codepoint': point, 'actual': repr(segment)})
                        break
            checked += len(points)
    ranges = []
    for point in escaped:
        if ranges and point == ranges[-1][1] + 1:
            ranges[-1][1] = point
        else:
            ranges.append([point, point])
    native_after = _native_metadata(args.native) if args.native else None
    native_executable = None
    if args.native:
        native_executable, integrity_errors = _native_integrity(native_before, native_after)
        errors.extend(integrity_errors)
    artifact = dict(reference=pm4py.__version__, reference_commit=REFERENCE_COMMIT,
                    reference_commit_scope=REFERENCE_COMMIT_SCOPE,
                    environment=_environment(), reference_module=_module_metadata(importer),
                    r4pm=backend, native_executable=native_executable,
                    checked=checked, ranges=ranges, errors=errors)
    args.output.write_text(json.dumps(artifact, indent=2) + '\n', encoding='utf-8')
    if args.header:
        rows = '\n'.join(f'    {{0x{lo:x}, 0x{hi:x}}},' for lo, hi in ranges)
        args.header.write_text('''// Generated from public PM4Py 2.7.22 / r4pm 0.6.2 RUSTXES observations.
// Reproduce with scripts/capture-xes-rust-unicode-reference.py; no source reads.
#pragma once
#include <cstdint>
#include <iterator>
#include <utility>

namespace minera::detail {
inline bool rust_xes_debug_escape(std::uint32_t point) {
  if (point < 128) return point < 32 || point == 127;
  static constexpr std::pair<std::uint32_t, std::uint32_t> ranges[] = {
''' + rows + '''
  };
  std::size_t left = 0, right = std::size(ranges);
  while (left < right) {
    const auto middle = left + (right - left) / 2;
    if (point < ranges[middle].first) right = middle;
    else if (point > ranges[middle].second) left = middle + 1;
    else return true;
  }
  return false;
}
}  // namespace minera::detail
''', encoding='utf-8')
    print(f'Checked {checked} code points; {len(ranges)} escape intervals')
    if errors:
        print(f'Unicode evidence failures: {len(errors)}')
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
