# Local diagnostics

The `LOCAL_DIAGNOSTICS` family provides four deterministic maps for bounded
organizational analysis: relative focus (group/activity divided by the global
activity total), relative stake (group/activity divided by that group's event
total), coverage (group/resource divided by the group's event total), and raw
member contributions. Maps are sorted by key in native and JSON output.

By default groups are read from the generic event attribute `org:group`, while
resource and activity use the first-class event fields. The three keys are
configurable. Explicit role/clustering mappings can be supplied through the
wire grammar `group:resource,resource;group2:resource` (duplicate members are
deduplicated). Events with blank group, resource, or activity are skipped.

Safety limits reject oversized group/resource sets and cap every emitted map
leaf through one deterministic row budget; zero-denominator ratios are emitted
as finite `0`. The native API is
`discover_local_diagnostics`, with adapters at `/api/local-diagnostics` and
`minera_export --local-diagnostics`.

Remaining limitation: explicit groups are resource membership only; dynamic
membership by case/trace attributes is not inferred.
