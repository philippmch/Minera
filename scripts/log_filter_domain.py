"""Seeded public-call domain for the EventLog filtering modules.

Generates small XES documents and, per document, calls into the public
`pm4py.algo.filtering.log.*` modules (attributes, between, cases, ltl, paths,
timestamp, variants). Every event carries a unique `rd:eid` string and every
trace a unique `concept:name`, so a returned log is summarized as the trace
names and event ids it holds, which both sides can report without a value
printer. Arguments and scalar results travel in one typed JSON form:

  {"s": text} {"i": "decimal"} {"f": "hex of the IEEE double"} {"b": bool}
  {"n": null} {"d": "YYYY-MM-DD HH:MM:SS[.ffffff]+00:00"} (UTC datetime)
  {"l": [...]} {"t": [...]} {"m": [[key, value], ...]} (list, tuple, dict)

Parameters travel as [[enum member name, typed value], ...] and are passed as
the module's `Parameters` members.
"""
import copy
import datetime
import random
import struct

LABELS = ("A", "B", "C", "D", "E")
RESOURCES = ("r1", "r2", "r3")
KINDS = ("x", "y", "z")
BASE = datetime.datetime(2021, 3, 1, 8, 0, 0)


# ---------------------------------------------------------------- typed JSON

def fbits(value):
    if value != value:
        value = float("nan")
        return "7ff8000000000000"
    return struct.pack(">d", value).hex()


def encode(value, traces=None):
    """Typed JSON of a Python value (raises TypeError when not representable).
    With traces (id -> index), a list of the log's traces becomes {"x": [index, ...]}."""
    if traces is not None and isinstance(value, list) and value and all(id(v) in traces for v in value):
        return {"x": [traces[id(v)] for v in value]}
    if value is None:
        return {"n": None}
    if isinstance(value, bool):
        return {"b": value}
    if isinstance(value, int):
        return {"i": str(value)}
    if isinstance(value, float):
        return {"f": fbits(value)}
    if isinstance(value, str):
        return {"s": value}
    if isinstance(value, datetime.datetime):
        if value.tzinfo is None:
            raise TypeError("naive datetime result")
        return {"d": value.astimezone(datetime.timezone.utc).isoformat(sep=" ")}
    if isinstance(value, list):
        return {"l": [encode(v, traces) for v in value]}
    if isinstance(value, tuple):
        return {"t": [encode(v, traces) for v in value]}
    if isinstance(value, dict):
        return {"m": [[encode(k, traces), encode(v, traces)] for k, v in value.items()]}
    raise TypeError(f"unrepresentable {type(value).__name__}")


def decode(node, log=None):
    (tag, value), = node.items()
    if tag == "x":
        return [log[i] for i in value]
    if tag == "n":
        return None
    if tag == "b":
        return value
    if tag == "i":
        return int(value)
    if tag == "f":
        return struct.unpack(">d", bytes.fromhex(value))[0]
    if tag == "s":
        return value
    if tag == "d":
        return datetime.datetime.fromisoformat(value)
    if tag == "l":
        return [decode(v, log) for v in value]
    if tag == "t":
        return tuple(decode(v, log) for v in value)
    if tag == "m":
        return {decode(k, log): decode(v, log) for k, v in value}
    raise ValueError(tag)


def S(v): return {"s": v}
def I(v): return {"i": str(v)}
def F(v): return {"f": fbits(v)}
def B(v): return {"b": v}
def N(): return {"n": None}
def L(*v): return {"l": list(v)}
def T(*v): return {"t": list(v)}
def D(dt): return {"d": dt.replace(tzinfo=datetime.timezone.utc).isoformat(sep=" ")}


# ---------------------------------------------------------------- documents

def xml_attr(text):
    from xml.sax.saxutils import quoteattr
    return quoteattr(text)


def stamp(dt, rng):
    roll = rng.random()
    text = dt.strftime("%Y-%m-%dT%H:%M:%S")
    if roll < 0.15:
        return text + f".{dt.microsecond:06d}"
    if roll < 0.3:
        return text + ".000+01:00"
    if roll < 0.4:
        return text + "Z"
    return text + ".000"


def generate_cases(count, bank_seed):
    rng = random.Random(bank_seed)
    return [{"traces": rng.randint(0, 7), "seed": rng.randrange(1 << 30), "typed": rng.random() < 0.4}
            for _ in range(count)]


def build_xes(spec):
    """One XES document. Timestamps mostly increase within a trace."""
    rng = random.Random(spec["seed"])
    labels = LABELS[:rng.randint(2, 5)]
    p_missing_ts = rng.choice((0.0,) * 5 + (0.05,))
    p_missing_label = rng.choice((0.0,) * 7 + (0.05,))
    lines = ['<?xml version="1.0" encoding="UTF-8"?>', '<log xes.version="1.0">']
    for t in range(spec["traces"]):
        start = BASE + datetime.timedelta(minutes=rng.randrange(0, 60 * 24 * 6))
        parts = [f'<string key="concept:name" value="rd-t{t}"/>']
        if rng.random() < 0.6:
            parts.append(f'<date key="rd:start" value="{stamp(start + datetime.timedelta(hours=rng.randint(-24, 24)), rng)}"/>')
        if rng.random() < 0.6:
            parts.append(f'<int key="rd:prio" value="{rng.randint(0, 3)}"/>')
        if rng.random() < 0.5:
            parts.append(f'<string key="rd:segment" value="{rng.choice(KINDS)}"/>')
        lines.append(" <trace>" + "".join(parts))
        now = start
        size = rng.choice((0, 1, 2, 3, 3, 4, 4, 5, 6, 8))
        for e in range(size):
            step = rng.choice((0, 0, 1, 5, 30, 60, 240, 1440))
            if rng.random() < 0.08:
                step = -rng.choice((1, 30, 600))
            now = now + datetime.timedelta(minutes=step, microseconds=rng.choice((0, 0, 0, 250000, 1)))
            ev = [f'<string key="rd:eid" value="t{t}e{e}"/>']
            if rng.random() >= p_missing_label:
                ev.append(f'<string key="concept:name" value="{rng.choice(labels)}"/>')
            if rng.random() >= p_missing_ts:
                ev.append(f'<date key="time:timestamp" value="{stamp(now, rng)}"/>')
            if rng.random() < 0.97:
                ev.append(f'<string key="org:resource" value="{rng.choice(RESOURCES)}"/>')
            roll = rng.random()
            if roll < 0.45:
                ev.append(f'<int key="rd:amount" value="{rng.randint(-5, 120)}"/>')
            elif roll < 0.75:
                ev.append(f'<float key="rd:amount" value="{rng.choice((0.5, 12.25, 99.9, 100.0, -3.5, 1e3))}"/>')
            if rng.random() < 0.7:
                ev.append(f'<string key="rd:kind" value="{rng.choice(KINDS)}"/>')
            if rng.random() < 0.3:
                ev.append(f'<boolean key="rd:flag" value="{rng.choice(("true", "false"))}"/>')
            if rng.random() < 0.6:
                ev.append(f'<date key="rd:due" value="{stamp(now + datetime.timedelta(hours=rng.randint(-5, 30)), rng)}"/>')
            lines.append("  <event>" + "".join(ev) + "</event>")
        lines.append(" </trace>")
    lines.append("</log>")
    return "\n".join(lines) + "\n"


def import_log(xes):
    import os
    import tempfile
    from pm4py.objects.log.importer.xes import importer as xes_importer
    handle, path = tempfile.mkstemp(suffix=".xes")
    try:
        with os.fdopen(handle, "w", encoding="utf-8") as out:
            out.write(xes)
        return xes_importer.apply(path, parameters={"show_progress_bar": False})
    finally:
        os.unlink(path)


# ---------------------------------------------------------------- calls

def present(log, key):
    return [e[key] for t in log for e in t if key in e]


def labels_of(log):
    seen = present(log, "concept:name")
    return sorted(set(seen)) or ["A"]


def times_of(log):
    return sorted(present(log, "time:timestamp")) or [BASE.replace(tzinfo=datetime.timezone.utc)]


def dt_arg(rng, log):
    """A dt1/dt2 argument: mostly the public '%Y-%m-%d %H:%M:%S' text, sometimes a
    datetime, sometimes a malformed value."""
    times = times_of(log)
    base = rng.choice(times)
    base = base + datetime.timedelta(minutes=rng.choice((0, 0, -1, 1, -90, 90, -3000, 3000)))
    roll = rng.random()
    if roll < 0.7:
        return S(base.strftime("%Y-%m-%d %H:%M:%S"))
    if roll < 0.93:
        return D(base.replace(tzinfo=None, microsecond=rng.choice((0, base.microsecond))))
    return rng.choice((S("2021-03-01"), S("bad"), I(5), N()))


def number(rng, choices):
    v = rng.choice(choices)
    return I(v) if isinstance(v, int) else F(v)


def variant_lists(rng, log, key="concept:name"):
    seen = []
    for t in log:
        v = [e.get(key) for e in t]
        if all(isinstance(x, str) for x in v) and v not in seen:
            seen.append(v)
    out = []
    for v in rng.sample(seen, min(len(seen), rng.randint(0, 3))):
        out.append(L(*[S(x) for x in v]))
    if rng.random() < 0.3:
        out.append(L(*[S(rng.choice(LABELS)) for _ in range(rng.randint(0, 3))]))
    return out


def param(rng, name, value, p=0.5):
    return [[name, value]] if rng.random() < p else []


def calls_for(rng, log):
    labels = labels_of(log)
    act = lambda: S(rng.choice(labels + ["Z"] if rng.random() < 0.15 else labels))
    calls = []

    def add(module, function, args, params=None):
        calls.append({"module": module, "function": function, "args": args, "params": params or []})

    # cases
    m = "cases"
    add(m, "filter_on_case_size", [number(rng, (0, 1, 2, 3, 4, 2.5))]
        + ([rng.choice((number(rng, (2, 3, 5, 1.5)), N()))] if rng.random() < 0.7 else []))
    add(m, "filter_on_ncases", [number(rng, (0, 1, 2, 3, 1000, -1, -2))])
    for fn in ("filter_on_case_performance", "filter_case_performance"):
        p = param(rng, "TIMESTAMP_KEY", S(rng.choice(("time:timestamp", "rd:due"))), 0.15)
        add(m, fn, [number(rng, (0, 0, 60, 300.5, 3600)), number(rng, (0, 60, 3600, 86400, 1e9))], p)
    for trace_index in range(len(log)):
        if rng.random() < 0.3:
            add(m, "satisfy_perf", [I(trace_index), number(rng, (0, 60, 600)), number(rng, (60, 3600, 1e9)),
                                    S(rng.choice(("time:timestamp",) * 5 + ("rd:due",)))])

    for module in ("cases", "timestamp"):
        if rng.random() < 0.1:
            add(module, rng.choice(("apply", "apply_auto_filter")), [], rng.choice(([], [["TIMESTAMP_KEY", S("x")]])))

    # paths
    m = "paths"
    pairs = [(rng.choice(labels), rng.choice(labels)) for _ in range(rng.randint(0, 3))]
    p = param(rng, "POSITIVE", B(rng.random() < 0.5)) + param(rng, "ATTRIBUTE_KEY", S(rng.choice(("concept:name", "org:resource"))), 0.2)
    add(m, "apply", [L(*[T(S(a), S(b)) for a, b in pairs])], p)
    p = (param(rng, "POSITIVE", B(rng.random() < 0.5)) + param(rng, "MIN_PERFORMANCE", number(rng, (0, 30, 60, 3600)))
         + param(rng, "MAX_PERFORMANCE", number(rng, (0, 60, 3600, 86400.5))) +
         param(rng, "TIMESTAMP_KEY", S("rd:due"), 0.1))
    add(m, "apply_performance", [T(act(), act())], p)
    key = rng.choice(("concept:name", "concept:name", "org:resource"))
    add(m, "get_paths_from_log", [S(key)] if rng.random() < 0.6 else [])
    add(m, "get_sorted_paths_list", [{"call": "get_paths_from_log"}])
    add(m, "get_paths_threshold", [{"call": "get_sorted_paths_list"}, number(rng, (0.6, 0.5, 0.0, 1.0))])
    add(m, "filter_log_by_paths", [{"call": "get_paths_from_log"}, {"call": "variants"}, {"call": "vc"},
                                   number(rng, (0, 1, 2, 3)), S("concept:name")])

    # variants
    m = "variants"
    vkey = lambda: param(rng, "ACTIVITY_KEY", S(rng.choice(("concept:name", "org:resource", "rd:kind"))), 0.15)
    p = param(rng, "POSITIVE", B(rng.random() < 0.5)) + vkey()
    add(m, "apply", [L(*variant_lists(rng, log))], p)
    add(m, "filter_variants_top_k", [number(rng, (0, 1, 2, 3, 10, -1))], vkey())
    add(m, "filter_variants_by_coverage_percentage", [number(rng, (0.0, 0.1, 0.2, 0.5, 1.0))], vkey())
    add(m, "filter_variants_by_maximum_coverage_percentage", [number(rng, (0.0, 0.1, 0.25, 0.5, 1.0))], vkey())
    add(m, "filter_log_variants_percentage", [number(rng, (0.0, 0.3, 0.5, 0.8, 1.0))] if rng.random() < 0.8 else [], vkey())
    add(m, "filter_variants_variants_percentage", [{"call": "variants"}, number(rng, (0.0, 0.2, 0.5, 1.0))])
    add(m, "find_auto_threshold", [{"call": "variants"}, number(rng, (0.6, 0.5, 0.0, 1.0))])

    # timestamp
    m = "timestamp"
    key_p = lambda: param(rng, "TIMESTAMP_KEY", S(rng.choice(("time:timestamp", "rd:due"))), 0.1)
    for fn in ("filter_traces_contained", "filter_traces_intersecting", "apply_events",
               "filter_traces_starting_in_timeframe", "filter_traces_completing_in_timeframe"):
        add(m, fn, [dt_arg(rng, log), dt_arg(rng, log)], key_p())
    add(m, "filter_on_trace_attribute", [dt_arg(rng, log), dt_arg(rng, log)],
        param(rng, "TIMESTAMP_KEY", S(rng.choice(("rd:start", "rd:start", "rd:prio", "rd:none"))), 0.9))
    add(m, "filter_traces_attribute_in_timeframe",
        [S(rng.choice(("concept:name", "org:resource", "rd:kind"))),
         S(rng.choice(labels + list(RESOURCES) + list(KINDS))), dt_arg(rng, log), dt_arg(rng, log)], key_p())

    # between
    m = "between"
    one = lambda: L(*[act() for _ in range(rng.randint(1, 2))]) if rng.random() < 0.25 else act()
    p = (param(rng, "SUBCASE_CONCAT_STR", S(rng.choice(("##@@", "-", ""))), 0.2)
         + param(rng, "ACTIVITY_KEY", S(rng.choice(("concept:name", "org:resource"))), 0.15)
         + param(rng, "CASE_ID_KEY", S(rng.choice(("concept:name", "rd:segment", "rd:prio"))), 0.15))
    add(m, "apply", [one(), one()], p)

    # attributes
    m = "attributes"
    akey = rng.choice(("concept:name", "concept:name", "org:resource", "rd:kind"))
    pool = {"concept:name": labels + ["Z"], "org:resource": list(RESOURCES), "rd:kind": list(KINDS)}[akey]
    values = L(*[S(v) for v in rng.sample(pool, rng.randint(0, min(3, len(pool))))])
    for fn in ("apply", "apply_events"):
        p = param(rng, "ATTRIBUTE_KEY", S(akey), 0.8 if akey != "concept:name" else 0.3) + param(rng, "POSITIVE", B(rng.random() < 0.5))
        add(m, fn, [values], p)
    tkey = rng.choice(("rd:segment", "concept:name", "rd:prio"))
    tvals = {"rd:segment": [S(k) for k in KINDS], "concept:name": [S(f"rd-t{i}") for i in range(4)],
             "rd:prio": [I(i) for i in range(4)]}[tkey]
    add(m, "apply_trace_attribute", [L(*rng.sample(tvals, rng.randint(0, 2)))],
        [["ATTRIBUTE_KEY", S(tkey)]] + param(rng, "POSITIVE", B(rng.random() < 0.5)))
    for fn in ("apply_numeric", "apply_numeric_events"):
        p = [["ATTRIBUTE_KEY", S(rng.choice(("rd:amount", "rd:amount", "rd:prio")))]] + param(rng, "POSITIVE", B(rng.random() < 0.5))
        if fn == "apply_numeric" and rng.random() < 0.3:
            p += [["STREAM_FILTER_KEY1", S("concept:name")], ["STREAM_FILTER_VALUE1", act()]]
            if rng.random() < 0.4:
                p += [["STREAM_FILTER_KEY2", S("org:resource")], ["STREAM_FILTER_VALUE2", S(rng.choice(RESOURCES))]]
        lo = number(rng, (-10, 0, 0.5, 10, 50))
        hi = number(rng, (0, 10, 50, 100, 100.0, 1e4))
        add(m, fn, [lo, hi], p)
    add(m, "filter_log_on_max_no_activities", [number(rng, (0, 1, 2, 3, 25, -1))] if rng.random() < 0.8 else [],
        param(rng, "ACTIVITY_KEY", S(rng.choice(("concept:name", "org:resource"))), 0.3)
        + param(rng, "ATTRIBUTE_KEY", S(rng.choice(("concept:name", "org:resource"))), 0.2)
        + param(rng, "KEEP_ONCE_PER_CASE", B(rng.random() < 0.5), 0.2))
    add(m, "filter_log_relative_occurrence_event_attribute", [number(rng, (0.0, 0.1, 0.3, 0.5, 0.9))],
        param(rng, "ATTRIBUTE_KEY", S(rng.choice(("concept:name", "org:resource"))), 0.3)
        + param(rng, "KEEP_ONCE_PER_CASE", B(rng.random() < 0.5), 0.5))
    add(m, "filter_log_by_attributes_threshold", [{"call": "attributes_count"}, {"call": "variants"}, {"call": "vc"},
                                                  number(rng, (0, 1, 2, 3)), S("concept:name")])

    # ltl
    m = "ltl"
    flow = [act() for _ in range(rng.randint(2, 3))]
    p = param(rng, "POSITIVE", B(rng.random() < 0.5))
    if rng.random() < 0.3:
        size = max(0, len(flow) - 1 + rng.choice((0, 0, 0, -1, 1)))
        bounds = [T(number(rng, (0, 0, 60)), number(rng, (60, 3600, 1e6))) for _ in range(size)]
        p += [["TIMESTAMP_DIFF_BOUNDARIES", L(*bounds)]]
    if rng.random() < 0.3:
        p += [["ENABLE_TIMESTAMP", B(rng.random() < 0.8)]]
    if rng.random() < 0.1:
        p += [["TIMESTAMP_KEY", S("rd:due")]]
    add(m, "eventually_follows", [L(*flow)], p + param(rng, "ATTRIBUTE_KEY", S("org:resource"), 0.1))
    add(m, "A_next_B_next_C", [act(), act(), act()], param(rng, "POSITIVE", B(rng.random() < 0.5))
        + param(rng, "ATTRIBUTE_KEY", S("org:resource"), 0.1))
    add(m, "four_eyes_principle", [act(), act()], param(rng, "POSITIVE", B(rng.random() < 0.5))
        + param(rng, "RESOURCE_KEY", S(rng.choice(("org:resource", "rd:kind"))), 0.2))
    add(m, "attr_value_different_persons", [act()], param(rng, "POSITIVE", B(rng.random() < 0.5))
        + param(rng, "RESOURCE_KEY", S(rng.choice(("org:resource", "rd:kind"))), 0.2))
    a = [number(rng, (0, 30, 100)) for _ in range(rng.randint(0, 2))]
    b = [T(number(rng, (0, 50)), number(rng, (60, 100))) for _ in range(len(a))]
    add(m, "timestamp_list_is_ge", [L(*a), L(*b)])
    add(m, "timestamp_list_is_le", [L(*a), L(*b)])
    return calls


def wild(rng, log):
    """A typed value drawn from the log's own attribute values or from edge cases."""
    roll = rng.random()
    if roll < 0.5:
        pool = [v for t in log for e in t for k, v in e.items() if k != "rd:eid"]
        pool += [v for t in log for v in t.attributes.values()]
        if pool:
            return encode(rng.choice(pool))
    return rng.choice((N(), S(""), S("A"), I(0), I(1), I(-1), I(2 ** 63), F(0.5), F(float("nan")),
                       F(float("inf")), F(-float("inf")), B(True), B(False), L(), T(),
                       L(S("A"), I(1)), D(BASE), D(BASE + datetime.timedelta(days=2))))


def wild_key(rng, log):
    keys = sorted({k for t in log for e in t for k in e} | {k for t in log for k in t.attributes})
    return S(rng.choice(keys + ["rd:none", ""])) if rng.random() < 0.9 else wild(rng, log)


KEY_PARAMS = {"ATTRIBUTE_KEY", "ACTIVITY_KEY", "TIMESTAMP_KEY", "RESOURCE_KEY", "CASE_ID_KEY",
              "STREAM_FILTER_KEY1", "STREAM_FILTER_KEY2"}


def roughen(rng, log, call):
    """Typed cases: replace some plain arguments and parameter values."""
    args = []
    for arg in call["args"]:
        if "call" not in arg and rng.random() < 0.2:
            arg = wild(rng, log)
        args.append(arg)
    params = []
    for name, value in call["params"]:
        if rng.random() < 0.3:
            value = wild_key(rng, log) if name in KEY_PARAMS else wild(rng, log)
        params.append([name, value])
    if rng.random() < 0.15:
        name = rng.choice(sorted(KEY_PARAMS & set(PARAMETER_NAMES[call["module"]])))
        params.append([name, wild_key(rng, log)])
    if call["function"] == "satisfy_perf":
        args[0] = call["args"][0]
    return dict(call, args=args, params=params)


PARAMETER_NAMES = {
    "attributes": ("ATTRIBUTE_KEY", "ACTIVITY_KEY", "CASE_ID_KEY", "STREAM_FILTER_KEY1", "STREAM_FILTER_KEY2"),
    "between": ("ACTIVITY_KEY", "CASE_ID_KEY"),
    "cases": ("TIMESTAMP_KEY",),
    "ltl": ("ATTRIBUTE_KEY", "TIMESTAMP_KEY", "RESOURCE_KEY"),
    "paths": ("ATTRIBUTE_KEY", "TIMESTAMP_KEY"),
    "timestamp": ("TIMESTAMP_KEY",),
    "variants": ("ACTIVITY_KEY",),
}


def build_case(spec):
    rng = random.Random(spec["seed"] ^ 0x51F7)
    xes = build_xes(spec)
    log = import_log(xes)
    traces = {id(t): i for i, t in enumerate(log)}
    calls = []
    generated = calls_for(rng, log)
    if spec.get("typed"):
        generated = [roughen(rng, log, call) for call in generated]
    for call in generated:
        try:
            call["args"] = [encode(helper_value(a["call"], log), traces) if "call" in a else a
                            for a in call["args"]]
        except Exception:  # noqa: BLE001 - the helper belongs to another module
            continue
        calls.append(call)
    return xes, calls


# ---------------------------------------------------------------- outcomes

def log_summary(log):
    return [[encode(t.attributes.get("concept:name")), [e.get("rd:eid") for e in t]] for t in log]


MODULES = {
    "attributes": "pm4py.algo.filtering.log.attributes.attributes_filter",
    "between": "pm4py.algo.filtering.log.between.between_filter",
    "cases": "pm4py.algo.filtering.log.cases.case_filter",
    "ltl": "pm4py.algo.filtering.log.ltl.ltl_checker",
    "paths": "pm4py.algo.filtering.log.paths.paths_filter",
    "timestamp": "pm4py.algo.filtering.log.timestamp.timestamp_filter",
    "variants": "pm4py.algo.filtering.log.variants.variants_filter",
}


def helper_value(name, log):
    """Public helper results that some calls take as arguments."""
    import importlib
    if name == "variants":
        from pm4py.statistics.variants.log import get as variants_get
        return variants_get.get_variants(log)
    if name == "vc":
        from pm4py.statistics.variants.log import get as variants_get
        return variants_get.get_variants_sorted_by_count(variants_get.get_variants(log))
    if name == "attributes_count":
        from pm4py.statistics.attributes.log import get as attributes_get
        return attributes_get.get_attribute_values(log, "concept:name")
    paths = importlib.import_module(MODULES["paths"])
    if name == "get_paths_from_log":
        return paths.get_paths_from_log(log)
    if name == "get_sorted_paths_list":
        return paths.get_sorted_paths_list(paths.get_paths_from_log(log))
    raise ValueError(name)


def run_call(call, log):
    """Public outcome of one call on a private copy of the log."""
    import importlib
    module = importlib.import_module(MODULES[call["module"]])
    log = copy.deepcopy(log)
    before = log_summary(log)
    args = [decode(arg, log) for arg in call["args"]]
    if call["function"] == "satisfy_perf":
        args[0] = log[args[0]]
    params = None
    if call["params"]:
        enum = module.Parameters
        params = {enum[name]: decode(value, log) for name, value in call["params"]}
    fn = getattr(module, call["function"])
    signature = __import__("inspect").signature(fn).parameters
    takes_parameters = "parameters" in signature
    if next(iter(signature)) in ("log", "df"):
        args.insert(0, log)
    try:
        if takes_parameters:
            result = fn(*args, parameters=params)
        else:
            result = fn(*args)
    except Exception as error:  # noqa: BLE001 - every public error is an outcome
        outcome = {"error": type(error).__name__, "message": str(error)}
    else:
        from pm4py.objects.log.obj import EventLog
        if isinstance(result, EventLog):
            outcome = {"log": log_summary(result)}
        else:
            try:
                outcome = {"value": encode(result)}
            except TypeError as error:
                outcome = {"unrepresentable": str(error)}
    after = log_summary(log)
    if after != before:
        outcome["source_after"] = after
    return outcome
