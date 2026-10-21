# Polars LTL filtering: public observation and native companion

The canonical `pm4py.algo.filtering.polars.ltl.ltl_checker` module remains
**partial**. Root executed one frozen bank of 675 distinct public calls: 516
complete returned frames and 159 errors. The native companion passed its first
coordinated execution of all 675 outcomes and native controls. Final acceptance
also passed all 47 native suites, 73 LTL production HTTP checks, 101 attributes
regression HTTP checks and 9,829 frontend tests across 192 files. TypeScript and
Vite passed. The module remains partial within the boundaries below.

The pinned public surface is PM4Py 2.7.22, commit
`0715c31f3f1d85924107d6aa5329806db69df899`, with module opaque SHA-256
`00f0961a164d8e87758a790db7fd0caf4d51c8f8bd0eea180b02143e654ef319`.
Root captured the zero-algorithm-call surface in
`build/filtering-ltl-public-surface-actual-1/capture.json`.

All four public functions are in scope: `eventually_follows`,
`A_next_B_next_C`, `four_eyes_principle`, and
`attr_value_different_persons`. The first three take `attribute_values`;
the final function takes singular `attribute_value`. The Polars sequence
signatures differ from the pandas/log fixed scalar argument signatures. Existing
native `LtlFilterOptions` and EventLog predicates do not establish Polars behavior.

The new native API is declared in `include/minera/polars_ltl_filter.hpp`.
`EventTable` and `LazyEventTable` overloads retain separate error type, phase and
length-carrying message. Typed roles, polarity, timestamp controls and optional
boundaries preserve literal selectors and absent versus explicit null controls.
The generic call carrier also preserves missing required operands, enum/string
parameter aliases and malformed parameter values for public diagnostic tests.

The frozen root-only recipe under
`build/filtering-ltl-polars-public-prepared-1` produced
`build/filtering-ltl-polars-public-actual-1`. It records complete source/result rows, schema, visible
Categorical/Enum dictionaries and public category declarations, apply/collect
errors, complete messages, warnings, public plans, source/control mutation checks
and opaque module identities. All complete actual call receipts are deduplicated
before the first algorithm entry. Its constant prefix/row-ordinal source payload
contains no case labels that could hide duplicate inputs. The original candidate
plan, effective-spec dedup audit, executed observer and cases are retained.
Both parsed-complete root audits were required and hash-bound before execution:
`build/filtering-ltl-retained-flat-search-1/audit.json` and
`build/filtering-ltl-combined-retained-search-1/audit.json`. They report zero
earlier actual/planned/unclassified calls in each exact LTL module. The archive
inventory covered 3,897 containers; the flat inventory and its exclusions remain
available in the original report. This bounded search is not a catalogue-wide
receipt index. Root alone executed the public observer once; no calls were replayed.

| Public function | Calls | Frames | Errors |
| --- | ---: | ---: | ---: |
| `eventually_follows` | 180 | 148 | 32 |
| `A_next_B_next_C` | 168 | 99 | 69 |
| `four_eyes_principle` | 168 | 137 | 31 |
| `attr_value_different_persons` | 159 | 132 | 27 |

All source and caller-parameter mutation checks were unchanged, and the module
opaque pin stayed unchanged. The complete receipt file SHA-256 is
`58403aae7c6c94a68dc457d98d62bcd2bc38978765c8c859e2f596304b65b59e`.
The records retain 11 UserWarnings and 297 DeprecationWarnings; public plan
observations and their own warnings/errors are separate. Native warning emission
and physical lazy-plan fidelity are not claimed.
The immutable public capsule is `docs/polars-ltl-filter-public.json.gz`
(610,562 bytes, SHA-256
`6270a238dc204c126e80ee816e5790449ad0f5b23f7ccce9fb7f1e9ab7fd417b`).

## Observed controls and rules

All four predicates select whole cases and retain complete source columns and
cell values. Case, attribute and positive selectors are active in ordinary
nonempty predicates. Public enum aliases take precedence over conflicting
string aliases in both insertion orders. Resource selectors are active for the
two performer rules and inert for sequence rules.

`timestamp_key` is active for both eventual and next filtering, even when
`enable_timestamp` is omitted or false. These predicates order events by selected
case and timestamp before testing sequences. Missing timestamp columns fail;
literal empty/custom selectors work. Reverse and tied timestamps distinguish the
ordering behavior. The two performer rules ignore timestamps, including missing
or malformed timestamp selectors. `enable_timestamp` and
`timestamp_diff_boundaries` are inert in **all four Polars functions** in this
bank, including true, malformed, reversed, zero and microsecond bounds. The
usable transport therefore exposes the ordering timestamp for sequence rules
and omits inert temporal constraint controls for Polars.

Eventual filtering requires each requested activity and an ordered occurrence
for every adjacent pair independently. It does not require one shared chain
through all operands: the retained `['A','A','A']` query accepts a case with two
A events. Empty sequences return the whole frame in the observed positive calls.
String sequences iterate characters, and ordinary list/tuple queries retain their
public behavior. Set/dictionary indexing errors are retained. Lazy eventual
results retain original source row order; eager results retain sorted row order.
Observed positive null and NaN case joins are excluded; negative null-case
selection is preserved.

Next filtering performs an apply-time collection, then matches a contiguous
Python list sequence inside each ordered case. Tuple/string/set/dictionary
queries retain their distinct comparison behavior; they are not coerced to lists.
An empty list matches cases in the retained nonempty source. A selection with no
qualifying cases constructs a Null-typed key frame: the later String-to-Null
join raises the observed collect-time SchemaError, even for negative selection.
Categorical/Enum case columns similarly retain their observed String join-key
mismatch. Eager DataFrame input raises its observed apply-time AttributeError
after sorting checks, rather than being silently converted to LazyFrame.

Four-eyes filtering intersects the condition for every pair of requested
activities. Positive selection requires both activities and disjoint resource
sets for each pair; negative selection requires an overlap for **every** pair.
It is not a whole-predicate complement, so a partly overlapping three-activity
case can satisfy neither polarity. Null resources do not compare equal in these
pair predicates; present empty resource strings do. Zero/one-operand queries
retain the entire frame in the observed calls.

Different-persons filtering groups resources for the singular matching activity.
Positive selection requires more than one distinct resource; negative selection
requires an existing matching group with one distinct resource. Null is one
distinct resource in the count, and absent activities do not create a negative
group. Literal numeric case selectors retain their actual expression semantics:
the observed integer selector groups the whole frame and appends an Int32
`literal` column. Its lazy metadata and materialized rows agree. The adapter
rejects an existing `literal` column collision outside this observed domain.

## Native acceptance boundary

The generated vectors contain every original complete expected frame and all
159 full diagnostic messages unchanged: 78 apply-time and 81 collect-time errors.
There are 236 shared small noinline frame functions and 675 small noinline case
functions. Test-only `/Od`, `/bigobj` and enabled assertions are coordinated by
root; production helper optimization is unchanged. The generated vector SHA-256
is `1b9c2b8740d8c17f94820d7ed057af78a498cdbce53520ffee8ae93405620111`.
Apply errors are checked without materialization; collect errors are checked
only after a LazyEventTable has returned. The driver catches failures and emits
full mismatch context from its first revision.
Root's `build/parity-closures/native-current/ctest-ltl-first.log` records the
successful Polars suite: all 516 complete frames, 159 class/phase/full-message
diagnostics and direct native controls passed. The same aggregate run and its
initial build attempts retain unrelated log/pandas repairs; they are not a
claim that its broader suite had passed at that time. The authoritative final
receipts are `build/parity-closures/native-current/build-ltl-sixth-final.log`
and `build/parity-closures/native-current/ctest-ltl-final2.log` (47/47). The
final source, objects, test executable/map and server/CLI identities are bound in
`docs/polars-ltl-filter-checks.jsonl.gz`; earlier build/test attempts remain
historical evidence.

Production acceptance is
`build/parity-closures/ltl-production-http-4/validation.json` (73/73), plus
`build/parity-closures/ltl-attributes-production-http-2/validation.json`
(101/101). Both used final server SHA-256
`be6f9e6b656cb157575554d0d2e4a11fa3984a4d935b04938dba903f3a944445`;
both dedicated owned servers exited, and the existing application was not
replaced. The archive retains the original production records and scripts,
including preceding attempts as historical evidence.

Final frontend receipts under `build/parity-closures` are
`frontend-ltl-full-final.log` (192 files, 9,829 tests),
`frontend-ltl-tsc-final-second.log` (successful empty TypeScript output), and
`frontend-ltl-vite-final.log` (successful production build). The corresponding
`frontend-ltl-dist-final` artifacts are included. These are executable
frontend/API checks; rendered browser acceptance is not claimed.

Additional native controls exercise all four friendly APIs, complete context
preservation, eager/lazy ordering, apply-time scans and restartable producers,
deferred empty-selection schema errors, literal-case schema projection, Enum
dictionary validation and single-pass diagnostic interpolation. User text such
as `%FRAME%` is never interpreted as a formatting directive. Public diagnostic
plan fragments are generated from retained error outputs, with dynamic columns,
roles and operands; they contain no input-ID/hash result lookup. More complex
unobserved selector/sequence-plan combinations remain outside the bounded
diagnostic projection.

The observed source domain declares String, Int64, UInt64, Float64, Boolean,
Null, Date, microsecond Datetime with and without UTC, named Categorical and Enum
dtypes. Complete visible category label dictionaries, including unused labels
and order, are projected into the existing EventTable dictionary carrier. Enum
sort ranks are precomputed once and unknown/missing dictionaries reject rather
than falling back to lexical order. Public category context/physical identity,
warning emission, lazy plan identity, Python object/index fidelity, error details
depending on unobserved group iteration, and unobserved dtype/value combinations
remain separate open gates. No module/package completeness status is promoted
by this bounded companion.
