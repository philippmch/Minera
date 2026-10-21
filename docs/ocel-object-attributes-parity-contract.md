# OCEL object attribute filtering parity contract

The public module `algo.filtering.ocel.object_attributes` has one filtering
callable, `apply`. Its native companion is
`filter_ocel_object_attribute_exact`; the existing convenience function keeps
its previous contract. Full catalogue parity remains a separate objective.

## Selection

The exact companion selects objects by membership in a supplied collection,
or its complement when positive is false. Canonical object ID and object type
selectors resolve the corresponding native fields. Other names resolve
literal attributes, including an explicitly declared empty or NUL-containing
name. An absent column raises `std::out_of_range` with its quoted name.
Declared empty columns and absent declarations remain distinct.

The scalar predicate reuses the event companion's exact numeric comparison.
Bool, signed integer and real values compare numerically without rounding
signed integers through double; this includes adjacent integers around 2^53.
Strings remain distinct from numbers. Explicit null and numeric NaN remain
distinct, and a numeric NaN query selects numeric NaN cells. A missing scalar
cell in a declared column behaves as numeric NaN.

`OcelObjectAttributeFilterValue` and `OcelObjectAttributeColumnKind` share the
event companion's carrier and column-kind contract. Typed datetime queries
compare exact seconds/nanoseconds and timezone awareness; literal date-looking
strings remain strings. Explicit timestamp/scalar kinds override automatic
kind resolution. Automatic object datetime kinds use the selected object's
type declarations. The canonical object ID/type fields retain scalar kinds
unless the caller explicitly supplies a timestamp kind.

The shared `OcelAttributeNaT` query sentinel, also exposed as
`OcelObjectAttributeNaT`, selects missing cells in a datetime-valued column.
Null and numeric NaN queries do not select those cells. The native datetime
column representation retains `"NaT"` for a missing datetime; null or numeric
NaN stored in an explicitly datetime-valued native cell also represent a
missing datetime. A scalar string `"NaT"` remains a literal string. This does
not extend the general stored `OcelValue` domain or infer datetime kinds from
string contents.

## Propagation and storage

Keep selected objects in source order, including selected objects with no
event relation. Retain their relation rows in original order and multiplicity,
then retain the events participating in those relations. Retain O2O/E2E rows
when both endpoints remain, and object changes when their object remains.
An empty object selection empties all six tables. Preserve metadata, type
catalogues and optional table-column declarations, and return independent
storage. Selected unused objects, their changes and their remaining O2O links
are preserved even when no event survives.

The native state controls include duplicate relation and graph rows, duplicate
attributes, negative zero, arbitrary byte strings, NUL/WTF-8 names, unused
declarations, explicit empty schemas, both column-kind overrides and result
mutation without source mutation. The supported typed log domain has coherent
identities and table references.

## Retained evidence and acceptance

Root executed the finite object bank exactly once at
`build/ocel-object-attributes-focused-public-actual-1`: **201 public calls**,
**195 returned logs**, and **six errors**. No input log, parameter dictionary
or value collection changed; no successful result was the input object.
The bank includes canonical and constructor-alias selectors, enum/string
parameter precedence, both polarities, collection shapes, literal selectors,
numeric and datetime domains, unused objects, empty shapes, all six table
permutations and rich metadata. The two missing-column errors have direct
native counterparts. Four Python arity/noncollection errors lie outside the
native vector signature and remain recorded in the evidence.

`test_ocel_object_attributes_module.cpp` embeds all 195 full ordered native
projections directly from returned snapshots, both representable diagnostics,
and independent native storage controls. Its assertion-enabled target is
`minera_ocel_object_attributes_module_tests`. The fresh-source suite passed
**195/195 full returned logs**, **2/2 native diagnostic counterparts** and the
independent native storage controls. The existing event suite and separate
12-case event NaT suite passed in the same current-source build. Dataframe indices, dtypes,
configuration and dataframe-only columns remain in the full public evidence;
native comparisons cover every available native storage member. Arbitrary
Python objects and integers outside signed 64-bit storage are not coerced.

The observer, case bank, actual outcomes, opaque pinned identities, generated
tests, production sources and native verification bindings are archived in
`docs/ocel-object-attributes-checks.jsonl.gz`. Each archived member retains its
byte count and SHA-256. The pin is PM4Py 2.7.22 commit
`0715c31f3f1d85924107d6aa5329806db69df899`. No reference implementation,
AST, bytecode or private state was inspected, and no captured call was
replayed. Finite acceptance checks the stable membership/propagation invariant
across its branches; it does not enumerate arbitrary log sizes.
