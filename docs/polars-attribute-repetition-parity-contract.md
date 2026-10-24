# Polars attribute repetition companion: bounded parity checkpoint

`filter_polars_attribute_value_repetition` filters a native `EventTable` or
`LazyEventTable` using retained public observations of
`algo.filtering.polars.attr_value_repetition.filter`. The canonical module
remains **partial**. The checkpoint covers explicit visible frame values and
dtypes; it does not establish all Polars dtypes, categorical dictionaries or
physical lazy query plans.

## Delivered table behavior

Count rows equal to the target in the selected attribute column, group those
matches by the selected case column, and retain every complete source row in
cases whose counts satisfy both inclusive bounds. Cases with no matching row
remain excluded even when the minimum is zero. Null case identifiers do not
join back into the result. Source row order, column order and all stored cells
survive, including nonmatching rows of an eligible case.

The default selectors are `case:concept:name` and `concept:name`, with minimum
two and maximum signed int64 maximum. Explicit empty selectors remain literal
column names. Newlines, quotes and NUL bytes are retained. The caller resolves
public string/enum parameter bindings into the native options.

The observed numeric rules include exact integer queries, integer-to-float
coercion for float queries, NaN equality in Float64 values and case groups,
numeric boolean comparisons and lowercase boolean/string comparisons.
Null targets and Null columns produce no matches. The native companion keeps
signed zero and all original floating cells unchanged. Date and microsecond
datetime queries use typed native dates; temporal string queries raise the
observed diagnostic. Native timestamps are parsed exactly before comparison.

Bounds accept arbitrary `TokenInteger` values, booleans and fractional or
nonfinite doubles. Null bounds select no cases. Polars total ordering places
NaN after other numbers: a NaN lower bound selects no case and a NaN upper
bound accepts ordinary counts. Reversed bounds select no case. String bounds
raise the retained numeric-comparison diagnostic.

## Laziness, diagnostics and native boundaries

The eager overload returns an independent table. The lazy overload returns a
restartable row producer without scanning the source during construction.
The retained selector construction errors occur immediately; missing-column,
comparison and bound errors occur during materialization. The native error
carrier exposes `type`, `phase` and a length-carrying `message`. The latter is
required because a retained diagnostic plan contains a literal NUL byte.
Tests compare the complete message, including its expression and plan text.
The native producer materializes its input when collected; its execution plan
and optimizer behavior are outside the public observation contract.

Selected column dtypes cover String, Int64, UInt64, Float64, Boolean, Null,
Categorical, Date, and Datetime with microsecond precision, naive or UTC.
UInt32 metadata columns are retained. Source rows, declared dtypes, attributes
and native metadata are copied independently. Native tables require unique
string column labels and declared dtypes, and reject dataframe index metadata.

Categorical visible values and the dtype label were observed, but dictionaries,
unused categories, ordering and physical identity were not. Other extension
or nested dtypes, Object/List/Array/Struct/Decimal values, Duration/Time,
nanosecond datetimes, other timezone identities, arbitrary Python objects and
complete diagnostic suggestion heuristics remain outside this checkpoint.
Native categorical metadata can travel unchanged without establishing a
reference dictionary comparison. These boundaries keep the module partial.

The domain-bank continuation in
[attribute-repetition-domain-parity.md](attribute-repetition-domain-parity.md)
supersedes the boundary paragraphs above where they differ.

## Retained observations and native acceptance

Root executed two disjoint banks against PM4Py 2.7.22 and Polars 1.44.1. The
first bank at `build/filtering-attribute-repetition-polars-public-actual-1`
contains 100 distinct calls, 82 full returned frames and 18 errors. The
second at
`build/filtering-attribute-repetition-polars-literal-public-actual-1`
contains five distinct successful calls covering actual empty and literal
newline/NUL column names. The first bank's initially unnamed Series became
`column_0`/`column_1`; those retained rows are missing-column errors. The
second bank constructs genuinely empty labels through the public rename API,
with fresh source metadata and call identities.

Both banks fingerprint complete typed inputs, targets, parameters and actual
invocation shapes before algorithm entry, record durable call intents and
observe apply and collect separately. All 105 calls were executed once, with
no duplicates, prior overlaps or input/target/parameter mutations. The opaque
module hash stayed unchanged. No reference source, AST, bytecode or private
state was inspected, and no retained algorithm call was replayed.

The offline generator embeds all 87 complete returned native projections and
18 full diagnostics. Independent native controls also check metadata,
restartable lazy collection, source preservation and zero-match exclusion.
The initial MSVC test executable exhausted its default stack before producing
a result (`0xC00000FD`). The initial CTest failure and a direct native exit
receipt are retained. Splitting generated snapshots and cases into separate
non-inlined functions addresses the test frame allocation; it changes no
production filtering logic or retained expected data.
The next native run reached the retained temporal diagnostics and exposed a
missing `failed` suffix on the first context-stack node. The general temporal
diagnostic formatter now retains that suffix for Date and both observed
Datetime dtypes. The failed native receipt is also preserved; expected data
was unchanged.

The rebuilt assertion-enabled target
`minera_polars_attribute_repetition_filter_tests` passes all 105 retained
outcomes: 87 full returned projections and 18 type/phase/full-message errors,
plus the independent native controls. The final coordinated run passes all
nine related log/table/native suites. These are native engineering checks;
the bounded dataframe observation domain keeps the canonical row partial.

`docs/polars-attribute-repetition-checks.jsonl.gz` binds the final sources,
generated vectors, recipes, both retained public banks, initial stack failure,
subsequent temporal-diagnostic mismatch and successful final build/test logs.
Every archived member has its original byte length and SHA-256. Compiled
source/test objects, executable, map and linked library identities are recorded
separately. The public runtime remains pinned to PM4Py commit
`0715c31f3f1d85924107d6aa5329806db69df899`.
