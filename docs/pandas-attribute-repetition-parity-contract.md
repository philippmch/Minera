# Pandas attribute repetition companion: bounded parity checkpoint

`filter_pandas_attribute_value_repetition` filters a native `EventTable` using
the observed public pandas repetition behavior. The canonical module
`algo.filtering.pandas.attr_value_repetition.filter` remains **partial**.
This checkpoint covers the retained scalar/datetime table domain; it does not
claim every pandas object, dtype or column-index behavior.

## Delivered table behavior

Find rows equal to the supplied target in the selected attribute column,
count those rows by their selected case value, and retain complete source
rows belonging to cases whose counts satisfy both inclusive bounds. Only
cases observed among matching rows are eligible: a minimum of zero does not
introduce cases with no matching value. Missing case values are excluded.
Result rows retain their original order, including nonmatching rows in an
eligible case.

Numeric bool/integer/real equality retains exact integer comparisons around
2^53. Null, numeric NaN, typed NaT and pandas.NA targets do not match cells.
The object-dtype source distinguishes typed datetimes from literal strings.
Declared datetime columns also accept parseable string queries, preserving
timezone awareness and exact nanoseconds. Native date resolution and zone
metadata travel with the stored cells. A list target acts as a row-aligned
comparison vector and raises the observed length error when its size differs
from the table. Captured tuple/dictionary queries against scalar cells do not
match.

Signed integer and fractional/nonfinite bounds preserve their observed
behavior. Null bounds select no cases; numeric NaN comparisons are false.
Reversed bounds return empty results. String bounds raise the captured
vectorized comparison error. Missing attribute columns are checked before
case grouping; literal empty names remain distinct from omitted selectors.
Enum parameter bindings and their precedence are resolved from the supplied
public parameters by the caller.

Group keys preserve scalar numeric equality (including bool/int/real), literal
strings and exact typed datetimes. Matching rows with mutable list/dictionary
case values raise the captured unhashable-key errors. Other arbitrary Python
object/hash behavior is outside this checkpoint.

## Metadata and native boundaries

The returned table keeps every stored cell in each retained row, source column
order, declared dtypes, categorical category order/unused categories/ordered
flag, dataframe attributes and native metadata. Source index labels and names
survive, including duplicate labels and MultiIndex tuples. A filtered
RangeIndex retains that kind when its labels form an arithmetic progression;
otherwise it becomes an ordinary Index. The source stays unchanged and result
storage is independent. Additional native controls retain inactive metadata,
private synchronization baselines, arbitrary byte strings, NUL and adjacent
nanosecond timestamps.

Native tables require unique string column labels. They do not carry a
dataframe column-index name, arbitrary pandas extension objects, or Python
integers outside the native signed/unsigned scalar carrier. All captured
column-index names are null. The finite bank covers object, float64, nullable
Int64/boolean/string, categorical and naive/UTC nanosecond datetime columns.
It does not establish all extension dtypes, nonstring/duplicate column labels,
all timezone identities, or mutable-versus-mutable selected-cell comparisons.
The latter comparison domain currently rejects explicitly. These boundaries
keep the canonical pandas row partial even though every retained native
projection and representable diagnostic passes.

The domain-bank continuation in
[attribute-repetition-domain-parity.md](attribute-repetition-domain-parity.md)
supersedes the boundary paragraphs above where they differ.

## Retained public observations and native acceptance

Root attempted planning once at
`build/filtering-attribute-repetition-pandas-public-actual-1`. The observer
rejected a NumPy boolean while snapshotting a nullable-boolean column, before
any algorithm entry: **zero public algorithm calls**. The original observer,
failure receipt and hashes are preserved. A separate context note attributes
the original error description to root's retained tool output; no disk stderr
or stack-trace log is claimed. Boolean/string NumPy scalars were
then normalized explicitly in the observer, with existing integral/real
normalization retained.

Before any algorithm entry in the successful run, the complete source frame,
typed target, supplied public parameters and actual invocation shape were
fingerprinted for every candidate. One duplicate candidate, the zero/zero
range, was removed before entry. The successful bank at
`build/filtering-attribute-repetition-pandas-public-actual-2` contains **140
distinct explicit calls**, **123 full returned tables** and **17 errors**,
with no prior-bank overlap and no source/target/parameter mutations. Each
successful retained call was executed once. The earlier log-module bank is
separate by callable and source carrier.

`test_pandas_attribute_repetition_module.cpp` embeds all **123 complete native
table projections** and **16 representable diagnostic counterparts** directly
from those retained observations, without replacing fields with expected
values. The missing-required-argument error lies outside the native signature.
The assertion-enabled fresh-source target
`minera_pandas_attribute_repetition_module_tests` passes all **139 native
outcomes**, plus independent native storage/nanosecond controls.

`docs/pandas-attribute-repetition-checks.jsonl.gz` retains the failed zero-entry
observer, successful bank, full receipt dedup audit, public interface and
opaque identities, generation recipes, source/test snapshots, build/test logs
and compiled source/test/object/binary/link bindings. Each member has a byte
length and SHA-256. The opaque module identity remains pinned to PM4Py 2.7.22
commit `0715c31f3f1d85924107d6aa5329806db69df899`. No reference implementation,
AST, bytecode or private state was inspected.


## Literal missing-column diagnostic continuation

A fresh, separately fingerprinted bank at
`build/filtering-attribute-repetition-pandas-literal-errors-public-actual-1`
adds **10 distinct calls**, all observed `KeyError` diagnostics, with no overlap
with the prior 140 calls and no input/target/parameter mutations. Seven missing
attribute names and three missing case names cover literal empty strings,
embedded NUL, newline, backslash, single quote, double quote and both quotes.
All observations were executed once; no prior calls were rerun.

The native diagnostic now uses the observed Python string representation:
control bytes are escaped, backslashes are doubled and quote choice avoids an
unnecessary escape when possible. The error carrier retains a full-length
`message` string independently of `what()`, so a caller can preserve complete
diagnostics containing embedded NUL. The new test
`test_pandas_attribute_repetition_literal_errors.cpp` compares all ten complete
messages and error kinds and verifies unchanged full source tables. Its offline
generator and public/native acceptance are preserved in the separate archive
`docs/attribute-repetition-ordinary-checks.jsonl.gz`. The original pandas archive
above remains historical. The assertion-enabled new diagnostic suite passes
**10/10**, and the original pandas suite still passes all **139** native
outcomes. The first combined run includes a separate Polars diagnostic failure;
acceptance here refers to the named pandas suites. The final combined run
passes **9/9**, and both build/test attempts remain in the evidence. Final
acceptance binds the accepted objects and binaries. This diagnostic
continuation does not widen dtype,
column-index or custom-object parity claims.
