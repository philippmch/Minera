# Temporal-profile description parity contract

The completion target is `algo.querying.llm.abstractions.tempprofile_to_descr`
for PM4Py 2.7.22, pinned to commit
`0715c31f3f1d85924107d6aa5329806db69df899`. The native public entry point is
`abstract_temporal_profile(const TemporalProfile&, bool include_header=true)`
in `include/minera/original_model_context.hpp`.

The normalized public input is an insertion-ordered mapping from string activity
pairs to binary64 mean/standard-deviation pairs. Native `TemporalProfile.entries`
retains that order; its extra count field has no description counterpart and is
ignored. Each entry interpolates both labels literally and formats both numbers
in fixed notation with two decimal places. There is no sorting, label escaping,
character budget or output cap. The result preserves exact text, its character
count and `truncated=false`.

## Observations and corrections

Root executed seven named public `abstract_temporal_profile` calls once, with
seven returns, no exceptions and no input mutations. The corpus covers omitted,
true and false header controls; empty models; insertion order; empty, NUL,
newline, Unicode and lone-surrogate labels; nearest-even ties and adjacent
representable values; decimal rounding traps; subnormals; minimum normal and
maximum finite numbers; signed zero, infinities and both NaN signs.

The empty model exposed a concrete mismatch. Without a header, its public output
is exactly one newline; with a header, the explanatory paragraph ends in one
newline. Native code previously emitted an extra newline in both empty cases.
Nonempty descriptions retain the two-newline prefix or header separator and
have no trailing newline.

The previous number formatter used the process locale and floating-point
rounding mode. `std::to_chars` now gives locale-independent, nearest-even
binary64 fixed-point text without changing either process setting. Negative
zero remains `-0.00`, infinities retain their signs, and NaN renders as `nan`
regardless of its sign. The buffer accommodates every finite binary64 value;
maximum magnitudes are not shortened or converted to exponent notation.

`temporal_public_byte_vectors` in the existing native test embeds every complete
captured output, reconstructed only from public argument/result records. It
checks all seven texts byte for byte, their Python character counts and the
absence of truncation. `temporal_numeric_environment` separately checks exact
output under a comma/grouping locale and all four floating-point rounding modes,
including unchanged input storage and caller rounding mode. Existing tests
continue to cover descriptions exceeding 2 MiB and native byte-string storage.

## Reproducibility and actual verification

`temporal-description-public.json.gz` contains the complete seven-call capture,
the root-only capture recipe, its retained runtime packet, the historical pin
receipt and the fresh root pin receipt. Each member records its original byte
count and SHA-256 and can be reconstructed by UTF-8 encoding its `utf8` member.
The capture retains every returned byte in `result.hex`; no reference
implementation text is included. Archive SHA-256:
`f3d1958189c3573da6c900edc527b35f43522fbe7a5c8561ceb1639e3c5ce53c`
(7,881 bytes).

Root independently compared opaque installed wrapper and abstraction-module
bytes against the pinned Git blobs, allowing only CRLF normalization. The leaf
also matches the retained Feature92 opaque hash. No reference source text, AST,
bytecode or private state was decoded, displayed or inspected.

The actual focused MSVC build compiled the changed temporal implementation and
linked it ahead of the validated Feature99 core archive. Its link map binds the
current implementation. All seven assertion-enabled CTest suites passed,
including `minera_original_model_context_tests`, which executes the new byte
comparisons and locale/rounding regressions together with the existing model
workbench checks. The build recipe, map and logs remain under
`build/parity-closures/native-current` and `build/parity-closures/native-tests.log`.
This is a focused integration result, not a fresh execution of every native
suite. No provider or browser calls were involved.

The finite records support the general formatter contract: ordered concatenation
preserves all entries and literal labels, while fixed binary64 conversion
determines every numeric field independently of locale and rounding state.
Arbitrary malformed Python containers, non-string objects and non-bool truthiness
are outside the annotated typed contract. Native-only duplicate pairs, count
fields and invalid UTF-8 byte strings are extensions, not additional Python
mapping obligations. Cached discovery-and-description workflows, the other
eleven abstraction modules and the complete catalogue retain their own statuses.
