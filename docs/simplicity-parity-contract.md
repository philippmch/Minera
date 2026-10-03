# Simplicity parity contract

The target is PM4Py 2.7.22, pinned to
`0715c31f3f1d85924107d6aa5329806db69df899`. The simplicity package remains
**partial**. The sections below record graph-only arc-degree/Cardoso
observations, imported PNML/composite paths and executable/presentation
cyclomatic comparisons.

`evaluate_simplicity(const PetriNetPresentation&, ...)` preserves distinct
place/transition identities and separate arc occurrences. Names do not identify
nodes. Arc-degree counts each arc object once; its weight does not multiply its
structural degree. Cardoso counts distinct successor-place sets for each place's
outgoing transition choices, including the empty set. Parallel arcs and weights
do not multiply identical Cardoso choices. Invalid identities or endpoints
produce an explicit uncomputed result. Cyclomatic evaluation supports both
executable and full-presentation reachability, with their distinct input semantics
described below.

Arc-degree now accepts every double baseline on both native graph carriers.
`NaN` yields NaN, positive infinity yields 1, and negative infinity yields 0.
The shared `simplicity_arc_degree_k` request field retains those values, signed
zero and finite baselines. Malformed/empty/range-error numeric strings remain
invalid. Three actual CLI processes verify all four per-model results: NaN is
encoded as JSON null with `computed=true`, while the infinity controls encode
1 and 0. Composite coefficient domains are described below; other request fields
retain their existing numeric validation.

The preceding presentation capture executes 1,510 actual calls, with zero reference errors or
input/parameter mutations. It includes all 256 binary incidence graphs on two
places and two transitions, plus empty/isolated, weighted/parallel and equal,
different or empty successor-set controls. Direct and parent routes, defaults,
finite/signed/nonfinite baselines and the exported enum parameter key are
retained. Opaque module hashes match pinned bytes after the recorded Windows
CRLF-to-LF conversion. No implementation or bytecode is inspected.

The actual preceding native probe executes 1,510 metric calls and 1,510 request
parses: 1,440 scalar results match, 10 differ, and 60 parameter cases are
unsupported. Its executable graph aggregates weighted/parallel occurrences;
the full presentation is retained in its snapshots but that metric entry did
not exist. Twenty-four aliases contain parallel arc occurrences, even where
their old scalar happened to match through threshold clipping.

The preceding accepted presentation probe parses the actual request family, variant and baseline
before passing the resulting variant/options and full graph to the production
metric. All 1,510 scalar results match exactly, with NaNs compared as NaNs.
All request values, signed zeros, graph fields, arc occurrences and input-after
checks match. Native Cardoso represents the exact public integer scalar as a
double; the original public result type remains in the capture. Both actual
probes retain 1,510 independently witnessed metric and parser calls.

A fresh Release build on 650 frozen source/configuration inputs passes core,
request-parameter, evaluation and evaluation-contract suites. MSVC's NDEBUG
undefinition and retained assertion text establish that checks are enabled.
Registered regressions cover identity scopes/name collisions, weighted/parallel
arcs, Cardoso successor sets, malformed endpoints and nonfinite/presence rules.

`simplicity-structure-validation.json` binds the 776 exact archive
members, tested binaries, compiler/test logs, before/after values and CLI
outputs. Binaries are retained in byte-verified chunks. Frozen build bytes and
Git's LF source representation have separate hash bindings. The original
zero-call reference integrity-check failure and capture helpers remain
separate; the evidence-saver metadata correction adds no metric/reference calls.
The preceding pushed checkpoint and its archived core remain hash-bound.

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `simplicity-structure-checks.jsonl.gz` | `bf9a1a0f1031782e21af3e31d2c022250d1aa3a5109cdc642ea8b1147cc288ff` | 63456521 |

## Imported PNML arc occurrences

`PnmlDocument::arc_occurrences` retains imported arc objects in XML order,
including endpoints, weight, type and ID. Executable input/output vectors still
represent firing multiplicities. `to_petri_net_presentation(document)` projects
the current nodes and original arc objects for graph metrics. The document
simplicity overload and imported-model CLI/server routes use that projection
for arc-degree and Cardoso; cyclomatic still uses executable reachability and
caller-supplied marking options. Composite evaluation uses retained arc
occurrences for its simplicity component, as described below.

Document export preserves each imported arc occurrence and its weight/type/ID.
Metadata must exactly reproduce executable arc order and multiplicities;
malformed or stale arc data is rejected before output, and graph evaluation
reports an uncomputed result. Clear/update arc metadata when editing executable
arcs, and separately maintain node ID dimensions. Documents without arc
metadata retain the existing grouped-weight export behavior. Generated IDs
avoid collisions with retained node/arc IDs; imported nonempty IDs are escaped.

The new pinned public capture makes 264 actual PNML imports and 1,320 actual
direct/parent simplicity calls on the same exhaustive two-place/two-transition
incidence family and empty/isolated/weighted/parallel/Cardoso controls. All
imports and metric calls succeed without observed graph/parameter mutation.
Seven opaque module proofs match the pin, with only recorded CRLF normalization.
No implementation or bytecode is inspected.

The baseline and accepted linked probes each execute 528 native imports and
2,640 native metric calls, including export/reimport. The baseline matches
2,638 scalars, loses one parallel-arc inventory, and changes the parallel K=0
score from the public 1/3 to 3/7 before and after roundtrip. The accepted probe
matches every scalar exactly and preserves all 264 complete normal-arc
inventories, weights, IDs and XML arc order through export/reimport. Native
Cardoso's double compares numerically with the captured public integer.

Six successful baseline CLI runs retain the scalar contrast. Twelve accepted
CLI runs preserve arc objects and produce the expected imported metrics; a
real loopback server produces 14 matching metrics across 21 recorded requests,
including imported export/reimport and nonfinite controls. Application JSON
keeps its existing six-significant-digit finite representation; NaN remains
null with computed=true. Exact production doubles are verified independently
by the linked probe. Full wire precision parity remains open.

Five assertion-enabled Release suites pass: core, request parameters,
evaluation, evaluation contracts and extended Petri arcs. The full production
compile succeeded on frozen inputs. After correcting one test-only fixture,
649 of 650 inputs and seven production/unchanged test binaries were verified
byte-identical and reused; the changed test was compiled again and all five
suites rerun. NDEBUG undefinition and retained new assertion text are checked.

The archive retains 1,686 exact files in 1,721 verified chunk rows, including
public values, native snapshots/exports, all 650 accepted source inputs,
compiler/test/process logs and byte-verified accepted binaries. Two failed
build/test attempts and corrected capture/comparison helpers remain separate.
The initial CLI adapter failed after one successful CLI process; its output
and exact argv survive, but that helper did not retain PID/timestamps. This
limitation is explicit and does not substitute for accepted evidence.
Preceding pushed evidence and its baseline core remain hash-bound.

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `simplicity-pnml-checks.jsonl.gz` | `bd84b9c898c79133607c330d3efa53aa2834e045f0d3dd569490495310771180` | 69762434 |

## Composite evaluation on imported documents

The composite overloads accept `PnmlDocument` on EventLog/EventDataView and
stream/table/lazy carriers. Fitness, precision and generalization use the
executable net; simplicity uses retained arc occurrences. The EventDataView
route materializes once, and no additional fitness replay is introduced.
Omitted marking vectors inherit complete document markings when supplied;
explicit vectors override them. Stored markings must match the place count,
contain nonnegative tokens and fit the native int range. Stale arc metadata
produces an explicit uncomputed result. Imported CLI/server evaluation uses
the document overload.

Composite weight parsing now preserves negative values, signed zero, infinity
and NaN. Normalization accepts negative totals; overflowed totals and
nonfinite coefficients follow the observed IEEE arithmetic. A zero total
parses successfully and produces an explicit uncomputed evaluation result,
corresponding to the public ZeroDivisionError. Malformed, empty and range-error
numeric strings remain rejected. Exception class/stack identity is not claimed.

The fresh pinned parent-API capture makes 784 PNML imports and 784 composite
calls: 688 return results and 96 raise the zero-total error. Four weighted,
parallel and sequential graphs cover fitting/mixed/empty inputs, 16 coefficient
profiles, EventLog/DataFrame carriers, string/enum keys and alternate activity
attributes. All graph/log/marking/parameter snapshots are unchanged. Sixteen
opaque module proofs match the pin before/after, with only recorded CRLF-to-LF
normalization. No upstream implementation or bytecode is inspected.

Each actual before/after native probe makes 784 imports, 784 request parses and
1,960 composite calls. The baseline parses 400 requests, matches 740 metric or
error results, rejects 720 valid aliases, and has 500 simplicity plus 320
aggregate scalar differences among its computed results. The accepted probe
parses all requests and matches all 1,960 outcomes: 1,720 successful results
yield 17,200 exact scalar matches, and 240 zero-total aliases are uncomputed.
All returned fitness fields, precision, generalization, simplicity, aggregate
and fscore are checked. Native aliases/normalized coefficient metadata are
recorded separately from public-returned fields. NaNs compare as NaNs; raw
MSVC hexfloat spelling remains retained.

Grouped native carriers preserve public EventLog empty traces. Flat native
stream/table/lazy carriers match public DataFrame rows, which cannot represent
empty traces. Each lazy input is materialized exactly once. Baseline metrics
use typed constructor options independently of parser rejection; accepted
metrics exclusively use successfully parsed options. Document, input and
option snapshots are unchanged.

The baseline's 196 real CLI runs include 100 successful exits and 50 matching
metric/error results. All 196 accepted CLI runs and 196 real loopback HTTP
evaluations match; all 228 recorded HTTP requests return 200. Empty CSV,
alternate activity attributes, 24 zero-total results and 36 computed nonfinite
aggregates are covered. Application numbers retain six significant digits and
nonfinite values serialize as null; exact native doubles are checked separately.
The task-owned server is deliberately terminated after the checks, and that
exit is retained separately from request success. Discovery internals and the
startup health request are not added to instrumented producer call counts.

A fresh full Release compile on 650 frozen source/configuration inputs passes
all five assertion-enabled suites: core, request parameters, evaluation,
evaluation contracts and extended Petri arcs. No build reuse is needed in
this step. Seven source files differ from the preceding checkpoint. Release
NDEBUG undefinition, new assertion text, actual process logs and tested binary
hashes are verified. The archive retains 2,827 exact files in 2,864
verified chunk rows, including before/after outputs, complete accepted sources,
public values, CLI/HTTP records and accepted/test/probe binaries. Two zero-call
public API inspection failures, the zero-call probe compile failure and the
comparison's MSVC NaN decoding correction retain their original helpers and
derivation accounting. The preceding pushed archive and baseline core are
hash-bound; full package/catalogue completion is not inferred.

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `composite-pnml-checks.jsonl.gz` | `e2914a4d7a7dc5ba5bfd6faf471c26ee3179250e47af6992b57bc53e4b04711e` | 74739515 |

## Cyclomatic reachable transition occurrences

The executable cyclomatic metric counts each enabled transition occurrence
between reachable markings, including alternatives that share a target.
The public scalar on the observed implicit-name executable domain equals
that occurrence count. The native value, simplicity and complexity aliases
report it; strongly connected components remain separate graph metadata.
Default empty-preset initialization, explicit marking vectors and the native
state cap keep their existing behavior. Document forwarding uses the corrected
executable route at that checkpoint; the following section extends ordinary
documents and full presentations.

Two alternatives from one marking to the same next marking now give 2 rather
than 1. A two-state cycle with one token gives 2 rather than the previous 1;
with two tokens it has three markings/four occurrences and gives 4 rather
than 2. Its actual strongly connected component count remains 1. Isolated
transition loops count separately. All-visible, silent, mixed and empty-label
controls establish that these observed cycle scalars do not depend on labels.

Fresh pinned evidence contains 1,684 actual public metric calls, 2,788 separately
witnessed reachability API calls and 324 public default-marking discoveries.
The initial matrix includes all 121 token-nonincreasing binary incidence graphs
on two places/two transitions, sparse/dense zero and positive markings,
weighted/parallel controls and observed elapsed-time parameters. Larger controls
include cycles, entries/exits, branches, joins, loops, supplied-name collisions
and alternate labels. Every public metric succeeds. Before/after module hashes
match the pin with only recorded CRLF normalization; no upstream implementation
or bytecode is inspected.

An initial graph snapshot adapter encounters 18 endpoint-identity KeyErrors;
its original helper/output survives. A fresh graph capture includes referenced
endpoint objects even when name equality has merged graph.states membership.
It adds 1,428 graph calls and no metric calls. The failed snapshot rows are not
accepted graph observations. Scalar conclusions do not use synthesized graph
scores. A tempting source-name/transition-name pair-count rule fits all 1,428
small-matrix metrics but fails 80 of 216 larger supplied-place-name controls;
those observations retain the open presentation/name-order gates.

The native probes each execute 1,426 request parses and 2,852 actual metric
calls across executable/document routes. They compare 1,426 faithfully
representable public controls; 258 supplied-name/distinct-input-arc cases remain
explicitly outside that executable projection. The baseline matches 2,436
complete outcomes, with 416 scalar and 280 edge-count differences. The accepted
probe matches all 2,852 scalars and all three graph metadata fields exactly,
giving 17,112 field comparisons across the three scalar aliases and metadata.
Native doubles represent captured public integer scalars numerically. Public
graph metadata comes from independently returned named-transition graphs.
Document and option snapshots are unchanged.

The 25 before/after CLI controls include parallel alternatives, weighted routing,
diamond/fork/join structures, cycles with different markings and label variants.
The baseline matches 8; all 25 accepted CLI and 25 real loopback HTTP metrics
match exactly. All 51 recorded HTTP requests return 200. These integer scores
fit existing application numeric precision; broader wire precision is not
inferred. Discovery internals and startup health requests are not added to the
instrumented producer counts, and the server is deliberately stopped afterward.

The first fresh build hits MSVC C1001 in unchanged components.cpp/xutility and
does not run tests. That terminal capture remains separate. A second fresh full
build uses all 650 identical source/configuration inputs with one build worker
and no binary reuse. Four suites pass; the old evaluation suite still expects
parallel transitions to collapse to one edge/value one. That expectation and
its test name are corrected to the captured value two. The accepted bundle
keeps 649 inputs byte-identical to that successful production compilation,
reuses seven byte-verified binaries (core, four unchanged suites, CLI/server),
and compiles the corrected evaluation test against the bound core. All five
assertion-enabled Release suites then pass: core, request parameters,
evaluation, evaluation contracts and extended Petri arcs. Reused binary hashes
are guarded before and after configure, corrected test compilation and CTest.
Release NDEBUG undefinition and retained new assertion text are verified.
An initial retry-preparation audit mishandles the unexecuted planned CTest
record; its zero-call original and corrected helper remain retained.

The archive contains 1,144 exact files in 1,183 verified chunk rows,
including all accepted sources, original public/native captures, failures,
CLI/HTTP records and tested core/application/test/probe binaries. The preceding
pushed evidence and baseline core remain hash-bound. These finite comparisons
do not establish full cyclomatic, simplicity or dependency/package parity.

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `cyclomatic-reachability-checks.jsonl.gz` | `7f1700b8c466591d4cb885d0c314fb4a5e90ebf7bb3d586e9ce82cad99d0e48c` | 74628422 |

## Cyclomatic presentation reachability

The full-presentation and ordinary imported-document overloads now retain
individual weighted input arcs, supplied place/transition names and sparse
marking keys during cyclomatic reachability. Each input arc enables against
the original marking at its individual weight. The observed public graph uses
weak firing: consumed place entries disappear when depleted or oversubscribed,
then output arcs add tokens. Two weight-one inputs enable on one source token;
consume-only reaches the empty marking, one output token to the source forms
a loop, and two output tokens visit a second marking before looping. The
executable net keeps its existing summed-input semantics.

`SimplicityOptions::presentation_initial_marking` supplies ordered signed-token
entries keyed by presented place IDs. It overrides the marking vector, including
an explicitly empty optional marking. Untouched zero keys survive, consumed
nonpositive entries disappear, and invalid or duplicate keys reject explicitly.
Existing vector/default initialization remains available. At this checkpoint,
ordinary PNML arc occurrences use the full presentation and extended-arc documents
retain their executable route. The following section extends the document and
executable defaults to decoded extended arcs. Stale metadata rejects explicitly.

Marking names follow captured public repr/normalization behavior: Python-style
quoting and Unicode 14 alphanumeric characters plus underscore. The owned
classifier generator verifies all 1,114,112 Unicode codepoints and 734 ranges
against public Python character operations; it makes no PM4Py calls. Sparse
key presence and supplied names affect graph projection. Distinct outgoing
transition names are counted on the selected representative for each normalized
marking name; graph edge identity includes source name, transition name and
target name. Strongly connected components remain separate graph metadata.
Native traversal chooses representatives deterministically. This does not
establish reference set/allocation-order equivalence when names collide.

Fresh pinned captures retain 2,832 actual public metric calls, 2,692 independent
graph calls, 324 default-marking discoveries, 52 state-name normalization calls
and 20 PNML imports. There are also 630 is_enabled, 630 regular execute and
630 weak_execute calls. The main matrix replays every preceding 1,684 metric
control and adds 378 weighted parallel-input cases. Separate firing controls
cover 45 input/output fixtures with tokens zero through six and sparse/dense
keys. All actual scalars equal the independently returned graph representative
outgoing-name count on the 2,692 metric/graph pairs. Regular firing and weak
firing have independently captured outputs; an owned weak-firing candidate
matches all 630 actual weak outputs. No implementation or bytecode is inspected.
Opaque before/after module hashes match the pin after recorded CRLF conversion.
Metric errors and observed input/marking/parameter mutations are zero.

The order experiment makes 120 real metric calls over eight new allocations
per control. Twelve of fourteen previously differing supplied-name controls
return more than one scalar; all results are retained, including stable controls.
The main native matrix has 14 scalar and 23 full-result differences per target
route, all in captured name-alias cases. These remain explicit gates. No public
case is omitted, and neither a prototype nor a selected repeat replaces an
actual metric oracle.

Each accepted and baseline native bundle makes 2,692 request parses and 8,076
metric calls across presentation, document and executable-control routes. Both
accepted target routes compute every case. On the main 2,062 cases, each matches
2,048 scalars and 2,039 complete six-field outcomes. All 3,934 target results
without public name aliases match every field. The separate 630-case firing
matrix matches all 1,260 target outcomes and all 7,560 scalar-alias/metadata
fields. All 2,692 executable controls are unchanged. Baseline presentations are
unsupported; baseline documents match 1,781 main and 434 firing full outcomes.
Document, presentation and option snapshots remain unchanged.

The 20 application fixtures use exact bytes from independent public PNML
imports, with four input-weight pairs and five token counts. All 20 accepted
CLI and 20 real loopback HTTP scalar results match; baseline CLI matches 13.
All 41 recorded HTTP requests return 200. Startup health requests and discovery
internals are not added to instrumented producer counts; the server is stopped
after verification. These finite integer scores fit current application numeric
precision, without establishing broader wire behavior.

The original giant native initializer exits with Windows stack overflow
0xC00000FD before main: zero metric calls and zero parses. Its exact helper,
probe, build/execution logs and binary survive. A derivative partitions the
same ordered fixtures into construction chunks of 32 and completes all calls.
The first production candidate compiles all 652 frozen inputs and passes five
assertion-enabled suites, but its regular firing fails 24 non-alias graph
metadata comparisons. Fresh regular/weak firing captures identify the gap.
The final source corrects weak firing and adds depletion/self-loop regressions;
it builds all 652 inputs again with zero binary reuse. All five assertion-enabled
Release suites pass. The 647 unchanged parent inputs, three changed inputs,
two new files and final correction of two intermediate inputs are bound.
Intermediate sources, binaries and all differing comparison rows survive
separately. A copied comparison subtotal is corrected from 4,124 to 1,260 for
the 630-case firing matrix without replaying calls; its original before output
and corrected derivation remain retained.

The two archives retain 1,197 exact files in 1,272 verified chunk
rows, including all accepted sources, producer captures, failures, application
records and both new builds' core/test/application binaries. The second archive
holds the intermediate build; the preceding pushed archive binds unchanged
baseline binaries and sources. Exact base64 chunk and whole-file hashes pass
round-trip verification. The package/module catalogue is not promoted.

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `cyclomatic-presentation-checks.jsonl.gz` | `075f57dbdaad05289aca55f9373b66f21f4205ad06f31aa4ec0a8e2f11557a9d` | 82040213 |
| `cyclomatic-presentation-intermediate-checks.jsonl.gz` | `c9e823477c9b6980ad50be9a139070b8e021a076ac039c356bc0696ff519b83c` | 64563737 |

Supplied marking-name collisions and reference set/allocation order remain
open. Additional typed/huge/signed arc-weight and marking domains, extended arcs,
foreign keys and exception identity, caps/time limits and unbounded graphs are
not established. Broader evaluation/conformance, composite log/graph/marking,
PNML metadata, exporter bytes/IDs and full application wire precision remain
open. The catalogue stays at 46/86 packages, 199/387 modules and
228 incomplete units. Full module/package parity is not established.

## PNML arc types and default cyclomatic semantics

PNML import and export now use `<arctype><text>reset|inhibitor</text></arctype>`.
The pinned public importer recognizes exactly the lowercase, unpadded values.
It ignores the old `<type>` annotation. An unknown, uppercase, padded or empty
arctype value leaves an ordinary arc; when both tags occur, arctype determines
the type. Native import preserves the decoded arc occurrence type and executable
input category. Export writes the interoperable arctype annotation. Ten new
wire regressions check those cases and native import/export round trips.

The public default cyclomatic metric applies classic weighted enabling and
weak firing to reset/inhibitor arcs. A gate with zero tokens blocks an otherwise
enabled transition for this default metric. Document evaluation now validates
and retains all presented arc occurrences on that route. Direct executable
extended nets project their grouped ordinary weights and unit reset/inhibitor
inputs through the same presented calculation. Other execution consumers retain
reset/inhibitor behavior. Regression controls check zero/positive gate markings
and stale extended-arc metadata rejection.

Fresh public captures contain 1,056 actual PNML imports, 3,168 simplicity calls,
1,056 independent reachability graphs, 48 public exports and 656 calls each to
is_enabled and weak_execute. The 304-model matrix covers ordinary, reset,
inhibitor and combined classes; three parallel ordinary guard presets; source
tokens zero through four; gate tokens zero through three; and two-transition
sequences. Twenty-four tag controls and 24 independent reimports of exact public
export bytes complete the 352 original carriers. A separate 352-case capture
measures every executable projection independently, including the 160 controls
whose ordinary parallel endpoints aggregate. No projected reference scalar is
synthesized or borrowed from a different carrier. Before/after opaque module
proofs match the pin after recorded CRLF conversion only. Observed metric
errors and input/marking/parameter mutations are zero. No upstream implementation
or bytecode is inspected.

Each corrected baseline, intermediate and final native bundle makes 352 imports,
1,056 request parses and 4,224 actual metric calls across presentation, document,
imported executable net and independently constructed executable-control routes.
All 2,112 final presentation/document outcomes and all 1,056 executable-control
outcomes match every checked scalar alias and metadata field: 19,008 fields.
Cyclomatic metadata uses independently returned public graphs. Structural counts
and arc-degree complexity derive from captured public input fields; zero SCC
and zero structural executable edges remain local default metadata. Imported
executable results are also retained without substituting them for a distinct
public carrier oracle. Document, presentation, arc and option snapshots are
unchanged.

The expanded baseline retains arc types/weights on only 106/352 carriers and
matches 346/352 document cyclomatic results. Its independently typed executable
control matches 219/352 cyclomatic results. All three counts become 352 in the
final bundle. All 810 structural/ordinary executable outcomes remain unchanged;
133 extended cyclomatic outcomes change to the actual public result. The early
304-model metric-only baseline had passed all scalar comparisons because the
native parser ignored arctype and lost the extended types. Its 2,736 actual
metric calls survive as evidence of that insufficient carrier check.

The exact 352 native document exports are independently imported by the public
runtime. All decoded arc types/weights and net subclasses, three metric scalars
and positive semantic marking entries match. The final exports are byte-identical
to those measured intermediate exports, and all 2,112 target metric outcomes
remain identical. This proves the stated carrier comparisons, without proving
general metadata or equality with public exporter bytes.

The 80 accepted CLI results and 80 real loopback HTTP evaluations match their
actual public scalars; all 161 recorded HTTP requests return 200. Baseline CLI
matches 74/80. The completed intermediate applications also pass. The task-owned
server is deliberately stopped after each HTTP run. Discovery internals and
startup health requests are not added to instrumented producer counts; these
finite integer scores do not establish broader application numeric precision.

Both candidates compile all 652 frozen source/configuration inputs afresh with
zero binary reuse and pass five assertion-enabled Release suites: core, request
parameters, evaluation, evaluation contracts and extended Petri arcs. The 647
unchanged parent inputs and five edited files are bound. Three final inputs
differ from the successful wire/document intermediate candidate, which still
had incorrect direct executable defaults. Release NDEBUG undefinition, retained
new assertion text, actual process logs and all tested binary hashes are checked.

An expanded probe initially prints multiline exports with std::quoted, which
does not encode JSON control characters. Its wrapper fails after observing all
4,224 metric, 1,056 parse and 352 import markers. Exact raw output, probe, helper,
build logs and binary remain. Original command PIDs and wrapper exit one are
retained; original process timestamps were not retained and are not reconstructed.
The corrected producer escapes control characters and records process witnesses
before adapter assertions. Zero-call preparation/comparison-schema corrections
retain their original helpers and derivations; no executed producer is edited.

The five archives retain 5,258 exact files in 5,328 verified chunk
rows, including every accepted source, raw public/native/app capture, failed
adapter and both new builds' core/test/application binaries. Each core is split
across two archives. All five archives stay below 50 MiB; chunk and whole-file
hashes pass exact base64 round-trip verification across the archive set. The
preceding pushed archives bind unchanged baseline sources and binaries. No
package/module catalogue promotion is inferred.

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `pnml-arctype-checks.jsonl.gz` | `63f2087610c25a894b65f0273bccc3bae24cd64f534ac5b7159152ce573e3b52` | 42739189 |
| `pnml-arctype-core-1.jsonl.gz` | `c4e630ea6f5f920b0fadd9022da565097e6765a6a825f9518bf3c8a0b091016c` | 23147401 |
| `pnml-arctype-core-2.jsonl.gz` | `c676b0b512f0ddb2d890836f34f1a93d4c7bc59f0edefb1835dc0dd574155ccc` | 26733473 |
| `pnml-arctype-core-3.jsonl.gz` | `a886bc8dd0713543a896243ddcc8059afb7157125c03a60ffa66127b7af18804` | 23158742 |
| `pnml-arctype-core-4.jsonl.gz` | `13eaa7e5c43c8aaaa6c0986d4b47844e4b218210205b3ac7fe736b90ae01f9c3` | 26726709 |

Supplied marking-name collisions and reference set/allocation order remain
open. Additional nonunit, typed, huge or signed extended-arc weights and marking
domains, foreign keys and exception identity are not established. Malformed or
repeated PNML annotations, general metadata, IDs, zero-key/order behavior and
exporter byte formats remain open. Unbounded graphs, time limits/state caps,
broader evaluation/conformance/composite domains and full application numeric
wire precision remain open. The catalogue stays at 46/86 packages,
199/387 modules and 228 incomplete units. Full module/package parity is not
established.

## PNML arc weights and XML annotation text

PNML reset/inhibitor input weights now retain their signed magnitude through
the document, primitive extended-arc metadata, full presentation and both native
export routes. Nonunit, zero, signed, ASCII sign/padding/underscore and Unicode
decimal lexemes use an independently written integer parser. Ordinary zero
weights remain presented arc occurrences. Ordinary signed PNML weights reject
explicitly when no primitive multiplicity projection exists. Typed output arcs
still reject on this measured traditional-net domain.

The generated owned decimal classifier uses public Python 3.11.9 Unicode 14
operations: 1,114,112 classified and round-trip-checked codepoints, 660 decimal
codepoints in 66 blocks and 58 actual leading/trailing integer-whitespace calls.
These are standard-library operations, with zero PM4Py calls. The executed
generator, record and exact header are hash-bound. Native public controls and
registered regressions exercise Arabic and fullwidth digits; the generation
check is not a claim of a native parser call for every Unicode codepoint.

Arc annotation text follows the measured public XML behavior. Repeated text
children use the last value, including empty text. Adjacent ordinary text, CDATA
and removed comments coalesce. A child element or processing instruction ends
the first text segment, so its following tail is ignored. Prefix-qualified
arctype/text elements are recognized. Empty numeric text rejects, while an
inscription without a text child leaves the default weight. XML coalescing is
opt-in for PNML; existing default XML consumers retain their mode.

Direct full-presentation cyclomatic evaluation handles signed weights and
uint64 magnitudes with checked signed64 marking arithmetic. Huge positive
inputs can be disabled without conversion overflow. Negative inputs add tokens
during classic weak firing; negative outputs retain negative token entries.
Reset/inhibitor documents and primitive extended nets supply their retained
weights to this classic default metric. Other execution consumers keep their
arc-type behavior; this checkpoint does not establish their semantic parity.

The original public matrix contains 262 actual imports: 172 successes and 90
errors (12 ValueError, 10 TypeError and 68 Exception). It spans 144 input/output
weight controls, 96 type-text controls and 22 inscription-text controls. Its
independent unit source guard bounds successful graphs to two firings, including
signed and arbitrarily large public integers. Every original row is retained.
Actual public scalar calls, object fields and independently returned graphs are
the oracles. Before/after opaque proofs for 11 modules match the pinned commit
after recorded CRLF-to-LF conversion only. No upstream implementation or
bytecode is inspected, and no reference scalar is synthesized.

The baseline native bundle makes 248 safe imports and 1,152 metrics; the final
bundle makes the same 248 imports and 1,392 metrics. Successful imports improve
from 114 to 150, carrier weight/type matches from 78 to 150, and rejection
boolean matches from 74 to all 90 public import errors. Direct signed-magnitude
presentation construction improves from 156 to 164. Every successful final
document matches all three metric outcomes and six checked fields: 450 outcomes.
Presentations match 489/492 outcomes, with three explicit marking-range gates.
Primitive nets match 449/450, with the explicit zero-input structural gate.
The full comparison records 8,331 matching fields without discarding failures.
Cyclomatic metadata derives from returned public graphs; structural vertices
and arc-degree complexity derive from public input fields. Non-cyclomatic SCC
and primitive structural edges are local zero metadata. Input/parameter/carrier
snapshots are unchanged.

Every one of the 150 actual native document exports is independently reimported
by the pinned runtime. All arc weights/types, net subclasses, metric triplets,
enabling results and nonzero initial/final/weak-firing marking entries match.
Across original and reimport captures, totals are 412 actual public imports,
966 metrics and 322 calls each to graph construction, export, enabling and weak
firing. All 150 CLI and 150 loopback HTTP evaluations match their original
public integer scalars, and all 301 recorded HTTP requests return 200. Startup
health and discovery internals are outside instrumented producer counts. The
task-owned server is stopped after checks. The previous 352-case matrix is
also rerun with the byte-identical probe against the new core: all 4,224 metrics,
1,056 parameter parses, carrier snapshots and exact document exports stay
identical. No new public calls are made for that regression comparison.

The full serial build compiles all production, application and test binaries
afresh from 655 frozen source/configuration inputs. Eight old files change and
three are added; 644 old inputs remain unchanged. Four suites initially pass,
while the Petri suite fails its obsolete ordinary zero-weight rejection
assertion. Only that old assertion is corrected. The accepted bundle freezes
655 inputs, 654 byte-identical to the compiled candidate, and recompiles only
the corrected Petri suite with project references disabled. Seven exact core,
application and unchanged-suite binaries are hash-guarded through configure,
test compilation and all five passing suites. Release NDEBUG undefinition,
new assertion text, process logs and all binary hashes are retained and checked.

A zero-call preparation failure (missing local writer) and a zero-call probe
compile failure (guessed serialization header) remain with their original
helpers, exact fixtures/probe files, correction records and available logs.
Actual process witnesses for the compile failure are retained. Missing original
preparation process timestamps are not reconstructed. Corrected producers are
new derivatives; no executed helper is overwritten. The failed full candidate
and its original failed test binary also remain.

The three individually sub-50-MiB archives retain 4,315 exact files in
4,350 chunk rows. All 655 accepted inputs, public/native/application
captures, attempted failures, generated classifier proof, fresh core, original
and corrected test binaries, applications and probes survive. The fresh core
spans two archives; reused accepted binaries bind the exact candidate bytes.
Every chunk and whole file passes exact base64 round-trip verification across
the archive set. The preceding pushed archives bind unchanged baseline inputs
and binaries. No package/module catalogue promotion is inferred.

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `pnml-extended-annotations-checks.jsonl.gz` | `3f3ba6d32f20d266cde3e91917e5c55786ee72338c12bcb4b51712a4412182a7` | 24426111 |
| `pnml-extended-annotations-core-1.jsonl.gz` | `e48213b907f80854a0ef812603f8056e690a3835ef8b2ae20e4d55b59431b090` | 23224772 |
| `pnml-extended-annotations-core-2.jsonl.gz` | `25e1baf9ccb82fc3fc2a29154b3be94e12fbb81fbec8b11b023dfb5d1fc67ecd` | 26838333 |

The retained matrix still has 14 ordinary positive weights whose native
imports are deliberately unattempted because primitive multiplicity expands
into vector slots; the smallest of these weights is 2,147,483,647. Eight public
successes exceed uint64 weight magnitude. Four signed ordinary PNML cases lack
a primitive execution projection. Three directly presented output weights
exceed signed64 reachable marking range and report uncomputed. One ordinary
zero-input case loses primitive structural adjacency: direct executable Cardoso
is 1, while the actual public, document and presentation results are 2. These
are explicit remaining gates, with their original public rows retained.

Exception class/message equality, additional malformed/repeated/namespace
annotations, combinations of typed inputs and outputs, supplied marking-name
collisions and set/allocation-order dependence remain open. General metadata,
foreign keys, IDs, zero-key/order behavior, public exporter byte formats,
unbounded graphs, time limits/state caps, full application numeric precision
and broader evaluation/conformance/composite domains remain open. The catalogue
stays at 46/86 packages, 199/387 modules and 228 incomplete units. Full
module/package parity is not established.

## Compact ordinary PNML weights

Ordinary arcs retain zero, signed and large weights without magnitude-sized
allocations. A transition containing a zero, signed or greater-than-1024
ordinary arc stores one signed-magnitude weight per endpoint occurrence.
Small positive ordinary weights keep the preceding repeated-unit convention.
The six independently captured 1023/1024/1025 input/output controls cross that
storage boundary. Document, primitive-net, full-presentation and both native
PNML export routes retain the measured arc weights, signs and zero adjacency.
Legacy primitive PNML export sorting remains unchanged.

Shared enabling and firing use checked signed-magnitude arithmetic. Direct
compact ordinary enabling is measured for 18 models; all match the public
classic result. Sixteen actual weak firings match the independently returned
markings, while two report explicit signed64 limits. Default cyclomatic
evaluation projects compact ordinary nets to the full presentation. The old
ordinary-zero primitive Cardoso adjacency gap is closed in this matrix.
Other specialized consumers are not inferred to support compact weights.

The original 262 public observations are reused from the preceding pushed
checkpoint, including all 90 import errors. This step makes six fresh public
boundary imports and 170 fresh imports of exact native export bytes: 176 imports,
528 metric calls and 176 calls each to graph construction, export, enabling and
weak firing. Actual public APIs, returned fields and independently returned
graphs supply the oracles. Before/after opaque proofs for 11 modules match the
pinned PM4Py 2.7.22 commit after recorded CRLF-to-LF conversion only. No upstream
implementation or bytecode is inspected; no reference scalar is synthesized.

Both native executions attempt all 268 matrix imports and make 1530 actual
metric calls, plus 18 enabling and 18 weak-firing calls. Each has 170 successful
imports/presentations with matching arc carriers and all 90 public rejection
booleans aligned. Across document, presentation and primitive-net routes, all
170 arc-degree and all 170 Cardoso outcomes match per route; cyclomatic matches
167 per route. In total 1521/1530 outcomes match all six checked fields, yielding
9126 matching fields. Nine metric outcomes retain explicit marking-range gates.
Structural metadata is checked against actual public input fields; primitive
structural edges and non-cyclomatic SCC remain local zero metadata.

All 170 independently reimported native exports preserve weights/types, net
subclasses, actual metric triplets, enabling and nonzero weak/initial/final
markings. Every final actual export is byte-identical to its captured public
reimport input, so no public recapture is counted. The previous 352-case matrix
is executed against both newly built cores using the byte-identical owned
probe and old public oracles. All returned cases, 4224 metric outcomes, 1056
parameter parses, snapshots and exact document exports remain identical.

The first full candidate passes all 17 assertion-enabled native suites but
fails five large-output application imports during automatic conformance.
Five HTTP model checks return 400; following metrics use the prior model,
creating misleading raw matches. That application run is retained as failed
and is never accepted as validation. The corrected import path retains compact
models for independent supported evaluations and exports. Its conformance
bundle carries computed=false and an error; placeholder conformance fields,
including sound, are invalid in that bundle. The frontend displays the error
before any conformance scores or method controls.

Final application checks make 170 actual CLI runs, all exiting zero, and 511
recorded loopback HTTP requests, all returning 200. CLI and HTTP each have 167
public cyclomatic scalar matches and three explicit range diagnostics. Before
every HTTP metric, GET PNML binds the exact exported bytes of the newly imported
model. All 170 bindings match. Eighteen compact models expose unavailable
conformance in both CLI and HTTP; no weighted conformance score is claimed.
The task-owned server is stopped after checks. Startup health and discovery
internals are outside these instrumented producer counts.

Both full serial builds compile core, applications and 17 suites afresh from
655 frozen source/configuration inputs, with zero binary reuse. The accepted
build changes 12 existing inputs and preserves 643 from the parent. All 17
suites pass with Release NDEBUG explicitly undefined; new assertion expressions
survive in the actual test binary. Frontend TypeScript and Vite production
builds pass, along with 253 existing tests across the types, app-logic and
conformance files. Rendered UI inspection is not claimed.

A failed derived preparer accidentally changed the expected source count from
655 to 1255. Its 655 exact copied inputs survive; no CMake/build/native/public
commands ran. The original helper remains immutable and a corrected derivative
prepares the accepted build. Available process witnesses and all failed
application bytes survive; missing outer failure PID/timestamps are not
reconstructed. All 44 actual core/application/test/probe binaries are retained.
Candidate and failed-preparation source aliases explicitly bind identical
accepted bytes, while every differing candidate input is retained separately.

Six individually sub-50-MiB deterministic gzip archives preserve 6,354
exact members in 6,424 base64 chunk rows. Each fresh core spans two
archives. Every chunk and whole file passes exact round-trip verification
across the archive set. The prior pushed archives bind reused oracles and
baseline evidence. No catalogue promotion is inferred.

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `pnml-ordinary-weights-checks-1.jsonl.gz` | `786968b97538d125bf913b50a4b3d3af666d74b4edbad3890397e481bc938659` | 29333608 |
| `pnml-ordinary-weights-checks-2.jsonl.gz` | `dad9d61add74d7793a22dd1161275638bdb325802537914e78921f45df9e7d5f` | 18405793 |
| `pnml-ordinary-weights-candidate-core-1.jsonl.gz` | `03458ed238a647b3fc9a00fd4a68c1e79a964e283b14d265830df4a2ee679c88` | 23230248 |
| `pnml-ordinary-weights-candidate-core-2.jsonl.gz` | `2b3e209ef18c971ae2457a0e1c8e907f2c74126024702e07acca4da280373e6d` | 26870038 |
| `pnml-ordinary-weights-final-core-1.jsonl.gz` | `051fc19ec341470af20a41e454c726af6cd0a497612a2d039d53dde45fc2e794` | 23229589 |
| `pnml-ordinary-weights-final-core-2.jsonl.gz` | `275ab2069605a531b884a94d8db2532c988f4e663d0d5f47d6bb6010b5b2e30f` | 26874479 |

Eight successful public integer weight magnitudes still exceed uint64 and
reject explicitly in the native importer. Three large output profiles exceed
signed64 reachable markings on each of three metric routes: nine uncomputed
outcomes. Two independently attempted weak firings exceed signed64. Arbitrary
integer carriers and markings remain incomplete.

Specialized weighted conformance, replay, analysis, transformation and composite
consumers remain unverified. The application exposes unavailable conformance for
the compact-weight models; this checkpoint does not implement those methods.
Weighted frontend graph rendering/labels, full application numeric precision,
unbounded reachability and time/state limits remain open. Exception class/message
identity, broader malformed/repeated/namespace combinations, metadata, foreign
keys/IDs, set/order dependence and marking-name/zero-key behavior remain open.
The catalogue stays at 46/86 packages, 199/387 modules and 228 incomplete units.
Full module/package parity is not established.

## PNML transition labels and invisible flags

The native importer defaults a transition with an absent or empty usable name
to its visible node ID. It takes the first nonempty direct name/text initial
segment, retaining whitespace. CDATA and comments join that segment; a child
element or processing instruction ends it, and tails are excluded. Later name
elements can supply a label after an empty segment; they cannot overwrite an
already selected label. Namespace-local name/text elements follow the measured
controls. A visible empty transition ID remains an empty string label and is
distinct from a silent transition. Document export retains that explicit ID
and continues to validate global node-ID uniqueness before output.

A toolspecific element marks a transition silent when its tool contains the
case-sensitive ProM text and its activity contains the measured invisible
marker. Other tools and lowercase prom do not set the flag. Missing tool or
missing activity on a ProM toolspecific element is rejected. Four actual public
import rejections match native rejection booleans; exception classes/messages
remain different and are not claimed equivalent.

Two fresh public captures attempt 43 PNML imports: 39 succeed and four reject.
The successful models make 624 actual direct/dispatcher forward replay calls
and 39 public exports. Eight logs per model cover empty traces and typed t0,
A, B, AB, empty-string, space and newline activities. The corrected native
probe makes 43 imports, 39 exports and 312 actual replay invocations; each
native invocation maps to both independently invoked public routes. All 39
labels and transition names match, and all 624 public trace outcomes match all
ten returned trace fields (6240 fields), including ordered activated/problem
lists, enabled-transition sets and exact reached-marking keys/counts. Native
document and public net/marking snapshots remain unchanged across replay.

Every successful native export is independently imported by the pinned public
runtime. All 39 exact input byte bindings, labels/names, ordinary arc endpoints
and weights, net classes, initial/final marking keys/counts and 624 complete
public replay returns match the original public controls. Dictionary/Marking
item order is canonicalized only for comparison. Object identity, net/object
properties and public serializer formatting equality are not inferred.

An additional public API inventory makes zero semantic calls. A fresh weighted
discovery matrix attempts 58 successful imports and 1044 actual forward replay
calls across nine profiles and both routes. All 1044 calls return, snapshots
of net/markings/log/parameters remain unchanged, and all 522 route pairs have
identical complete returns. The ten fixtures originally named owned_hidden
contain no ProM invisible marker: the actual public label is hidden. They are
retained as visible named-transition controls; no weighted silent-routing
coverage is claimed. Actual signed and over-uint64 results are retained for
the separate weighted replay correction. These observations do not establish
native weighted conformance parity.

Across the inventory, weighted discovery, label controls and exact native
export reimports, this checkpoint makes 140 fresh public imports, 2292 actual
replay calls and 78 actual exports. Seven or nine opaque module proofs before
and after each process match the pinned PM4Py 2.7.22 commit after recorded
CRLF-to-LF conversion only. No upstream implementation or bytecode is read.

The prior 352-control regression is executed on the new frozen core with a
byte-identical owned probe and reused public observations. Entire returned
cases, 4224 metric outcomes, 1056 parameter parses and 352 exact document
exports remain identical to the pushed parent. Application checks make 43
actual CLI invocations and 83 recorded loopback HTTP requests: each route has
39 accepted models and four rejected imports. Seven conformance fields per
accepted model match the captured t0-event trace. Every accepted HTTP bundle
is followed by an exact GET PNML carrier byte check; all 39 match. Both checks
must pass. Startup health and discovery internals are outside recorded request
counts. The task-owned server is stopped after the checks.

The first full build passes 18 of 19 assertion-enabled suites, then fails the
old arc-occurrence assertion that unnamed t1 is silent. Only that obsolete
expectation is updated to visible label t1. A fresh full serial build passes
all 19 suites, including both forward replay diagnostics/transport suites.
Both builds freeze 655 source/configuration inputs and compile 22 binaries
afresh with zero reuse. The accepted source changes three existing inputs and
preserves 652 from the parent. Release NDEBUG is explicitly undefined for all
19 test projects and new assertion expressions survive in the actual binary.

Original failed preparations/derivations remain retained: a stale weighted
fixture-count guard after copying 48 fixtures, a followup pointer-count guard,
a partial build derivation with an incorrect pointer literal, and an integration
packed-bool decoder failure before any application call. A baseline probe
also executes successfully but emits invalid JSON because std::quoted wins
over an owned helper for lvalue strings. Its raw bytes and three exit-zero
process witnesses are retained as failed serialization, never accepted as
comparison evidence. A corrected derivative makes 43 imports/344 replays and
supplies the valid baseline. Missing outer failure PID/timestamps are not
reconstructed. No producer failure is hidden or overwritten.

Six individually sub-50-MiB deterministic gzip archives preserve 2,004
exact members in 2,076 base64 chunk rows, including both fresh cores,
all 48 build/probe binaries, all 655 accepted source inputs, differing failed
inputs, explicit identical-source aliases, public inputs/returns and immutable
helpers/process witnesses. Every chunk and entire file passes exact round-trip
verification across the archive set. Parent archives bind reused evidence.
Frontend code is unchanged and its previous checks are not reported as fresh.

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `pnml-transition-labels-checks-1.jsonl.gz` | `61a70a2b93391dfe247013fa6c2c1661c71bced65a9e6311112b1d491f22b34b` | 29164468 |
| `pnml-transition-labels-checks-2.jsonl.gz` | `563d529b2a46452d81c9cbda7f334ee511fd6e32d25ae8badf37ffc020e943ee` | 19098859 |
| `pnml-transition-labels-failed-core-1.jsonl.gz` | `0a0601ade81eb3f9612dac5e9f5ec3b1705407147bb08d01381d42dfb7f48011` | 23210238 |
| `pnml-transition-labels-failed-core-2.jsonl.gz` | `ffbbfb9449c4695ab774c2fb21120003860dced69703bce73bb27e5961372ec2` | 26886096 |
| `pnml-transition-labels-accepted-core-1.jsonl.gz` | `8934614bd69c56b2915e88b4d4324bd60088869903251c2a911f5c79836aa890` | 23224891 |
| `pnml-transition-labels-accepted-core-2.jsonl.gz` | `5233c6d7fd0414985c80ecfc2f19285550a156814518098bdb02928ec8b08596` | 26877623 |

Compact weighted replay/conformance is not corrected by this label change.
The existing application computed=false/error gate remains for zero, signed and
large ordinary weights. Public replay observations contain negative consumed or
produced totals and accumulated totals/markings exceeding uint64; native replay
still counts compact ordinary endpoints by one and lacks arbitrary signed integer
counters/markings. Weighted hidden routing, other diagnostic and parameter
variants, specialized analysis/transformation, full UI numeric precision and
unbounded numeric/state behavior remain open.

Exception class/message identity is not matched. General malformed/repeated/
namespace combinations, exporter formatting and metadata, empty place IDs,
foreign IDs/keys, set/order dependence and broader markings remain unverified.
The catalogue stays at 46/86 packages, 199/387 modules and 228 incomplete units.
No package/module promotion or full parity is claimed.

## Weighted forward replay follow-up

The separate signed ordinary-weight forward replay correction now matches all
1098 supported public trace outcomes and 1544 measured place counter fields.
Its declared scope, exact source/binary provenance and remaining carrier/method
gates are recorded in [token-replay-parity-contract.md](token-replay-parity-contract.md)
and [weighted-token-replay-validation.json](weighted-token-replay-validation.json).
Weighted aggregate/composite conformance remains gated; the catalogue is unchanged
at 46/86 packages, 199/387 modules and 228 incomplete units.
