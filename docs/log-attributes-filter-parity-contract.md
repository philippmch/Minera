# Log attributes filter parity contract

The native companion implements all eight public functions of `pm4py.algo.filtering.log.attributes.attributes_filter` from PM4Py 2.7.22, pinned commit `0715c31f3f1d85924107d6aa5329806db69df899`. Public signatures, docs, enums, full public calls and opaque hashes were used. Reference implementation source, AST, bytecode and private state were not inspected. Closed legacy filtering APIs remain unchanged.

The API in `include/minera/log_attributes_filter.hpp` provides categorical whole-trace/event/trace-attribute selection, numerical whole-trace/event selection, maximum activities, relative occurrence and the supplied-cache threshold helper. Values, bounds and selectors use typed ordinary `EventAttribute` carriers; numerical comparisons retain exact integer values and datetime awareness. Relative integer cutoff multiplication uses exact TokenInteger arithmetic, including large bounds and the omitted event-count numerator that can exceed the number of traces. Categorical trace matching skips empty traces; negative trace-attribute selection includes missing attributes. The ordinary attribute selector is `ATTRIBUTE_KEY`; `ACTIVITY_KEY` is active for maximum activities. Literal empty names remain literal. Stream key/value filters apply to numerical whole-case selection, while numerical event selection ignores them.

Mutable converter-bearing overloads retain real source mutations before returns and errors. Categorical events, numerical cases/events, filtering maximum-activity calls and relative occurrence inject all trace attributes into source events under the `case:` prefix. Event results regroup by their selected full case cell, remove case-prefixed event fields and reconstruct trace attributes, dropping trace properties. If those attributes lack concept:name, they append the typed selected group identifier as the canonical trace ID after the existing attributes. Numerical whole-case results retain complete mutated source traces and their properties; a case can appear in both positive and negative results when it has both inside and outside events. Categorical events, numerical events, relative occurrence and filtering maximum activities use the case-id parameter as a full event cell name. Their custom case:alternate selector succeeds with canonical Trace identifiers absent, where the default raises the captured KeyError. Numerical whole-case selection instead uses a trace key that is prefixed for stream lookup. Its separate glue parameter changes lookup, not the converter's injected prefix. Const conveniences explicitly copy the input.

Maximum activities ranks by descending frequency and preserves first-seen activity order among equal counts. Opposite label orders Z/A and A/Z each retain the first encountered activity at maximum one. Maximum activities uses `ACTIVITY_KEY`, rewrites the caller's literal `ATTRIBUTE_KEY` to that value/default, and preserves distinct enum-key entries. The ordered mutable `LogAttributesFilterParameters` carrier exposes that mutation. It occurs for explicit empty dictionaries, before bound errors, and on unchanged-source returns. An enum attribute key can continue to take precedence over the newly inserted literal. Maximum limits that admit every activity return the original value without converter mutation; filtering calls delegate to event selection. Relative occurrence has distinct omission defaults: omitted keep_once_per_case counts event occurrences but divides by all source traces, including empty traces. Explicit true counts once per case and divides by all traces; explicit false counts event occurrences and divides by the event total. The three-way fixture with two empty traces and A/B/C/D counts 4/3/2/1 at half threshold selects A/B only on omission, and selects nothing for either explicit boolean. Threshold and comparison errors can precede converter injection, while later filtering errors retain it.

The threshold helper accepts a genuine typed attribute-to-count dictionary, a full variant-to-traces dictionary and the supplied ordered vc list. It reads `variants[vc[0][0]][0]` and uses that actual trace's selected event values as exemptions. Cache contents are independent of the variant key: an external trace containing A/D under key ('CACHE',) exempts A/D source events. Count membership is required and count comparison precedes exemption. Empty vc or empty cached trace vectors raise IndexError, absent keys preserve typed KeyError values, and missing selected source cells are skipped. Returned traces retain attributes and drop trace properties. Neither source nor cache mutates. The native cache ABI copies full trace values and metadata; it does not claim Python alias-graph identity.

Root executed six banks once: 255 first-bank calls, twelve disjoint valid-boundary calls, four disjoint max-parameter timing calls, four discriminating case-selector forwarding calls, two opposite-order equal-count ranking calls and three omission/explicit relative-counting calls. All 280 receipts are distinct: 182 returns, 98 exact errors, zero warnings, 162 source-mutated calls and 165 control-mutated calls. The extra three are genuine caller-parameter-only mutations; all other aggregate control changes include aliases to the mutated source. No independent auxiliary cache mutation occurred. Continuations reconstructed and asserted all 255, then 267, then 271, then 275 and then 277 full prior receipt hashes before new entries. Original candidates, complete contexts, controls before/after, warnings, errors, full outputs and durable intents/dedup audits are retained under the corresponding `build/filtering-attributes-log-*-public-actual-1` directories.

The opaque module SHA256 is `afaa57bc2455ca58dc5845e0283c67aef3835a468ba61d939469d31f4f726a46`; public surface Git blob is `e31cb190d566d0106d9c3452cfac220b439a02aa`. Both root and worker retained searches found no earlier exact-module algorithm receipts. The generated acceptance driver contains 153 complete snapshots, full external cache trace values and separate non-inlined trace/snapshot/case frames. Each case compares the complete result/error, complete source after both success and failure, mutable parameter ordering/enum identity and cache preservation. Additional native storage controls retain first-class roles, independent event case fields, input order, context, const-copy isolation and parameter mutation before an error.

## Original typed calls through API and UI — 2026-10-22

The attributes workspace now offers an original typed-call editor and exact UTF8
File import. `POST /api/reference-log-attributes` selects this mode by presence
of `call_json`, using schema `minera-log-attributes-original-call-v1`. It requires
an actual complete native EventLog `source_file`; friendly CSV/XES controls are
exclusive. Ordered `args` omit the source: membership and relative operations
take one operand, numeric operations two, maximum zero or one, and threshold
four or five. Missing/extra operands are transport controls, without invented
reference binding diagnostics.

Parameters are absent when both parameter fields are omitted. `parameter_value`
preserves a raw typed value; `parameter_entries` preserves an ordered array of
`{key, enum_key, value}` entries, including an explicitly empty dictionary.
These fields are mutually exclusive; threshold forbids both by presence.
Recognized public string/enum identities retain native resolution and actual
maximum-activity dictionary writes before returns and errors. Repeated underlying
names reject as an unsupported transport domain. Common typed operands retain
exact integer tokens, float values, null/NA/NaT/UUID/bytes/date distinctions and
recursive containers. This reuses the unchanged LOG-private codec.

Threshold count dictionaries use `{type:"attribute_counts", entries:[[key,value]]}`
with typed keys and values. Variant dictionaries use
`{type:"variant_cache", entries:[{key, traces:[handle]}]}`. A handle has exactly
one nonnegative integer `source_trace_index` or `auxiliary_trace_index`. The
latter requires an actual complete native `variants_file`; otherwise that File
is forbidden. Raw typed count/cache alternatives reach native diagnostics.
Full source and external Trace values are copied into the native cache. The
observed bank has no ordinary non-Trace dictionary values; that unobserved
transport domain rejects explicitly.

The fixed envelope has 30 fields. Its four additions are `call_mode`, exact
`call_json`, `threshold_cache_before_json` and `threshold_cache_after_json`.
Complete materialized cache artifacts retain dictionary/raw union tags, typed
keys/counts, variant entry boundaries, every full trace, vc and the typed selector;
they are independent of the three exact raw descriptor echoes. Actual caller
parameter artifacts distinguish all 202 dictionaries, four raw NA values and
44 omissions. Original-mode `filter_attribute_key` stays null; the imported
activity role independently controls reopening. Original error args and Python
identity metadata remain in retained receipts; API errors expose type/full
message, without claiming arbitrary Python object or alias-graph identity.

A separate closed receipt adapter reconstructs all 280 original canonical IDs,
sorted set SHA256 `ec5330eb47c1a4611d4790d94a02178cde7e914b48d6f5be6c9b93a5fe88a597`.
Root's fresh index resolves all 280 intents with zero gaps and twelve rehashed
cache hits. All 41 receipt integrity tests pass. The full native oracle remains
byte-identical (3,481,841 bytes, SHA256
`657b5ab0e92b78de2e85adc685c1f6e949623f12390ddbeca382cc72a61a2e7c`).
The public acceptance target directly reuses all 153 snapshot constructors and
all 280 original cases, then compares all 280 calls through both direct API and
owned HTTP: 182 full logs / 98 exact errors, complete source after both outcomes,
202 dictionary / four raw / 44 omitted parameter states and all 30 full caches.
All four native/control/transport targets pass. All 193 frontend files / 9,865
tests, 35 focused tests, TypeScript and production build pass.

Actual rendered browser acceptance passes 17 checks with 15 attributes requests
and one actual mine reopen. It covers all eight operations, source/external
caches, caller-only mutation before failure, raw NA parameters, exact large
integer/CRLF import, complete artifact downloads, held actual HTTP response
cancellation and deferred own File import cancellation. Exact XES File bytes
and the independent opening role passed unchanged to native fetch, and the
actual mine response/navigation succeeded. Raw encoded multipart body inspection
is not claimed. Listener ownership was verified; both new owned processes closed
with stdio drained. Root inspected success/error screenshots. Two locator failures
and two burst-download timeouts remain preserved. An isolated first XES download
passed; spacing downloads in the complete harness then passed with the same
product server. No product or expected-result change was needed for those attempts.

This bounded checkpoint makes no new reference call, replays no retained
reference call and promotes no catalogue row. All 86 packages / 387 modules,
201 strict open units and 565 unentered grouping queries remain in scope.

## Historical native/friendly acceptance

Coordinated native acceptance passed all 41 suites, including all 280 retained LOG outcomes, complete source/control mutation assertions and exact integer cutoff/storage controls. Authoritative receipts are `build/parity-closures/native-current/configure-attributes-workflows-first.log`, `build-attributes-final.log` and `ctest-attributes-final.log`. Initial failed module/direct receipts and the three original source/compiled-binding snapshots remain preserved. The final owned production server passed all 95 cross-family HTTP requests in `build/parity-closures/attributes-production-http-4/validation.json`, using server SHA256 `1d6b11e78d1197130a162db0419df62ec62e906de4cbee1db60e26e74c54b1e3`. It exited after validation and did not replace the existing application. Frontend acceptance passed 190 files and 9,809 tests in `frontend-attributes-full-third-final.log`, plus 65 focused checks, TypeScript and Vite. The first three HTTP attempts remain preserved with their original executed scripts, records and failure contexts: the harness needed to decode valid byte-wrapped table counts, respect the threshold helper dropping returned trace properties, and reopen XES with the explicit exported activity role. Those three harness corrections used the same unchanged product server. The final checks archive binds the current header, implementation, driver, executable, map, object files and production server to these receipts. The canonical row remains partial; finite banks do not establish arbitrary Python objects, every malformed parameter/container shape, arbitrary alias graphs or pathological hash collisions. No rendered-browser validation is claimed.

## Disjoint bank closure (2026-10-25)

The module is promoted on the seeded public-call bank in
[log-filter-domain-parity.md](log-filter-domain-parity.md) (240,450 calls
across the seven EventLog filtering modules, zero failures), which also lists
the native rules that bank added.
