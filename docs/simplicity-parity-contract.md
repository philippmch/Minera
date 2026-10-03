# Simplicity parity contract

The target is PM4Py 2.7.22, pinned to
`0715c31f3f1d85924107d6aa5329806db69df899`. The simplicity package remains
**partial**. This checkpoint verifies the graph-only arc-degree and extended
Cardoso metrics through a native full-presentation and imported PNML entry points.

`evaluate_simplicity(const PetriNetPresentation&, ...)` preserves distinct
place/transition identities and separate arc occurrences. Names do not identify
nodes. Arc-degree counts each arc object once; its weight does not multiply its
structural degree. Cardoso counts distinct successor-place sets for each place's
outgoing transition choices, including the empty set. Parallel arcs and weights
do not multiply identical Cardoso choices. Invalid identities or endpoints
produce an explicit uncomputed result. Cyclomatic evaluation retains its
executable-net entry point because it requires reachability.

Arc-degree now accepts every double baseline on both native graph carriers.
`NaN` yields NaN, positive infinity yields 1, and negative infinity yields 0.
The shared `simplicity_arc_degree_k` request field retains those values, signed
zero and finite baselines. Malformed/empty/range-error numeric strings remain
invalid. Three actual CLI processes verify all four per-model results: NaN is
encoded as JSON null with `computed=true`, while the infinity controls encode
1 and 0. Other request fields retain their existing numeric validation.

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
caller-supplied marking options. Composite evaluation remains executable-only.

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

Extended cyclomatic reachability/parameters, composite evaluation and broader
evaluation/conformance dependencies remain open. General PNML metadata,
marking keys, malformed/extended-arc and additional carrier domains are outside
these finite comparisons. Public PNML imports omit explicit zero-token keys
that native documents preserve; public exporter byte/ID formatting is not
claimed equivalent. The catalogue remains 46/86 packages, 199/387 modules and
228 incomplete units. Full module/package parity is not established.
