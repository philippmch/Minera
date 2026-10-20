# Petri-net description parity contract

The completion target is `algo.querying.llm.abstractions.net_to_descr` for
PM4Py 2.7.22, pinned to `0715c31f3f1d85924107d6aa5329806db69df899`.
The native public entry points are `abstract_petri_net` in
`include/minera/original_model_context.hpp`: a supplied net with integer initial
and final markings, or the corresponding named `PnmlDocument`.

The normalized domain has string place/transition names, optional string activity
labels, coherent graph endpoints and integer marking entries. Document arc
occurrences retain the public graph's distinct arc objects, separately from
executable firing multiplicity. Document IDs retain original place names,
and native transition-name metadata retains original transition names. Arbitrary
Python object-valued names or deliberately inconsistent mutable root/endpoint sets
are not claimed as native constructors. Native missing/short metadata produces
checked errors, already covered by the workbench tests.

## Observable formatting and corrections

Places, transitions and arcs are sorted by their displayed strings. Marking entries
are sorted by their unquoted `place:tokens` strings and then represented as a list
of quoted strings. Explicit zero marking keys remain present; absent keys do not
become zero entries. Arc weights do not appear in the description. Empty graphs,
silent transitions, omitted/true/false headers and complete long labels are retained.
No character budget or truncation is applied.

Original arc occurrences are authoritative when present. Their existing PNML
projection validates endpoints, type/direction, weights and consistency with the
executable graph, then contributes one displayed arc per object. Without occurrence
metadata, the existing canonical PNML inventory applies: repeated primitive
endpoints form one weighted arc, while explicit compact-weight slots retain their
own occurrences. Reset and inhibitor arcs remain distinct displayed arcs.

The previous formatter read executable endpoint vectors directly. A small positive
PNML weight expands those vectors, so a single weight-2 arc appeared twice, and
two parallel arcs with weights 2 and 3 appeared five times. The correction reuses
`to_petri_net_presentation` for its validated inventory, without changing graph
construction or execution. Stale occurrence metadata now produces a clear native
diagnostic rather than silently rendering a different graph. Source storage and
owned-error snapshots remain unchanged. Unrelated display IDs, marking keys and
stochastic fields keep their existing read rules.

Two different string rules matter. A transition label is interpolated literally
inside fixed single quotes. An embedded apostrophe, backslash, NUL, newline or
nonprinting Unicode character remains literal; it is not Python string `repr`.
The native formatter previously applied `repr` here, changing those bytes.
The correction preserves the exact public text even when it is not a parseable
Python expression.

Marking entries do use Python-style string representation. They choose a quote
delimiter, escape that delimiter and backslashes, use named tab/newline/carriage
return escapes, and render other nonprinting characters with `\x`, `\u` or `\U`
escapes. The previous native helper handled only ASCII controls and surrogates.
It now decodes UTF-8/WTF-8 and uses the existing owned Unicode 14 printability
table in `root_cause_unicode.hpp`, matching the captured Python runtime's database.
Malformed native byte sequences are preserved literally as native extensions;
they are not replacement-decoded or presented as Python Unicode inputs.

## Actual observations and verification

Root executed sixteen public `abstract_petri_net` calls once: sixteen returns,
no exceptions and no input mutations. All graph names were explicitly supplied;
the tests do not depend on generated object identities or set construction order.
The corpus contains empty and ordinary models with all three header forms;
quote, control, empty, Unicode and surrogate labels; nonprinting place names in
markings; reversed construction; weighted arcs; and a 12,001-character label.
The four new supplemental calls distinguish one weight-2 arc, two weight-1
parallel arcs, parallel weights 2 and 3, and ordinary/reset/inhibitor arcs with
shared endpoints. They retain actual public arc classes and complete graph
snapshots. None of the first twelve calls was replayed.

`petri_public_byte_vectors` reconstructs these named native graphs from the public
snapshots and compares every complete output byte, Python character count and
`truncated=false`. It also compares full native model storage before/after.
Expected strings use the authoritative `result.hex`: ordinary JSON decoding can
merge an escaped adjacent surrogate pair. That input is retained by the hash-bound
original capture recipe, and its two WTF-8 sequences remain distinct in both
native input and captured output. `petri_native_byte_extensions` independently
checks raw invalid/truncated/overlong byte preservation and the shared tuple quote
policy. Existing temporal, DECLARE, skeleton, Petri constructor and owned-error
regressions remain in the same test executable.

`petri_arc_occurrence_byte_vectors` imports three literal PNML documents and uses
existing full native literal storage for the fourth mixed-type graph, matching
the supplemental public graphs and comparing complete output bytes/counts. It
checks the original occurrence inventories against expanded execution vectors,
the documented primitive fallback, and stale-inventory errors with unchanged
full storage. The old test that treated stale occurrences as inactive now checks
the explicit owned error and retained PNML diagnostic instead.

Root compiled the changed implementation and complete context test executable
with assertions enabled, then independently reran the final CTest: **1/1 passed**.
It covers all sixteen Petri byte vectors, the seven temporal byte vectors,
locale/rounding regressions and existing context/workbench checks. The link map,
build logs and actual `LastTest.log` remain under
`build/parity-closures/petri-description-native`. This is a focused integration
result, not a fresh run of every native suite; no public call was replayed.

Failed integration history remains alongside the accepted check. The first
arc-suite run aborted because the mixed-type fixture used the existing PNML
importer's rejecting mutators. Full literal storage already represents that
graph, so the fixture now uses it without changing any expected output bytes.
A second assertion wrongly required a separate optional PNML export error to
equal the primary context error; the corrected check requires both the exact
primary occurrence diagnostic and a retained nonnull export diagnostic. The
original failed CTest/build logs and `arc-diagnostic-*` outputs are retained.
Windows assertion/exception reporting now makes such failures visible.

The PNML importer rejected ordinary/reset/inhibitor arcs sharing an endpoint
during this capture's first integration attempt. The subsequent parser repair
and five passing regression suites are recorded in
[pnml-mixed-input-arcs.md](pnml-mixed-input-arcs.md). This does not establish full
PNML import parity; the description module's supplied-graph contract is separate.

The compact `petri-description-public.json.gz` archive contains the complete
two actual captures, both own recipes, snapshot helper, runtime packet and fresh root pin
receipt. Members record original UTF-8 bytes and SHA-256 for exact reconstruction;
returned byte files can be reconstructed from `result.hex`. Root independently
verified opaque installed wrapper, abstraction and public Petri object-definition
bytes against the pinned Git blobs, allowing only CRLF normalization. No reference
implementation text, AST, bytecode or private state was decoded or inspected.
Archive SHA-256:
`bbad153a7d15ac14a4e3bbf2764c0ec3e9798da9f87d5044515cab8b066e7486`
(19,102 bytes).

The general contract follows sorted node/arc displays, literal transition-label
interpolation, exact integer decimal markings and Unicode-category-based string
representation. The finite cases test these distinct rules without claiming an
enumeration of every graph. Cached discovery/context adapters, other abstraction
modules, Petri import/export algorithms and the complete catalogue retain their
own statuses.
