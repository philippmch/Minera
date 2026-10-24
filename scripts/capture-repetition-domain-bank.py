"""Capture a disjoint public attribute-value repetition domain bank.

For one engine (pandas, polars or log), generates seeded cases with
scripts/repetition_domain.py, runs every call of every case in a fresh
interpreter per PYTHONHASHSEED, and stores the native transport of each input
together with the summarized public outcome of each process. Inputs are
rebuilt for every call, so a call cannot observe another call's mutation.
check-repetition-domain-bank.py compares the bank with the native workbench
without the reference runtime.
"""
import argparse
import copy
import gzip
import hashlib
import importlib.util
import json
import os
import subprocess
import sys
import warnings
from pathlib import Path

HERE = Path(__file__).resolve().parent
BANK_SEED = 20261024


def domain():
    spec = importlib.util.spec_from_file_location("repetition_domain", HERE / "repetition_domain.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def public_parameters(engine, params):
    if engine == "log":
        from pm4py.algo.filtering.log.attr_value_repetition.filter import Parameters
    elif engine == "pandas":
        from pm4py.algo.filtering.pandas.attr_value_repetition.filter import Parameters
    else:
        from pm4py.algo.filtering.polars.attr_value_repetition.filter import Parameters
    names = {"min_rep": Parameters.MIN_REP, "max_rep": Parameters.MAX_REP,
             "attribute_key": Parameters.ATTRIBUTE_KEY}
    if engine != "log":
        names["case_id_key"] = Parameters.CASE_ID_KEY
    return {names[k]: v for k, v in params.items()}


def error_outcome(error):
    return {"error": type(error).__name__, "message": str(error)}


def run_cell(engine, count, bank_seed):
    warnings.filterwarnings("ignore")
    d = domain()
    rows = []
    if engine == "pandas":
        from pm4py.algo.filtering.pandas.attr_value_repetition import filter as public
        for spec in d.generate_pandas_cases(count, bank_seed):
            frame, calls = d.build_pandas_case(spec)
            source_json = d.pandas_table_json(frame)
            for target, params in calls:
                frame, _ = d.build_pandas_case(spec)
                before = d.pandas_table_json(frame)
                try:
                    result = public.apply(frame, target, parameters=public_parameters(engine, params) or None)
                    outcome = {"summary": d.table_summary(source_json, d.pandas_table_json(result))}
                except Exception as error:
                    outcome = error_outcome(error)
                outcome["source_unchanged"] = d.pandas_table_json(frame) == before
                rows.append(outcome)
    elif engine == "polars":
        from pm4py.algo.filtering.polars.attr_value_repetition import filter as public
        for spec in d.generate_polars_cases(count, bank_seed):
            frame, calls = d.build_polars_case(spec)
            source_json = d.polars_table_json(frame)
            for target, params in calls:
                frame, _ = d.build_polars_case(spec)
                outcome = {}
                try:
                    lazy = public.apply(frame.lazy(), target, parameters=public_parameters(engine, params) or None)
                except Exception as error:
                    outcome = error_outcome(error) | {"phase": "apply"}
                else:
                    try:
                        outcome = {"summary": d.table_summary(source_json, d.polars_table_json(lazy.collect()))}
                    except Exception as error:
                        outcome = error_outcome(error) | {"phase": "collect"}
                rows.append(outcome)
    else:
        from pm4py.algo.filtering.log.attr_value_repetition import filter as public
        for spec in d.generate_log_cases(count, bank_seed):
            xes, calls = d.build_log_case(spec)
            for target, params in calls:
                log = d.import_log(xes)
                before = d.log_fingerprint(log)
                try:
                    result = public.apply(log, target, parameters=public_parameters(engine, params) or None)
                    outcome = {"retained": d.log_retained(log, result)}
                except Exception as error:
                    outcome = error_outcome(error)
                outcome["source_unchanged"] = d.log_fingerprint(log) == before
                rows.append(outcome)
    return rows


def case_inputs(engine, count, bank_seed):
    """Native transport for every call, in call order."""
    d = domain()
    out = []
    if engine == "pandas":
        for c, spec in enumerate(d.generate_pandas_cases(count, bank_seed)):
            frame, calls = d.build_pandas_case(spec)
            source = d.pandas_table_json(frame)
            for target, params in calls:
                out.append(d.native_call(c, spec, source, target, params))
    elif engine == "polars":
        for c, spec in enumerate(d.generate_polars_cases(count, bank_seed)):
            frame, calls = d.build_polars_case(spec)
            source = d.polars_table_json(frame)
            for target, params in calls:
                out.append(d.native_call(c, spec, source, target, params))
    else:
        for c, spec in enumerate(d.generate_log_cases(count, bank_seed)):
            source, calls = d.build_log_case(spec)
            for target, params in calls:
                out.append(d.native_call(c, spec, source, target, params))
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", choices=("pandas", "polars", "log"), required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--cases", type=int, default=400)
    parser.add_argument("--seeds", type=int, default=4)
    parser.add_argument("--bank-seed", type=int, default=BANK_SEED)
    parser.add_argument("--cell", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.cell:
        json.dump(run_cell(args.engine, args.cases, args.bank_seed), sys.stdout)
        return
    import pm4py
    assert pm4py.__version__ == "2.7.22"
    import numpy, pandas, polars
    calls = case_inputs(args.engine, args.cases, args.bank_seed)
    outcomes, index = [], {}

    def intern(value):
        text = json.dumps(value, sort_keys=True)
        if text not in index:
            index[text] = len(outcomes)
            outcomes.append(value)
        return index[text]

    for seed in range(args.seeds):
        env = dict(os.environ, PYTHONHASHSEED=str(seed))
        run = subprocess.run([sys.executable, "-B", "-X", "utf8", __file__, "--cell", "--engine", args.engine,
                              "--cases", str(args.cases), "--bank-seed", str(args.bank_seed)],
                             env=env, capture_output=True, text=True, encoding="utf-8")
        if run.returncode:
            sys.stderr.write(run.stderr)
            raise SystemExit(f"hash seed {seed} failed")
        observed = json.loads(run.stdout)
        assert len(observed) == len(calls)
        for call, outcome in zip(calls, observed):
            call.setdefault("observations", []).append(intern(outcome))
        print(f"{args.engine} hash seed {seed}: {len(observed)} calls", file=sys.stderr)
    body = dict(engine=args.engine, reference=pm4py.__version__, python=sys.version.split()[0],
                versions=dict(pandas=pandas.__version__, polars=polars.__version__, numpy=numpy.__version__),
                bank_seed=args.bank_seed, cases=args.cases, hash_seeds=list(range(args.seeds)),
                outcomes=outcomes, calls=calls)
    data = json.dumps(body, sort_keys=True).encode("utf-8")
    with open(args.output, "wb") as raw, gzip.GzipFile(filename="", fileobj=raw, mode="wb", mtime=0) as out:
        out.write(data)
    print(f"{len(calls)} calls, {len(outcomes)} distinct outcomes, sha256 "
          f"{hashlib.sha256(data).hexdigest()}", file=sys.stderr)


if __name__ == "__main__":
    main()
