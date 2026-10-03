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
Existing vector/default initialization remains available. Ordinary PNML arc
occurrences use the full presentation; extended-arc documents retain their
existing executable route. Stale arc metadata still rejects explicitly.

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
