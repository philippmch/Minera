# Supplied case description parity contract

The target is `algo.querying.llm.abstractions.case_to_descr`, pinned PM4Py
2.7.22 at `0715c31f3f1d85924107d6aa5329806db69df899`. Root captured the
finite public bank once; the native acceptance target subsequently passed
all retained observations and caller controls without replaying reference
calls. The typed value scope is specified below.

The existing native case branch in `src/backend/llm_context.cpp` is a log
projection. It selects `case_index`, synthesizes `concept:name` from the
trace ID, excludes first-class attribute aliases, converts timestamp lexical
forms, sorts extra event attributes and applies the shared character budget
and 2 MiB result cap. Its header-disabled path starts with one newline and
adds two after the case attributes. The retained original-case output noted
by the prior audit puts the leading separator before the attributes instead.
The existing public wrapper signature retained in
`docs/feature-first-llm-context-validation.json` takes an already supplied
`Trace`, seven explicit options, and no character budget. The generic branch
therefore cannot be assumed to implement the original case abstraction.

`build/parity-closures/case-description-surface.py` is prepared for a single
root execution into a fresh directory. It verifies opaque wrapper, leaf and
public Trace/Event module hashes against the pin; it captures only the named
public signatures, documentation and `Parameters` iteration. It neither
constructs fixtures nor calls the formatter. Failed or partial captures must
be retained. Reference source, AST, bytecode and private state are excluded.

Root executed the finite behavior bank once at
`build/parity-closures/case-description-public-1/capture.json`: all 32 calls
completed, 30 returned strings, two raised `KeyError`, and every supplied
input snapshot stayed unchanged. The calls cover all sixteen boolean option
combinations, empty cases, custom activity/timestamp keys, missing/null
selected keys, scalar/date/tuple/bytes values, plain list/dict values, XES
value/children wrapper dictionaries, a 12,001-character label and a returned
string above 2 MiB. Three leaf calls cover `parameters=None` and the same six
controls through string keys and public enum members. No successful call was
replayed.

Observed formatting preserves case attribute insertion order and sorts extra
event attributes by key. Custom selectors exclude only those exact keys;
canonical keys with other values remain visible. Missing selected activity
or enabled timestamp raises `KeyError`; a present null prints `None`.
Timestamp display uses the supplied value's string form. An event without
an enabled timestamp or extra attributes keeps its trailing space without
parentheses. Header-disabled output places three newlines before case
attributes. There is no public length option or byte cap.

The implementation adds `abstract_case` in `case_description.cpp`, using a
scoped ordered value carrier for the actual formatter inputs. This keeps
plain list/dict values distinct from XES value/children wrapper dictionaries.
Existing native number/date/bytes formatting is reused; recursive string
representation preserves nested Python literal formatting, Unicode and lone
surrogates. The existing generic log-case extension remains unchanged.

`case_description_workbench_json.cpp` accepts a `case_file` File input and
optional mutually exclusive `case_options_json` or `case_options_file`.
`variant=case` may be supplied. The options are the wrapper's four booleans,
`activity_key` and `timestamp_key`. Unknown/duplicate controls and malformed
carrier fields are rejected before the formatter. The response records
native entry/return, typed errors, original input bytes, before/after case
snapshots, options, complete returned text bytes and character count.

The case file uses the public receipt's typed-value schema: `attributes` is
a tagged `builtins.dict` with ordered `[key,value]` item pairs; `events` is an
array of the same tagged dictionaries. Supported value tags are
`builtins.NoneType`, `builtins.str`, `builtins.bool`, `builtins.int`,
`builtins.float` (binary64 hex), `datetime.datetime` (ISO value and fold),
`builtins.bytes` (hex), `builtins.list`, `builtins.tuple` and `builtins.dict`.
String attribute names remain literal. The carrier represents supplied
values, without arbitrary Python-class constructors or projected aliases.

`src/backend/tests/case_description_public_vectors.inc` is generated offline
from all 32 retained rows by `generate-case-description-tests.py`. Each
returned result asserts every byte, Python character count, no truncation,
and input preservation. Error rows assert the selected key and exact public
message. The same vectors exercise the supplied-case adapter and carrier
round trips. Separate controls cover defaults, File presence, repeated
fields, contradictory inputs, unknown options, wrong option types and
transport failures. A direct `abstract_case` assertion and a File-options
adapter assertion also run in `configuration()`, which `main()` invokes.

Native verification used root's focused registered build directory:

```
cmake --build build/parity-closures/native-current/out --config Release --target minera_case_description_tests --parallel 2
ctest --test-dir build/parity-closures/native-current/out -C Release -R '^minera_case_description_tests$' --output-on-failure
```

The target compiled and the assertion-enabled test passed 1/1 in 1.29 seconds.
This includes every returned byte and error from all 32 retained rows and
the complete 2,097,176-byte result. Root owns CMake, HTTP dispatch, Git and
the shared coverage ledger. The compressed public evidence is
`docs/case-description-public-evidence.json.gz`; the original capture SHA256
is `4f933e99866bbd146b55d67573f17532038a42711a11bf0e1393cf2d602c06d2`.
