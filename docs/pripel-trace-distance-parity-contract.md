# PRIPEL encoded trace distance

Completion target: `algo.anonymization.pripel.util.trace_levenshtein` at
PM4Py 2.7.22 commit `0715c31f3f1d85924107d6aa5329806db69df899`.
The three public helpers have signatures `length(s)`, `enumerateSequence(s)`
and `trace_levenshtein(s1, s2)`, without algorithm options.

Their native counterparts are `pripel_encoded_trace_length`,
`enumerate_pripel_trace` and `pripel_trace_levenshtein`, in
`include/minera/pripel_trace_distance.hpp`. The supported semantic input is
an encoded string. Its separator is a literal **`@`**, not a comma or a
Unicode character boundary. Leading, trailing and repeated separators preserve
empty tokens. Empty input has one empty token. Enumeration starts at zero and
retains complete token strings. Distance is unit-cost insertion, deletion and
substitution over those tokens; labels compare exactly.

The implementation splits into non-owning token views and computes Levenshtein
distance with two rolling rows. Time is quadratic in token counts, with token
comparison costs; state storage is linear. The smaller token count determines
row width. There is no heuristic cutoff, normalization, random behavior or
character edit fallback. Embedded NUL and UTF-8 labels remain intact. The eager
native enumeration vector is the materialized counterpart to the public
enumeration iterator. Python arbitrary-object string coercion and errors from
passing unsupported list/Trace objects are outside this typed string API.

The existing anonymizer's event-vector distance and cutoff remain separate:
that operation preserves literal labels, including labels containing `@`.
This helper addition does not silently change its matching or privacy behavior,
or promote the PRIPEL anonymizer or trace-matcher modules.

## Observations and validation

Three retained public banks made **276 actual calls**: 89 initial surface/domain
controls, 81 delimiter controls and 106 new encoded-string controls. Successful
calls were not replayed while refining the bank. Of these, **205 returned
supported string outcomes** are embedded literally in the registered
`pripel_trace_distance_module_acceptance` case: 47 lengths, 47 enumerations and
111 distances. They include every short binary token pair, substitutions,
insertion/deletion, empty tokens, repeated separators, Unicode, whitespace,
NUL and delimiter lookalikes. Every output agrees. The other 71 calls are
retained too: 68 Python errors for unsupported carriers and three successful
non-string length controls. None is presented as a string algorithm success.

Native-only checks cover arbitrary invalid byte strings and a 150,000-byte
single token. Assertion-enabled MSVC C++20 CTest passed after a bounded peer
review corrected the row initializer's integer type to `size_t`. Expected
outputs come from actual public records, rather than the native recurrence.
Closure also follows the complete parameter-free string contract, exact split
and enumeration rules, and the general unit-cost edit recurrence; the finite
bank is not an enumeration of every possible input string.

The reference leaf's normalized Git blob identity was verified opaquely before
and after observations. Only CRLF normalization was allowed. No reference body,
AST, bytecode or private state was inspected. Public imports initially failed
on missing/incompatible optional dependencies; preserved setup logs record
those zero-algorithm-call attempts. The successful bank records PM4Py 2.7.22,
diffprivlib 0.6.6, scikit-learn 1.4.2 and the actual numeric library versions.
The isolated dependencies do not replace the application's runtime.

`pripel-trace-distance-public.json.gz` retains **23 byte-verified members**,
including all captures, own recipes, opaque metadata, dependency failures and
final build/test logs. It is 14,772 bytes; SHA-256:
`eb5fc8a392af843a1482e5144c407b6f20028e26558cf00c65a77d9809fe5fce`.
