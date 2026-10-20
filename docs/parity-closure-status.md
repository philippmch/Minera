# Parity closure status

Target: reference 2.7.22, commit `0715c31f3f1d85924107d6aa5329806db69df899`.
The complete catalogue remains 86 dispatcher packages and 387 implementation
and support modules. Full parity is not complete.

## Closed in this pass

Displayed diagrams, charts and tables now have direct PNG and raster PDF
downloads. A single diagram also has styled SVG export. The controls are
available in the dataset views and all 44 independent workspace branches.
SVG fonts are not embedded. This is a delivered application capability,
separate from strict algorithm module counts. All 9,628 frontend tests,
TypeScript and the production build passed; the product PDF writer's sample
was independently parsed, rendered and visually checked. Rendered application
interaction remains unverified under the previously recorded browser block.

The original-query workspace now exposes trace, prefix, suffix, start, end
and rework filtering for CSV, XES and full native logs. Results can be
downloaded as complete native JSON or XES, and opened with their actual
activity selector. Native-to-XES projections are explicitly labeled.
The native workflow and real multipart HTTP acceptance tests pass. All
9,651 frontend tests passed before the final handoff fixes; the final 67
focused tests, TypeScript and production build also pass. See
[reference-log-filter-workflow.md](reference-log-filter-workflow.md).

Quality-result JSON also preserves computed double values through parsing,
including negative zero, instead of rounding to six significant digits. The
numeric regression and affected core/evaluation checks pass. Its precise scope
is recorded in [quality-numeric-json-contract.md](quality-numeric-json-contract.md);
this output repair does not itself promote an algorithm module.

PNML imports now retain mixed ordinary/reset/inhibitor and parallel extended
arcs sharing an endpoint. Exact arc weights, IDs and occurrence inventories
survive export/reimport. Five affected native suites pass; see
[pnml-mixed-input-arcs.md](pnml-mixed-input-arcs.md).

| Requirement | Evidence |
| --- | --- |
| Arc-degree simplicity module | 120 retained public selector records covering 102 numeric states; exact scalar, identity, arc occurrence, baseline and input invariants; assertion-enabled focused and full evaluation contract checks |
| Extended Cardoso simplicity module | All 264 retained public graph fixtures, graph permutations, identities, inactive options, diagnostics and PNML entry points; assertion-enabled focused check |
| Start/end activity common helpers | 76 public sorting/threshold observations; stable mapping ties, adaptive scans, nonfinite factors and exact uint64 comparisons; assertion-enabled native check |
| Object-interaction graph | Empty event/object identities repaired; 90/90 direct public edge sets match, with preserved validation and existing OCEL regressions |
| Temporal-profile description | Empty-model newlines repaired; seven complete public-byte outputs match; numeric text verified under a hostile locale and all four rounding modes |
| Infix-to-regex conversion | Missing converter implemented; all 34 captured outputs match, including wildcard boundaries and every ASCII escaping character |
| Log prefix and suffix filtering | Two modules match all 60 captured cuts, strict anchor exclusion, first/last occurrence, literal empty labels, custom keys and full metadata |
| OCEL 2.0 graph computation | New faithful companion matches all 134 captured outcomes, original-row multiplicity, copy/append behavior, selected failures and composite-specific inheritance behavior |
| PRIPEL encoded trace helpers | Three missing string helpers implemented; 205 captured length, enumeration and token edit-distance outcomes match |
| DECLARE descriptions | Supplied family and rule order preserved through formatter and original-model route; twelve captured results/errors match |
| Log start/end activity filters | Admission and supplied-variant helpers match all 66 captured outcomes, custom keys, polarity, protected endpoint class and exact thresholds |
| OCEL object-type endpoint filtering | Existing start/end implementations match all 160 captured full returned logs; six-table propagation, order and metadata verified |
| OCEL per-type object-count filtering | Exact integer/fractional thresholds added; 183 captured full returned logs and 175 compatible legacy controls pass |
| OCEL event-attribute filtering | Exact membership and timestamp companions match 195 captured full logs and 50 native diagnostics; typed datetime columns, nanosecond boundaries and all six table projections retained |
| Log rework filtering | Faithful standalone filter matches all 88 captured outcomes, typed targets, exact thresholds, lazy errors and noncomplement NaN polarity |
| Supplied case descriptions | All 32 captured byte/error outcomes pass, including nested containers, literal selectors, four formatting flags, no truncation above 2 MiB and input preservation |
| Log trace filtering | New serialized-activity API matches all 50 captured outcomes, custom keys, polarity, full metadata and missing-value errors |
| Object-codeath, descendants and inheritance graphs | Three direct modules each match 119/119 captured outcomes; literal IDs and missing-event diagnostics repaired; six affected suites pass |
| Petri-net descriptions | Sixteen captured full-byte outputs match; literal quoting and weighted/parallel arc occurrences repaired; full context suite passes |
| Object-cobirth graph | Empty IDs and missing-event failures repaired; 97/97 captured outcomes match, including ordering and parameter controls |

These are semantic module closures, including missing helper implementations
and repairs. They do not complete their parent families. Detailed contracts
and evidence limits are linked from each module's coverage record.

Coverage is now **46 complete / 40 partial packages**, **224 complete / 163
partial modules**, and **203 open strict coverage units**, down from 228.
The machine-readable source of truth is
[reference-2.7.22-coverage.json](reference-2.7.22-coverage.json).

## Active requirements

- Expose the exact OCEL membership, timestamp and count controls in the
  preparation workspace; continue object-attribute and OCDFG working-time gaps.
- Resolve Alpha+ identity-dependent cleanup selection. The old 28/64 mismatch
  count compares one reference process; both returned graph shapes occur in
  the broader public captures, so it is not evidence of 28 incorrect models.
- Keep all other partial rows, rendered UI, connector/platform, solver and
  performance requirements open until their own evidence supports closure.

The proof scanner's UTF-16 indexing defect is repaired and regression-tested.
The coverage check for these updates validates artifacts, symbol bindings and
registered cases against the unchanged declarations from the committed pinned
ledger. It is an offline structural check, not a fresh reference source audit.
