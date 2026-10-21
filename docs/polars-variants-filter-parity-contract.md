# Polars variants: bounded native companion

`filter_polars_variants`, `filter_polars_variants_top_k`,
`filter_polars_variants_from_list` and `auto_filter_polars_variants` provide
`EventTable` and `LazyEventTable` companions for the four public functions of
`algo.filtering.polars.variants.variants_filter` in PM4Py 2.7.22 with Polars
1.44.1. The canonical module remains **partial**, with the remaining gates below.

## Membership and variant representation

Membership defaults to case `case:concept:name`, activity `concept:name`,
positive selection and canonical timestamp `time:timestamp`. It sorts by case
and canonical timestamp to build each case's comma-joined activity sequence,
then joins the original source. Returned rows therefore retain original source
order, complete columns and cell values, rather than the intermediate sort
order. Noncontiguous source rows belong to the same case. Opposite source/time
order controls distinguish timestamp ordering from source adjacency.

Each admitted inner sequence is iterated and **all** its string elements are
joined with commas. Inner lists and tuples both work. An inner string `"A,B"`
is character-joined into `A,,,B`, which differs from the two-element sequence
`["A", "B"]`. An outer string `"AB"` instead iterates the individual admitted
sequences `"A"` and `"B"`. The observed outer tuple, set and string-key dictionary
forms retain their Python iteration behavior; numeric/None/noniterable inputs
retain their exact errors. Empty inner sequences produce an empty variant label.
The comma encoding remains ambiguous when activity labels contain commas.

Positive selection retains whole matching nonnull cases. False polarity keeps
whole unmatched cases and all null-case rows: the public anti-join does not
match null keys. Captured None, numeric, string and collection polarities use
Python truth values. NaN case keys group together; signed zero case keys share
a group while the returned original floating cell bits remain intact.

Activity aggregation skips null cells. An all-null activity sequence becomes
the empty string, which matches an empty admitted sequence. Fresh matching
controls distinguish exact signed/unsigned decimal strings above 2**53,
`NaN` and `-0.0`, lowercase Boolean `true`/`false`, ISO Date text and the UTC
microsecond Datetime form `YYYY-MM-DD HH:mm:ss.000000+00:00`. Datetime strings
using `T` or a literal ` UTC` suffix do not match that observed cast.

## Top-k and public unavailable functions

Top-k first collects statistics during **apply**, including when its source is
a LazyFrame. Statistics sort by selected case and timestamp, retain raw activity
tuples, count equal tuples and sort by descending frequency with descending
lexical tuple order at ties. The equal-frequency A/B/C/D control keeps C and D.
The general offline model also reproduces the retained mixed None/int and
None/string comparison diagnostics before selected raw tuples are joined.
There is no admitted-equivalence relaxation or captured-case branching for ties.

The selected raw tuples are comma-joined and passed to membership filtering.
Top-k polarity remains active. A negative k follows Python sequence slicing;
zero selects no variants; huge integers and positive infinity are capped at the
number of variants. Finite nonintegral k and NaN retain the observed slice
errors when slicing is required. Nonstrings in selected raw activity tuples
retain their exact Python joining errors rather than being silently cast as
membership aggregation would cast them.

The undeclared generic `pm4py:param:timestamp_key` parameter is active for top-k
statistics. It is **inert for final membership**, which still requires and sorts
canonical `time:timestamp`. Fresh literal custom-time controls make these two
orders disagree: statistics can choose AB while final membership constructs BA
and returns empty. Omitting canonical time then fails during result collection,
after the statistics scan has succeeded. `PolarsVariantsFilterOptions` exposes
the statistics timestamp field while preserving that canonical membership rule.

Top-k's captured eager DataFrame calls fail with
`AttributeError: 'DataFrame' object has no attribute 'collect'`, after their
schema-resolution precedence. A native lazy top-k result scans once during
construction for statistics and scans its source again during each subsequent
materialization. Plain membership performs no construction scan. Native controls
check this distinct lifecycle and result independence.

The pinned public `apply_from_variant_list` and `apply_auto_filter` functions
fail with `AttributeError: module 'pm4py.util.constants' has no attribute
'DEFAULT_NAME_KEY'`, including supplied activity selectors. Malformed parameter
containers retain their earlier errors. The native companions preserve these
observed failures; they do not substitute intended successful filtering.

## Native carrier and phase contract

`PolarsVariantsFilterError` exposes `type`, `phase` and a length-carrying
`message`. The complete public type/phase/message projection includes literal
NUL, resolved plans, context stacks and joining/comparison errors. Membership
parameter lookup prefers enum keys over conflicting strings regardless of
insertion order. Top-k statistics instead unrolls equivalent keys in insertion
order, so a later string selector overrides an earlier enum selector there. The
final membership stage retains its separate enum-preferred lookup. The native
ordered binding carrier preserves both stages. Explicit missing-control
overloads distinguish an omitted required Python argument from a supplied None
value; the three captured missing-argument messages remain distinct.

Native attributes, metadata, source-role provenance, complete category-label
dictionaries and original row cells survive independently of filtering. Native
controls check sorted variant construction versus original output order,
negative null-case selection, full context, signed-zero payloads, lazy apply
scans and restarts, unavailable functions before source scans, missing versus
None arguments and result independence. Selected Enum sort columns require full
String label dictionaries, cached once per operation; every selected nonnull
cell is validated even on single-row frames that do not invoke comparisons.

Native tables require declared dtypes and no dataframe index. Full public
Categorical/Enum observations retain ordered labels, unused labels, declared
Enum categories and empty-result dictionaries. The native descriptor preserves
representable labels and order. Public namespace, physical representation and
runtime identity remain separately recorded and unclaimed by this carrier.

## Evidence and acceptance boundary

The zero-call public surface pins reference commit
`0715c31f3f1d85924107d6aa5329806db69df899`, opaque module SHA-256
`b1b4d4328210cd3a33df3b3c02cf3db07f4d8d928a92f97fdf6537a21a46c198`.
The completed global exact-module JSON/JSONL/Markdown inventory, with ignored
build receipts included, found only catalogue/coverage/status snapshots and
that zero-call surface before the new bank. No reference source, private state,
AST or bytecode was inspected.

`build/filtering-variants-polars-public-actual-1` retains 399 distinct actual
calls: 131 complete frames and 268 errors. All four public functions are covered,
including 35 eager controls, literal and enum selectors, malformed parameters,
case/activity dtypes, typed iterables, polarity, timestamp order/prerequisites,
empty inputs, cutoff ties and complete categorical context.

The disjoint continuation
`build/filtering-variants-polars-casts-public-actual-1` adds twelve new calls:
eleven frames and one collect error. Its actual whole-call pre-entry barrier
reconstructs and asserts the saved hashes of **all 399 prior full receipts**.
Its matching cast labels and divergent custom/canonical timestamp controls
resolve ordinary boundaries that the original all-empty dtype controls could
not discriminate. No original call was replayed or expected outcome rewritten.

Both entire banks deduplicate before any entry using full rows, dtypes and
public dictionary observations; exact callable, complete typed kwargs and
parameter keys; eager/lazy input kind and invocation shape. Immediate pre-entry
and post-call checks find no source/category or keyword/control mutation,
duplicate, prior overlap or opaque implementation hash change. The root
coordinator alone executed each frozen observer once. Public unoptimized plans
and plan-observer warnings/errors are recorded separately from the single
actual result collection.

The offline generator preserves **411 outcomes: 142 exact complete projected
frames and 269 full diagnostic types/phases/messages** (258 apply, 11 collect).
Individual noninline frame/case functions bound MSVC stack use. All 30 public
input/result categorical snapshots retain independent context metadata and
observer warnings. The 176 algorithm warning observations are the public
`str.concat` deprecation; the native frame/error API does not emit Python
warnings. The first coordinated build succeeds and 33 of 34 suites pass; the
Polars suite exposes a missed top-k statistics parameter-precedence diagnostic.
Before repair, exact helper/header/driver/vector snapshots, first configure/
build/failed CTest receipts and compiled identities are preserved in
`polars-variants-filter-acceptance/initial-native`. The source-only general
repair separates statistics insertion-order resolution from final membership
enum-preferred resolution. The header, driver and expected vectors remain
byte-identical. No reference call or expected-data change is used to repair it.
The second successful build exposes the later eager missing-column formatting
branch in its direct test. Its helper snapshot, full mismatch receipt and
compiled identities are preserved separately before a second source-only
repair: eager sort errors omit unresolved lazy plans, and eager activity errors
refer to the materialized group-by frame. Lazy top-k statistics remain lazy
plans even though their failure phase is apply. Both repairs preserve the
original header, driver and expected vectors byte for byte. The third build
passes all 411 outcomes and all 34 assertion-enabled suites in
`build/parity-closures/native-current/ctest-variants-final.log`. Production
acceptance passes 50 actual HTTP checks in `variants-production-http-1` and the
owned server exits. Frontend acceptance passes 186 files / 9,755 tests, 38 final
focused tests, TypeScript and Vite. Complete current sources, compiled identities,
frozen recipes, public receipts, both earlier failures and final acceptance are
retained in `docs/polars-variants-filter-checks.jsonl.gz`.

Finite observations do not establish every dtype/value/selector cross-product,
all timestamp units/timezone identities, arbitrary regex or multi-column
selectors, nested extension objects, all mixed-type diagnostic comparison orders
on large frequency tables, internal-name collisions beyond captured ordinary
columns, physical execution plans, streaming behavior, warning emission or
Python/categorical object identity. General frequency counting uses cached
variant keys and valid large tuple sorts avoid quadratic insertion work; no
performance gate is inferred from a short regression run. The complete canonical
module remains partial.
