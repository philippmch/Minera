"""Public-only checks for date grouping and schema-driven Rust extensions."""
import argparse
import hashlib
import json
import os
import runpy
import tempfile
from importlib.metadata import version
from pathlib import Path
import warnings


REFERENCE_VERSION = "2.7.22"
BACKEND_VERSION = "0.6.2"
REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"
REFERENCE_COMMIT_SCOPE = "configured_reference_pin"


def _load_public_reference_api():
    warnings.filterwarnings("ignore")
    import pm4py
    from pm4py.objects.log.importer.xes import importer
    return pm4py, importer


def _require_version(pm4py):
    actual = getattr(pm4py, "__version__", None)
    if actual != REFERENCE_VERSION:
        raise RuntimeError(
            f"This oracle requires pm4py=={REFERENCE_VERSION}; got {actual!r}")


def _require_backend(actual):
    if actual != BACKEND_VERSION:
        raise RuntimeError(f"This oracle requires r4pm=={BACKEND_VERSION}; got {actual!r}")


def _environment():
    return {"PYTHONHASHSEED": os.environ.get("PYTHONHASHSEED"),
            "TZ": os.environ.get("TZ")}


def _file_metadata(path):
    resolved = Path(path).resolve()
    digest = hashlib.sha256()
    try:
        with resolved.open("rb") as source:
            for chunk in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(chunk)
    except OSError as error:
        raise RuntimeError(f"Unable to hash {resolved}: {error}") from error
    return {"path": str(resolved), "sha256": digest.hexdigest()}


def _module_metadata(module):
    filename = getattr(module, "__file__", None)
    return _file_metadata(filename) if filename else None


CAPTURE_ID = 'rustxes_conversion_schema_reference'
EXPECTED_OBSERVATION_COUNT = 7
EXPECTED_OBSERVATION_IDENTITIES = (
    'date_glue_equal_instants', 'discarded_reserved_extension', 'null_schema_extension',
    'discarded_reserved_extension_default', 'null_schema_extension_default',
    'missing_glue_null_prefix', 'null_glue_null_prefix',
)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    pm4py, importer = _load_public_reference_api()
    _require_version(pm4py)
    backend = version('r4pm')
    _require_backend(backend)
    helpers = runpy.run_path(str(Path(__file__).with_name('capture-xes-rust-conversion-reference.py')))
    attribute, event, trace, capture = (helpers[key] for key in ('attribute', 'event', 'trace', 'capture_value'))
    cases = [
        ('date_glue_equal_instants', '<log>' + ''.join(
            trace(str(index), event(str(index), attribute('date', 'gid', date)))
            for index, date in enumerate(('2024-01-01T00:00:00Z', '2024-01-01T00:00:00.000000Z',
                                          '2024-01-01T01:00:00+01:00', '2024-01-01T00:00:00.000001Z'))
        ) + '</log>', {'pm4py:param:case_id_key': 'gid'}),
        ('discarded_reserved_extension', '<log>' + trace('c', event('A', attribute('string', 'case:org:resource', 'r'))) + '</log>', {'case_attribute_prefix': 'x:'}),
        ('null_schema_extension', '<log>' + trace('c', event('A', attribute('string', 'org:resource', null=True))) + '</log>', {'case_attribute_prefix': 'x:'}),
    ]
    cases.extend((name + '_default', xml, {}) for name, xml, _ in list(cases)[1:])
    cases.extend([
        ('missing_glue_null_prefix', '<log>' + trace('c', event()) + '</log>',
         {'pm4py:param:case_id_key': 'missing', 'case_attribute_prefix': None}),
        ('null_glue_null_prefix', '<log>' + trace('c', event('A', attribute('string', 'gid', null=True))) + '</log>',
         {'pm4py:param:case_id_key': 'gid', 'case_attribute_prefix': None}),
    ])
    observations = []
    with tempfile.TemporaryDirectory(prefix='minera-rust-schema-') as directory:
        path = Path(directory) / 'input.xes'
        for name, xml, parameters in cases:
            path.write_text(xml, encoding='utf-8')
            try:
                result = capture(importer.apply(str(path), variant=importer.Variants.RUSTXES, parameters=parameters))
            except BaseException as error:
                result = {'error': type(error).__name__, 'message': str(error)}
            observations.append(dict(name=name, observation_id=name, case=name, xml=xml,
                                     parameters=parameters, result=result))
    observed_ids = tuple(observation.get('observation_id') for observation in observations)
    if (observed_ids != EXPECTED_OBSERVATION_IDENTITIES
            or len(set(observed_ids)) != EXPECTED_OBSERVATION_COUNT):
        raise RuntimeError('schema capture observation manifest is incomplete or duplicated')
    validation_errors = []
    if not observations or len(observations[0]['result'].get('traces', [])) != 2:
        validation_errors.append('date_glue_equal_instants did not produce two traces')
    if any('Organizational' not in observation['result'].get('metadata', {}).get('extensions', {})
           for observation in observations[1:5]):
        validation_errors.append('schema extension observations lack Organizational extension')
    artifact = {
        'capture_id': CAPTURE_ID,
        'expected_observation_count': EXPECTED_OBSERVATION_COUNT,
        'expected_observation_identities': list(EXPECTED_OBSERVATION_IDENTITIES),
        'reference': pm4py.__version__,
        'reference_commit': REFERENCE_COMMIT,
        'reference_commit_scope': REFERENCE_COMMIT_SCOPE,
        'environment': _environment(),
        'reference_module': _module_metadata(importer),
        'r4pm': backend,
        'observations': observations,
        'errors': validation_errors,
        'validation_errors': validation_errors,
    }
    args.output.write_text(json.dumps(artifact, indent=2) + '\n', encoding='utf-8')
    print(f'Captured {len(observations)} date-group/schema observations')
    if validation_errors:
        for error in validation_errors:
            print(f'Validation failure: {error}')
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
