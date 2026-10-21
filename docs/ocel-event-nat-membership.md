# Exact OCEL event NaT membership

`OcelAttributeNaT` extends the exact event membership query carrier with a
typed missing datetime. Within a datetime-valued column it selects NaT cells;
null and numeric NaN queries do not. Both polarities use the same stable
event-based six-table propagation as the existing exact companion. Ordinary
scalar columns retain their literal/null/numeric-NaN distinctions.

Root executed a new finite bank exactly once at
`build/ocel-event-attributes-nat-membership-public-actual-1`. All **12 calls**
returned independent logs without changing inputs, query collections, bounds
or parameters. The bank crosses canonical and generic datetime columns,
NaT/null/numeric-NaN queries and both polarities. The canonical NaT query
selects e1/e3; the generic nullable datetime query selects e0/e2. Null and
numeric NaN positive queries select no events in either datetime column.

`test_ocel_event_nat_membership.cpp` embeds every full ordered native result
directly from the actual snapshots, including unchanged source checks. Its
target is `minera_ocel_event_nat_membership_tests`; `main` directly invokes
`retained_public_event_nat_membership`. All **12/12 full results** passed in
the fresh assertion-enabled native build. Existing categorical, timestamp and
diagnostic acceptance remains in the separate original event module suite.

The native datetime storage represents missing time as `"NaT"` (and accepts
null/numeric NaN missing cells when the column kind explicitly identifies a
datetime). It does not infer a datetime column from a date-looking string.
General stored `OcelValue` payloads and native dataframe projection boundaries
remain as documented in the original event attribute contract.

`docs/ocel-event-nat-membership-checks.jsonl.gz` retains the new observer,
durable call intents, complete captures, source/test snapshots and native
verification bindings, with per-member byte lengths and SHA-256 identities.
Pinned opaque public module identities match PM4Py 2.7.22 commit
`0715c31f3f1d85924107d6aa5329806db69df899`. No reference implementation,
AST, bytecode or private state was inspected, and no captured call was replayed.
