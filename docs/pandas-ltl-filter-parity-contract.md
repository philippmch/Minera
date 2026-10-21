# Pandas LTL filters: bounded public/native contract

The dedicated `pandas_ltl_filter.hpp` companion models all four public functions
of `pm4py.algo.filtering.pandas.ltl.ltl_checker`, pinned to PM4Py 2.7.22 commit
`0715c31f3f1d85924107d6aa5329806db69df899`, pandas 3.0.6 and Python 3.11.9.
The opaque module SHA-256 is
`bef9e8995fa293b6924cadd3c2b9d0ddb27412bb0709386f9c644abfe341a264`.
The canonical module remains **partial**; these observations do not close the
repository's full parity goal. Existing convenience LTL filters remain separate.

## Rules and row order

`filter_pandas_ltl_eventually_follows` selects whole cases containing the supplied
activity sequence at strictly increasing source-row positions. Rows need not be
adjacent. A later intermediate occurrence can satisfy a timestamp window even
when an earlier occurrence fails it. Captured list, tuple, string, set and
dictionary-key iterable patterns retain their observed distinctions. Empty and
single-element patterns produce the captured IndexError.

`filter_pandas_ltl_next` uses each case's final B and final C occurrences. C
must immediately follow that B, and an A must immediately precede B in original
global source rows. A later B or C can invalidate an earlier ABC, whereas a
trailing A does not invalidate ABCA. Interleaved cases and dropped missing-role
rows do not become adjacent. B=C yields no matches because its final positions
coincide, while A=B and A=C retain their observed matches.
`filter_pandas_ltl_four_eyes` requires both activities: positive selects disjoint
resource sets and negative selects a shared resource. Both polarities require
both activities. Resource sets are built separately for captured distinct scalar
representations of an equal case ID (for example True, int 1 and float 1.0).
Their decisions are assigned in first-appearance order to normal numeric-equal
case membership, with the last representation's decision winning. A fresh
four-row True/int-1 witness distinguishes this from an OR of disjoint groups.
Ordinary repeated A/B events retain whole-resource-set semantics. `filter_pandas_ltl_different_persons` selects cases with
at least two unequal resources on the activity. Its negative polarity, and the
negative eventual/next polarities, complement matching case identities across
the original source. Empty and whitespace resource strings remain literal.

None of these functions sorts source rows by timestamp. Intermediate matching
drops missing case/activity cells and, for resource modes, missing resources.
Returned cases still contain all their original rows and columns. Missing case
IDs do not become normal matching groups. Captured numeric/Boolean equivalence,
bytes, recursive tuple and nanosecond date identities remain typed. List case
labels retain the captured unhashable diagnostic; list activity queries retain
the captured comparison-length errors.

## Timestamp activation and boundaries

Timestamp controls are active only for eventual following. The other three
functions ignore captured timestamp selectors, enable flags and boundary values.
With an absent enable flag, a nonempty boundary collection enables time checks;
an empty collection does not. Explicit false or None disables time checks even
when boundaries are supplied. Explicit true reads the timestamp column and drops
missing times even for empty boundaries, without imposing an additional time-order
constraint. Explicit None boundaries still raise their captured length TypeError
when the enable flag is false. Absence and explicit None therefore remain distinct.

Time windows are inclusive. Nanosecond comparisons preserve adjacent timestamps
and do not subtract floating epoch values. Datetime64 naive and UTC source
columns are captured. String timestamps retain subtraction errors; ordinary
numeric or object datetime columns retain the captured `.dt` errors. Additional
outer boundaries, short collections, malformed bound pairs, NaN/NA/null bounds
and mixed scalar types retain their recorded outcomes or diagnostics. Date and
integer identity arithmetic is exact; it does not rely on Windows `long double`
precision or a microsecond-only datetime key comparator.

## Parameters, full frames and evidence

Typed options carry literal case/activity/resource/timestamp selectors, positive,
an optional enable flag and optional boundaries. The parameter adapter preserves
enum-key identity; the captured enum key takes precedence over its string alias
in both insertion orders. Source frames are immutable. Complete expected frames
cover row/column order, nullable cells, signed and unsigned integers beyond 2^53,
nanosecond dates, bytes and NUL names, index labels/names/kinds/dtypes, unused
categorical values and order, and dataframe context attributes. RangeIndex
classification uses exact integer differences, including extreme int64 labels.

The root executed nine fresh banks once: 386 + 27 + 6 + 1 + 3 + 1 + 2 + 2 + 1 = **429 distinct
algorithm calls**, comprising **350 complete returned frames and 79 diagnostics**.
Every continuation reconstructed all earlier complete receipts and asserted each
saved SHA before any new algorithm entry. Whole-bank full-receipt deduplication
also preceded entry. All calls recorded warnings and source/argument snapshots;
zero warning emissions and zero source/argument mutations were observed. The
native driver contains 429 separate noinline cases and 268 separate complete
frame snapshot constructors. Oracle expectations remain the captured values.

The combined prior searches preserved complete inventories and classification
receipts: 3,897 compressed archives and 500,039 flat files. Exact-module prior
algorithm receipt counts were zero for all three engines. The compressed scan
searched the four rule identities; its audit binds the flat classification that
also covers the two LOG timestamp helper identities. A failed flat-classifier
codec attempt is retained alongside the successful classification of the same
immutable inventory. No reference implementation was inspected. Observers were
executed only by the root; offline generation and packaging make no reference
imports or algorithm calls.

`docs/pandas-ltl-filter-public.json.gz` retains all nine full public banks,
observers, input plans, intents, diagnostics, mutation/warning receipts, surface
and opaque pins, prior audits and generator. The focused native suite passed
all 429 outcomes (350 complete frames and 79 exact error type/message pairs) in
`ctest-ltl-pandas-fifth.log`, with zero aggregated failures. The authoritative
coordinated run passed all 47 suites in `ctest-ltl-final2.log`; the matching
server/CLI build passed in `build-ltl-sixth-final.log`. Two owned temporary-server
runs passed 73 LTL HTTP checks and 101 attribute-workflow regression checks, both
exited normally, and both bind server SHA-256
`be6f9e6b656cb157575554d0d2e4a11fa3984a4d935b04938dba903f3a944445`.
Final frontend checks passed 192 files / 9,829 tests, 20 focused checks, TypeScript
and Vite. These are engineering acceptance results, not an expansion of the
public semantic boundary. The initial build's optional-cell compile failure is
preserved in `build-ltl-first.log`; pre-repair header, source and 419-case driver
are retained with source hashes. The first executed 420-case driver exposed
NEXT selection behavior on merged numeric-equal case identities. Its source,
executable, map and object hashes are retained before repair. Fresh disjoint
homogeneous/mixed identity and trailing-anchor witnesses distinguish that branch.
The subsequent 424-case driver exposed Four Eyes grouping of mixed numeric-equal
case representations. Ordinary resource-set, literal string-collision and
last-wins witnesses distinguish its grouping and membership stages. Both failed
executed native source/binary-hash checkpoints are preserved. The driver now
collects all thrown case failures before failing and retains all exact table,
diagnostic, source-immutability and count gates. No previous oracle values were
rewritten. One resource-witness preparation failed before any algorithm entry
because its fixture name reached an existing domain dispatcher; its original
script, zero-call validation and error log are retained beside the corrected
preparation and successful capture.
`docs/pandas-ltl-filter-checks.jsonl.gz` binds final source, compiled objects,
executable, build/test logs and production receipts. Its JSONL acceptance record
and separately hashed members preserve current and historical outcomes.

## Remaining boundary

These finite banks do not establish arbitrary Python object or custom DataFrame
subclass behavior, every extension dtype or column-index layout, arbitrary
unordered-set iteration under other hash seeds, or all cross-type exception
precedence and timezone combinations. Mixed-type case groups with missing A/B
anchors beyond the captured witnesses remain unobserved. Public plans preserve
the actual typed arguments and captured runtime behavior; a normalized set serialization does not
promise another Python process's iteration order. No browser interaction is
claimed by this module proof. The module remains partial until further genuinely
disjoint public evidence and corresponding native/workflow acceptance justify
expanding that boundary.
