# BOSE semantic parity contract

Semantic requirements verified against commit `c223660ac33da47a919eb7095e29bd645ba6b856`.
The completion audit and independent scope review are bundled with this contract.

Target: PM4Py 2.7.22, commit `0715c31f3f1d85924107d6aa5329806db69df899`.
Scope: `algo.concept_drift` and `algo.concept_drift.variants.bose`, under
the existing semantic acceptance criteria in `docs/reference-2.7.22-parity.md`.
Completion of these two rows does not establish project-wide parity.

## Required behavior and evidence

| Requirement | Native behavior | Evidence |
| --- | --- | --- |
| Unique activities | Typed equality, first representatives, comparable sorting, numeric aliases, calendar-date distinction, tuples, NA, frozen sets and shared/distinct NaNs | Typed-helper, NativeNA and frozenset corpora; `bose_module_contract`; `test_bose_typed_helpers`, `test_bose_na`, `test_bose_frozenset` |
| Global features | Eventual-follow relation counts in explicit alphabet order, duplicates/extras, omitted-label errors, mixed typed identities and ordered NA membership | Feature/alphabet, typed-helper and NA membership corpora; `bose_module_contract` |
| Follows relation | Alphabet filters targets only, sources unrestricted, unique semantic pairs retain original typed representatives; mutable alphabet entries use ordered equality while trace activities remain hashable | Typed-helper and 110-call mutable-alphabet corpus; `test_bose_follows_alphabet`; `bose_module_contract` |
| Split log into sublogs | Consecutive groups, optional remainder, signed sizes, empty input, original trace/event metadata | Standalone helper corpus and split assertions in `bose_module_contract` |
| Permutation test | Rectangular double feature windows, NumPy-compatible stateful shuffle, numerical reductions, negative/zero count behavior, RNG continuation | 757 permutation cases and numerical reduction probes; `bose_module_contract` |
| Raw detection | String and typed traces, signed group/window/permutation/cap parameters, threshold, ranked change points, original grouped values and RNG continuation | Direct/typed-direct, NA and frozenset corpora; `bose_module_contract` |
| EventLog apply | Activity and first-timestamp selectors, existing trace order, regenerated output frames and boundary timestamps, diagnostic/RNG timing | Signed and typed EventLog, NativeNA, frozen-set and timestamp-resolution corpora; `bose_module_contract` |
| DataFrame apply | Native EventTable representation, case projection, selected case/time grouping, categorical and nanosecond keys, regenerated frame schema, exact probabilities and rounded boundary timestamps | Table, ordering, activity-type, NativeNA and timestamp-resolution corpora; `bose_module_contract` |
| BOSE dispatch | Default and explicit BOSE, EventLog/EventTable, stateful/seeded routes, invalid selector rejects before RNG | Public dispatch corpus; typed EventLog probe also invokes default and explicit dispatch plus direct adapter; `bose_dispatch_contract` |

All eight documented apply parameters are exposed: sublog size, window size,
permutation count, threshold, maximum change-point count, and activity,
timestamp and case selectors. Raw detection exposes the five numerical
parameters. Helper alphabets and split remainder handling are explicit inputs.
The public-surface inventory records all seven BOSE callables, their signatures,
and the parameter enum without inspecting implementation bodies.

The named module and dispatcher cases are built by the registered
`minera_bose_parity_contract_tests` target. The broader
`minera_concept_drift_tests` includes typed labels, NA, frozensets, mutable
alphabets and timestamp controls. Named proof cases complement, rather than
replace, the full captured behavioral evidence.

## Representation and output distinctions

Native EventLog and EventTable implement the advertised EventLog/DataFrame
input forms. EventStream is absent from both public reference signatures and
is not converted by the native dispatcher. Captured nonempty EventStreams
reject and empty streams yield empty results. Explicit Python `variant=None`
is invalid, whereas omitting the selector selects BOSE; the native closed
enum rejects invalid selections before adapter work and RNG consumption.
`parameters=None` corresponds to default native options.

Typed activities include native strings, bytes, booleans, signed/unsigned
integers, binary64 numbers, null/None, a distinct NativeNA singleton, calendar
dates/datetimes, immutable tuples and frozen sets. NativeNA is not null or NaN.
Mutable lists, sets and string-keyed dictionary alphabet entries are supported
by standalone follows; mutable trace labels and feature alphabets reject.
Python object ownership and symbol spellings are not the semantic target;
diagnostics preserve rejection meaning and observable RNG timing rather than
Python exception class/message identity. Arbitrary user-defined Python object
methods are not native activity representations.

One distinct-NaN tuple unique-list order differs while preserving typed members;
32 repeated public observations produced all six permutations of that input.
Twelve incomparable-frozenset unique-list orders also differ. Native frozen-set
ordering compares unique member counts then canonical identities, keeping every
strict subset before its superset. Equal tuple prefixes advance to later fields.
These 13 differences remain visible and are not counted as exact matches.
The frozenset checkpoint independently reviewed this ordering against the
existing multiple-valid-output criterion, verified same members and strict
comparisons, and checked raw detection and RNG invariance under 32 paired
label recodings. Features/follows and resulting drift behavior match captured
reference outputs. No universal Python hash-iteration order is claimed.

Calendar dates remain distinct from datetimes at midnight; mixed date/datetime
sorting rejects. Boundary timestamps reproduce binary64 epoch conversion and
six-decimal rounding, including signed zero; activity/case precision is retained.
Explicit resource bounds and seeds wider than 32 bits are native extensions.
Numerical evidence targets the recorded float64 NumPy 2.4.6, pandas 3.0.6,
Python 3.11.9 Windows x86-64 runtime, not every external BLAS or array dtype.
The stricter diagnostic `detect_concept_drift(EventLog, ConceptDriftOptions)`
is a native extension; signed apply/raw APIs carry the reference-facing behavior.

## Current validation and completion gate

The committed mutable-alphabet checkpoint records 20,899 comparisons,
20,886 exact matches and the 13 documented order differences, with no new
skipped cases. It includes before/after evidence on identical frozen reference
bytes, two passing native suites, 14 CLI checks and independent code review.
Its core SHA-256 is
`6114330fa513547f359b2e3ce1488b547e08052c739e95c8da5672df52e41e88`.
Earlier NativeNA integration also passed 14 affected native suites.

Authoritative evidence is the committed `docs/bose-*-reference.json.gz`,
`docs/bose-*-native-checks.json.gz` and matching validation manifests, especially
`docs/bose-follows-alphabet-validation.json`, whose bundle replays the prior
corpora against the current core. Captures include source/runtime provenance;
native reports include exact wire inputs, checker/probe sources and hashes.

The requirement-by-requirement completion audit and independent scope review
verify this contract. Exact module/selector proof records and catalogue results
are recorded in `docs/bose-parity-validation.json`. A structural manifest pass alone is not a
semantic completion proof. The project-wide strict gate must remain failing
while other incomplete coverage units remain.
