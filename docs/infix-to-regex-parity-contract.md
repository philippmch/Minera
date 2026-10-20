# Infix-to-regex support module contract

The completion target is `algo.filtering.common.traces.infix_to_regex`, pinned
to PM4Py 2.7.22 at commit `0715c31f3f1d85924107d6aa5329806db69df899`.
Its module-owned public callable is `translate_infix_to_regex(infix)`. Public
metadata also exposes imported Python-standard-library `re.escape(pattern)`;
that component supplies literal escaping and is not a separate process-mining
algorithm. No implementation, AST, bytecode or private state was inspected.

The native public entry point is `minera::translate_infix_to_regex` in
`include/minera/infix_to_regex.hpp`. It accepts an ordered vector of exact
activity strings and returns the corresponding regex text. The exact token
`...` is special; significant spaces and all other labels are literal. Empty
vectors and empty labels are valid. This adds the missing functional string
conversion without substituting an activity-vector matcher for its observable
output. Existing log-filter parsing/matching is a separate surface with its own
input validation and is not promoted by this support-module proof.

## Observed conversion contract

The conversion starts with `^` and finishes with `$`. Each nonfinal literal
contributes its escaped text followed by a comma; the final literal contributes
only its escaped text. A nonfinal wildcard contributes `([^,]*,)*`. A final
wildcard removes the preceding output character and contributes `(,[^,]*)*`.
That last operation preserves observable edge behavior rather than simplifying
the generated expression:

| Input | Returned regex |
| --- | --- |
| empty vector | `^$` |
| `A` | `^A$` |
| `A, B` | `^A,B$` |
| `..., A` | `^([^,]*,)*A$` |
| `A, ...` | `^A(,[^,]*)*$` |
| `A, ..., B` | `^A,([^,]*,)*B$` |
| lone `...` | `(,[^,]*)*$` |
| two `...` tokens | `^([^,]*,)(,[^,]*)*$` |

In particular, a lone wildcard is unanchored at the start, and adjacent trailing
wildcards do not have the output of a normalized wildcard matcher. Commas
inside a literal remain commas, including their ambiguity in comma-joined
trace strings. The native converter preserves that returned text rather than
introducing a new escaping or normalization convention.

Literal escaping follows the observed Python 3.11 regex behavior for all 128
ASCII codepoints. Regex metacharacters, `&`, `~`, `#`, spaces and the specified
ASCII whitespace controls receive a backslash. Other characters, including NUL,
Unicode labels and the observed non-ASCII spaces, are retained. Input strings
are neither trimmed nor split. Native raw invalid UTF-8/WTF-8 byte preservation
is checked separately as a native extension; the public observations use Python
Unicode strings.

## Evidence and focused verification

Root executed one finite bank of **34 actual public conversions**, with 34
returned strings, zero errors and zero input mutations. Every record retains
ordered before/after input, the exact returned string and its UTF-8
surrogate-pass bytes. The cases cover wildcard positions and repetition, empty
labels, commas, significant spaces, metacharacters, backslashes, newlines,
CRLF, NUL, Unicode, and one complete ASCII/codepoint escaping matrix.

For each returned expression, the capture also records standard-library regex
match/fullmatch/search observations on 31 fixed strings. Those are explicitly
standard-library operations, not additional PM4Py filter calls. Exact native
output-byte equality establishes the same returned expression; it does not
claim equivalence between Python regex semantics and another regex engine.

Root verified the one module's installed bytes against its opaque pinned Git
blob, permitting only CRLF normalization. The imported standard-library `re`
module is identified by the recorded interpreter/runtime version and is not
misrepresented as a PM4Py Git blob. No source text was decoded, displayed or
parsed in the opaque verification.

The compact archive `infix-to-regex-public.json.gz` preserves the original
capture, root-only capture and pin scripts, and opaque pin receipt. Each member
retains its byte count, SHA-256 and original UTF-8 text for exact reconstruction.
The archive contains no reference implementation text. Archive SHA-256:
`1949eceec5193e16a495f3c70f3d9ec7b184e81a0f346ef9b53541be5fc42050` (`7168` bytes).

`src/backend/tests/test_infix_to_regex.cpp` embeds the exact literal inputs and
returned bytes from all 34 public calls. The module-specific
`infix_to_regex_module_acceptance` case checks the native entry point and
unchanged inputs for every record. A separate native-only check retains raw
invalid UTF-8, NUL and WTF-8 bytes without labelling it a reference observation.

A focused C++20 MSVC build of the new source/test passed with assertions
explicitly enabled (`/UNDEBUG`); its focused CTest passed. Independent source
review found no concrete issue in escaping or wildcard handling and checked
that removing a character for the final wildcard is safe. Root owns final
production registration and structural coverage verification. The supported
semantic scope is this deterministic sequence-to-text helper; no timestamp,
trace-filter, dataframe or broader filtering completion is inferred from it.
