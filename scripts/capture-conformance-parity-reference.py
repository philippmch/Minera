"""Capture black-box conformance observations from the pinned reference release.

Covers Petri-net alignments (all four enum variants plus both generator
modules), process-tree alignments (all four variants), classic DFG alignments,
RECOMPOS_MAXIMAL decomposed alignments, and forward/backwards token replay.

Only public entry points are called; implementation source is never read.
Inputs are generated deterministically from --seed: random process trees are
converted to workflow nets through the public conversion API, serialized to
PNML/PTML through the public exporters, and paired with model-language traces
plus noisy variants. Run in the pinned runtime (pm4py 2.7.22, pandas 3.0.5,
numpy 2.4.6, scipy 1.17.1, networkx 3.6.1, PuLP 2.9.0 with its bundled CBC).
"""
import argparse
import contextlib
import gzip
import hashlib
import itertools
import json
import math
import os
import random
import re
import signal
import sys
import tempfile
import warnings
from pathlib import Path

REFERENCE_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899"
ALPHABET = "abcdefgh"
GENERATOR_LIMIT = 64
# Public token-replay parameters, each toggled away from its default.
REPLAY_OPTIONS = [("consider_remaining_in_fitness", False),
                  ("try_to_reach_final_marking_through_hidden", False),
                  ("walk_through_hidden_trans", False),
                  ("stop_immediately_unfit", True),
                  ("cleaning_token_flood", True),
                  ("consider_activities_not_in_model_in_fitness", True),
                  ("exhaustive_invisible_exploration", True)]
BACKWARDS_OPTIONS = {"consider_remaining_in_fitness", "try_to_reach_final_marking_through_hidden",
                     "walk_through_hidden_trans", "stop_immediately_unfit", "cleaning_token_flood"}
EXPECTED_VERSIONS = {"pm4py": "2.7.22", "pandas": "3.0.5", "numpy": "2.4.6",
                     "scipy": "1.17.1", "networkx": "3.6.1", "pulp": "2.9.0"}


@contextlib.contextmanager
def silenced():
    """Discard solver/progress chatter written directly to fds 1 and 2."""
    sys.stdout.flush(); sys.stderr.flush()
    saved = os.dup(1), os.dup(2)
    with open(os.devnull, "w") as sink:
        os.dup2(sink.fileno(), 1); os.dup2(sink.fileno(), 2)
        try:
            yield
        finally:
            sys.stdout.flush(); sys.stderr.flush()
            os.dup2(saved[0], 1); os.dup2(saved[1], 2)
            os.close(saved[0]); os.close(saved[1])


# ---------------------------------------------------------------- corpus

def random_tree(rng, depth, labels):
    """Return a tree in the reference's textual grammar, consuming labels."""
    if depth == 0 or not labels or rng.random() < 0.25:
        if not labels or rng.random() < 0.12:
            return "tau"
        return "'" + labels.pop() + "'"
    op = rng.choice(["->", "->", "X", "+", "*"])
    arity = 2 if op == "*" else rng.choice([2, 2, 3])
    children = [random_tree(rng, depth - 1, labels) for _ in range(arity)]
    if op == "*" and children[0] == "tau" and children[1] == "tau":
        children[1] = "'" + labels.pop() + "'" if labels else "'h'"
    return op + "(" + ", ".join(children) + ")"


def sample_language(tree, rng):
    """Sample one word of the tree's language (bounded loop unrolling)."""
    def walk(node):
        if node.operator is None:
            return [] if node.label is None else [node.label]
        name = str(node.operator)
        kids = node.children
        if name == "->":
            return [x for k in kids for x in walk(k)]
        if name == "X":
            return walk(rng.choice(kids))
        if name == "+":
            parts = [walk(k) for k in kids]
            out = []
            while any(parts):
                part = rng.choice([p for p in parts if p])
                out.append(part.pop(0))
            return out
        if name == "*":
            out = walk(kids[0])
            while rng.random() < 0.35 and len(out) < 12:
                for redo in kids[1:]:
                    out += walk(redo)
                out += walk(kids[0])
            return out
        raise ValueError(name)
    return walk(tree)


def noisy(word, rng):
    word = list(word)
    kind = rng.choice(["insert", "delete", "swap", "replace", "unknown", "duplicate"])
    if kind == "insert" or not word:
        word.insert(rng.randrange(len(word) + 1), rng.choice(ALPHABET))
    elif kind == "delete":
        del word[rng.randrange(len(word))]
    elif kind == "swap" and len(word) > 1:
        i = rng.randrange(len(word) - 1)
        word[i], word[i + 1] = word[i + 1], word[i]
    elif kind == "replace":
        word[rng.randrange(len(word))] = rng.choice(ALPHABET)
    elif kind == "unknown":
        word.insert(rng.randrange(len(word) + 1), "x")
    else:
        i = rng.randrange(len(word))
        word.insert(i, word[i])
    return word


def build_corpus(seed, count):
    from pm4py.objects.process_tree.utils.generic import parse
    rng = random.Random(seed)
    cases = []
    seen = set()
    while len(cases) < count:
        size = rng.choice([3, 4, 5, 6, 7, 8])
        labels = list(ALPHABET[:size]); rng.shuffle(labels)
        text = random_tree(rng, rng.choice([2, 3, 3, 4]), labels)
        if text in seen or "'" not in text:
            continue
        seen.add(text)
        tree = parse(text)
        words = []
        for _ in range(3):
            words.append(sample_language(tree, rng))
        for _ in range(5):
            words.append(noisy(rng.choice(words[:3]), rng))
        words.append([])
        cases.append(dict(name=f"tree-{len(cases):03d}", tree=text, traces=words))
    return cases


def tractable(net, im, fm, words):
    """Discovered nets can make the reference search explode (unbounded or
    huge state spaces). Such models would only yield timeouts, so the corpus
    keeps nets whose default alignments of the samples finish quickly."""
    from pm4py.algo.conformance.alignments.petri_net import algorithm as pn
    for word in words[:3] + [[]]:
        for variant in (pn.Variants.VERSION_STATE_EQUATION_A_STAR,
                        pn.Variants.VERSION_DIJKSTRA_LESS_MEMORY):
            outcome = record(lambda: pn.apply(as_log([word])[0], net, im, fm, variant=variant), 2)
            if "timeout" in outcome:
                return False
    return True


def build_net_corpus(seed, count):
    """Non-tree-shaped models, frozen to PNML text with neutral element ids.

    Alpha, heuristics and ILP nets are discovered from language samples of a
    random tree; duplicate-label trees are converted directly. Element ids are
    rewritten to p<i>/t<i> so every capture run reads byte-identical inputs.
    Each case also carries random cost functions for the cost-parameter
    families (keyed by transition id; trace costs per trace and position)."""
    import pm4py
    from pm4py.objects.process_tree.utils.generic import parse
    rng = random.Random(seed)
    kinds = ["alpha", "heuristics", "ilp", "duplicate"]
    cases = []
    seen = set()
    while len(cases) < count:
        kind = kinds[len(cases) % len(kinds)]
        size = rng.choice([3, 4, 5, 6])
        if kind == "duplicate":
            labels = [rng.choice(ALPHABET[:size]) for _ in range(size + 2)]
        else:
            labels = list(ALPHABET[:size]); rng.shuffle(labels)
        text = random_tree(rng, rng.choice([2, 3, 3]), labels)
        if (kind, text) in seen or text.count("'") < 4:
            continue
        seen.add((kind, text))
        tree = parse(text)
        sample = [sample_language(tree, rng) for _ in range(8)]
        with silenced():
            if kind == "duplicate":
                net, im, fm = pm4py.convert_to_petri_net(tree)
            else:
                discover = dict(alpha=pm4py.discover_petri_net_alpha,
                                heuristics=pm4py.discover_petri_net_heuristics,
                                ilp=pm4py.discover_petri_net_ilp)[kind]
                net, im, fm = discover(as_log([w for w in sample if w] or [["a"]]))
        if not fm or not im or not tractable(net, im, fm, sample):
            continue
        for i, place in enumerate(sorted(net.places, key=lambda p: p.name)):
            place.name = f"p{i}"
        for i, transition in enumerate(sorted(net.transitions,
                                              key=lambda t: (t.label is None, t.label or "", t.name))):
            transition.name = f"t{i}"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "model.pnml"
            pm4py.write_pnml(net, im, fm, str(path))
            pnml = path.read_text(encoding="utf-8")
        words = sample[:3] + [noisy(rng.choice(sample[:3]), rng) for _ in range(5)] + [[]]
        costs = dict(
            model={t.name: (rng.choice([0, 1, 2]) if t.label is None else rng.randint(1, 9))
                   for t in net.transitions},
            sync={t.name: rng.choice([0, 0, 1]) for t in net.transitions if t.label is not None},
            trace=[[rng.randint(1, 9) for _ in w] for w in words])
        cases.append(dict(name=f"{kind}-{len(cases):03d}", kind=kind,
                          tree=text if kind == "duplicate" else None,
                          model_pnml=pnml, traces=words, costs=costs))
    return cases


# ---------------------------------------------------------------- reference calls

def as_log(words):
    from pm4py.objects.log.obj import Event, EventLog, Trace
    return EventLog([Trace([Event({"concept:name": a}) for a in w],
                           attributes={"concept:name": str(i)})
                     for i, w in enumerate(words)])


def number(value):
    if isinstance(value, bool):
        return value
    if isinstance(value, float):
        if math.isnan(value):
            return "nan"
        if math.isinf(value):
            return "inf" if value > 0 else "-inf"
        return value
    if isinstance(value, int):
        return value
    try:
        return float(value)
    except (TypeError, ValueError):
        return str(value)


def move_side(value):
    # SEARCH_GRAPH_PT reports model sides as process-tree leaves.
    if value is None or isinstance(value, str):
        return value
    if getattr(value, "operator", "missing") is None and hasattr(value, "label"):
        return value.label
    raise TypeError(f"unexpected alignment element {type(value).__name__}")


def pairs(alignment):
    return [[move_side(a), move_side(b)] for a, b in alignment]


UUID = re.compile(r"[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}")


def alignment_row(result, sink=False):
    if result is None:
        return None
    row = {"alignment": pairs(result["alignment"])}
    if sink:  # generated DFG sink names are normalized to the native spelling
        row["alignment"] = [[a, "@@dfg_sink" if isinstance(b, str) and UUID.fullmatch(b) else b]
                            for a, b in row["alignment"]]
    for key in ("cost", "fitness", "bwc", "visited_states", "queued_states",
                "traversed_arcs", "lp_solved", "closed_set_length",
                "num_visited_markings"):
        if key in result:
            row[key] = number(result[key])
    return row


class CallTimeout(Exception):
    pass


def _alarm(signum, frame):
    raise CallTimeout()


def record(call, seconds=20):
    """One public call. Calls exceeding the wall-clock budget are recorded as
    timeouts and excluded from equivalence, never retried."""
    signal.signal(signal.SIGALRM, _alarm)
    signal.alarm(seconds)
    try:
        with silenced():
            return {"result": call()}
    except CallTimeout:
        return {"timeout": seconds}
    except Exception as error:  # observation, not a harness failure
        return {"error": type(error).__name__, "message": str(error)[:300]}
    finally:
        signal.alarm(0)


def serialize_models(tree_text, directory):
    import pm4py
    from pm4py.objects.process_tree.utils.generic import parse
    net, im, fm = pm4py.convert_to_petri_net(parse(tree_text))
    pnml = Path(directory) / "model.pnml"
    ptml = Path(directory) / "model.ptml"
    pm4py.write_pnml(net, im, fm, str(pnml))
    pm4py.write_ptml(parse(tree_text), str(ptml))
    return pnml.read_text(encoding="utf-8"), ptml.read_text(encoding="utf-8")


def permute_pnml(text, permutation, salt):
    """Shuffle place, transition and arc element order; the model is unchanged.

    Element order is not part of a net's meaning, so every order-induced
    difference in a reference result is an admissible alternative."""
    if not permutation:
        return text
    import xml.etree.ElementTree as ET
    root = ET.fromstring(text)
    rng = random.Random(f"{permutation}:{salt}")
    for page in root.iter("page"):
        children = list(page)
        rng.shuffle(children)
        for child in children:
            page.remove(child)
        for child in children:
            page.append(child)
    return ET.tostring(root, encoding="unicode")


def observe(case, families=None, permutation=0):
    import pm4py
    from pm4py.algo.conformance.alignments.petri_net import algorithm as pn
    from pm4py.algo.conformance.alignments.petri_net.variants import (
        generator_dijkstra_less_memory as gen_less, generator_dijkstra_no_heuristics as gen_no)
    from pm4py.algo.conformance.alignments.process_tree import algorithm as pt
    from pm4py.algo.conformance.alignments.dfg.variants import classic as dfg_classic
    from pm4py.algo.conformance.alignments.decomposed.variants import recompos_maximal
    from pm4py.algo.conformance.tokenreplay import algorithm as tbr
    from pm4py.objects.process_tree.utils.generic import parse
    from pm4py.objects.log.obj import EventLog

    with tempfile.TemporaryDirectory() as directory:
        if "model_pnml" in case:
            case["pnml"] = case["model_pnml"]
            case["ptml"] = None
        else:
            case["pnml"], case["ptml"] = serialize_models(case["tree"], directory)
        case["pnml"] = permute_pnml(case["pnml"], permutation, case["name"])
        (Path(directory) / "model.pnml").write_text(case["pnml"], encoding="utf-8")
        net, im, fm = pm4py.read_pnml(str(Path(directory) / "model.pnml"))
    log = as_log(case["traces"])
    out = {}

    def want(family):
        return not families or any(family.startswith(p) or p.startswith(family)
                                   for p in families)

    for variant in [v for v in pn.Variants if want("petri:" + v.name)]:
        out["petri:" + variant.name] = [
            record(lambda t=t: alignment_row(pn.apply(t, net, im, fm, variant=variant)))
            for t in log]
    for name, module in (("GENERATOR_DIJKSTRA_LESS_MEMORY", gen_less),
                         ("GENERATOR_DIJKSTRA_NO_HEURISTICS", gen_no)) if want("petri:GENERATOR") else ():
        rows = []
        for t in log:
            def run(t=t):
                # Equal-cost enumeration can be unbounded on silent cycles.
                return [alignment_row(r) for r in
                        itertools.islice(module.apply(t, net, im, fm), GENERATOR_LIMIT)]
            rows.append(record(run))
        out["petri:" + name] = rows

    if "costs" in case:
        by_name = {t.name: t for t in net.transitions}
        model_costs = {by_name[n]: c for n, c in case["costs"]["model"].items()}
        sync_costs = {by_name[n]: c for n, c in case["costs"]["sync"].items()}
        for variant in [v for v in pn.Variants if want("petricost:" + v.name)]:
            rows = []
            for t, trace_costs in zip(log, case["costs"]["trace"]):
                parameters = {"model_cost_function": model_costs,
                              "trace_cost_function": list(trace_costs)}
                if variant != pn.Variants.VERSION_DIJKSTRA_LESS_MEMORY:
                    parameters["sync_cost_function"] = sync_costs
                rows.append(record(lambda t=t, parameters=parameters: alignment_row(
                    pn.apply(t, net, im, fm, variant=variant, parameters=parameters))))
            out["petricost:" + variant.name] = rows
        for name, module in (("GENERATOR_DIJKSTRA_LESS_MEMORY", gen_less),
                             ("GENERATOR_DIJKSTRA_NO_HEURISTICS", gen_no)) if want("petricost:GENERATOR") else ():
            rows = []
            for t, trace_costs in zip(log, case["costs"]["trace"]):
                parameters = {"model_cost_function": model_costs,
                              "trace_cost_function": list(trace_costs)}
                if module is gen_no:
                    parameters["sync_cost_function"] = sync_costs
                def run(t=t, parameters=parameters):
                    return [alignment_row(r) for r in itertools.islice(
                        module.apply(t, net, im, fm, parameters=parameters), GENERATOR_LIMIT)]
                rows.append(record(run))
            out["petricost:" + name] = rows

    for variant in pt.Variants if want("tree:") and case["tree"] else ():
        def run_tree(variant=variant):
            results = pt.apply(log, parse(case["tree"]), variant=variant)
            return [alignment_row(r) for r in results]
        out["tree:" + variant.name] = record(run_tree)

    # DFG model: frequency DFG of the model-language part of the corpus.
    language = as_log(case["traces"][:3])
    dfg, starts, ends = pm4py.discover_dfg(language)
    case["dfg"] = dict(edges=sorted([[a, b, n] for (a, b), n in dfg.items()]),
                       starts=sorted([[a, n] for a, n in starts.items()]),
                       ends=sorted([[a, n] for a, n in ends.items()]))
    if want("dfg:"):
        out["dfg:CLASSIC"] = [
            record(lambda t=t: alignment_row(dfg_classic.apply(t, dfg, starts, ends), sink=True))
            for t in log]

    # Per-trace logs keep one non-terminating recomposition from hiding the
    # remaining observations; such calls are recorded as timeouts.
    if want("decomposed:"):
        out["decomposed:RECOMPOS_MAXIMAL"] = [
            record(lambda t=t: [alignment_row(r) for r in
                                recompos_maximal.apply(EventLog([t]), net, im, fm)], 5)
            for t in log]

    def replay(variant, log, parameters=None):
        rows = tbr.apply(log, net, im, fm, variant=variant, parameters=parameters)
        result = []
        for row in rows:
            # Variations differ in which keys they report; absent keys stay absent.
            item = {}
            converters = dict(
                trace_is_fit=lambda v: v, trace_fitness=number,
                activated_transitions=lambda v: [t.name for t in v],
                transitions_with_problems=lambda v: [t.name for t in v],
                reached_marking=lambda v: sorted([[p.name, n] for p, n in v.items()]),
                enabled_transitions_in_marking=lambda v: sorted(t.name for t in v),
                missing_tokens=lambda v: v, consumed_tokens=lambda v: v,
                remaining_tokens=lambda v: v, produced_tokens=lambda v: v)
            for key, convert in converters.items():
                if key in row:
                    item[key] = convert(row[key])
            extra = sorted(set(row) - set(converters))
            if extra:
                item["unrecorded_keys"] = extra
            result.append(item)
        return result
    if want("replay:"):
        for variant in (tbr.Variants.TOKEN_REPLAY, tbr.Variants.BACKWARDS):
            out["replay:" + variant.name] = record(lambda variant=variant: replay(variant, log))
        # Backwards fitness divides by zero on an empty trace, failing the
        # whole log; the nonempty traces are observed separately.
        nonempty = EventLog([t for t in log if len(t)])
        out["replay:BACKWARDS:nonempty"] = record(
            lambda: replay(tbr.Variants.BACKWARDS, nonempty))
        if "costs" in case:  # the net corpus also exercises replay parameters
            for option, value in REPLAY_OPTIONS:
                for variant in (tbr.Variants.TOKEN_REPLAY, tbr.Variants.BACKWARDS):
                    if variant == tbr.Variants.BACKWARDS and option not in BACKWARDS_OPTIONS:
                        continue
                    source = log if variant == tbr.Variants.TOKEN_REPLAY else nonempty
                    out[f"replay:{variant.name}:{option}={value}"] = record(
                        lambda variant=variant, source=source, option=option, value=value:
                        replay(variant, source, {option: value}))
    return out


def versions():
    import importlib.metadata as md
    found = {name: md.version("PuLP" if name == "pulp" else name) for name in EXPECTED_VERSIONS}
    if found != EXPECTED_VERSIONS:
        raise RuntimeError(f"pinned runtime mismatch: {found}")
    return found


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=2722)
    parser.add_argument("--count", type=int, default=60)
    parser.add_argument("--corpus", choices=["trees", "nets"], default="trees")
    parser.add_argument("--models", type=Path,
                        help="frozen corpus file: read it when present, else write the built corpus")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--permutation", type=int, default=0,
                        help="nonzero: shuffle PNML element order with this seed")
    parser.add_argument("--families", default="",
                        help="comma-separated family prefixes to keep (default: all)")
    args = parser.parse_args()
    warnings.filterwarnings("ignore")
    os.environ.setdefault("TQDM_DISABLE", "1")
    runtime = versions()
    if args.models and args.models.exists():
        cases = json.loads(gzip.decompress(args.models.read_bytes()))["cases"]
    else:
        cases = (build_corpus if args.corpus == "trees" else build_net_corpus)(args.seed, args.count)
        if args.models:
            # Discovery and conversion are not reproducible across processes
            # (generated ids, set order), so the first run freezes its inputs.
            frozen = [{k: case[k] for k in ("name", "kind", "tree", "model_pnml", "traces", "costs")
                       if k in case} for case in cases]
            data = json.dumps(dict(seed=args.seed, corpus=args.corpus, cases=frozen),
                              sort_keys=True, separators=(",", ":")).encode()
            with gzip.GzipFile(filename="", mode="wb", fileobj=args.models.open("wb"), mtime=0) as sink:
                sink.write(data)
    for index, case in enumerate(cases):
        case["observations"] = observe(
            case, tuple(args.families.split(",")) if args.families else None,
            args.permutation)
        print(f"[{index + 1}/{len(cases)}] {case['tree']}", file=sys.stderr, flush=True)
    payload = dict(reference_version="2.7.22", reference_commit=REFERENCE_COMMIT,
                   runtime=runtime, python=sys.version.split()[0],
                   hash_seed=os.environ.get("PYTHONHASHSEED"),
                   seed=args.seed, corpus=args.corpus, permutation=args.permutation, cases=cases)
    data = json.dumps(payload, sort_keys=True, separators=(",", ":"), default=lambda o: {"unserializable": type(o).__name__, "repr": str(o)}).encode()
    with gzip.GzipFile(filename="", mode="wb", fileobj=args.output.open("wb"), mtime=0) as sink:
        sink.write(data)
    print(f"{len(cases)} cases, sha256 {hashlib.sha256(data).hexdigest()}")


if __name__ == "__main__":
    main()
