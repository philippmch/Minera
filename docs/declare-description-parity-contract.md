# Ordered DECLARE description parity contract

The completion target is `algo.querying.llm.abstractions.declare_to_descr` for
PM4Py 2.7.22, pinned to `0715c31f3f1d85924107d6aa5329806db69df899`.
The native entry point is `abstract_declare(const OrderedDeclareModel&, bool
include_header=true)` in `original_model_context.hpp`. The existing original-model
context operation uses this ordered input for supplied DECLARE models.

## Input and observable behavior

The normalized input is an ordered dictionary of template families, each containing
ordered unary activity keys or activity pairs. Native `OrderedDeclareFamily`
retains the family key and complete ordered `DeclareConstraint` records, including
inactive second labels, binary flags, support and confidence. Formatting reads
the activity keys; it does not filter or rank rules using their metrics.
String labels remain literal unary text or Python-style represented pairs.
The existing owned Unicode table handles pair quoting and nonprinting characters.

Family and rule insertion order are both observable. An explicitly supplied empty
known family still contributes its `Family: ` line. All eighteen display labels
and the complete explanatory header match the retained public output, including
`Responded Existence` with a capital E. An unknown family produces a key lookup
error: native C++ reports `std::out_of_range` with the captured Python KeyError's
quoted key message. No description is fabricated on that error.

Empty models, omitted/true/false headers, empty labels, quotes, embedded NUL,
newlines, nonprinting Unicode, lone surrogates and long labels are retained.
There is no text budget or truncation. The native result retains complete bytes,
character count and `truncated=false`.

The shared mined `DeclareModel` layout is unchanged. Its existing map-based
`abstract_declare` overload keeps deterministic map ordering and compatibility
formatting, including its existing empty-family and unknown-family behavior.
Cached discovery/context workflows are unchanged. Ordered supplied-model behavior
is an additional meaningful formatter contract, not a replacement of mined-model
storage or an isolated constructor experiment.

## Actual original-model route

`parse_original_context_ordered_declare_json` accepts the existing `rules`
entry-array form and legacy object form. The private JSON reader retains object
key insertion order; entry arrays already carry it. Duplicate decoded family keys,
including byte-carrier aliases, still fail instead of silently overwriting data.
The existing five native fields of every rule remain required and validated.

The original-model context operation invokes the ordered formatter. Its actual
source/effective before-and-after snapshots and canonical model artifacts contain
the ordered `rules` entry array, rather than a sorted projection of a different
argument. Canonical artifacts can be read again without losing order or metadata.
The existing wire type label `DeclareModel` and model-file schema remain compatible.
Original input bytes and complete owned errors remain available; no frontend
control or additional HTTP endpoint is required.

## Observations, tests and reproduction

Root executed twelve new public calls once: eleven returned complete descriptions
and one produced the expected unknown-family KeyError. No input mutated. The bank
covers default/header/empty behavior, reversed family and rule order, all eighteen
template labels, all eighteen explicitly empty families, literal unary/pair keys,
an unknown family and a 12,001-character activity label.

`declare_public_byte_vectors` embeds all twelve public outcomes. It compares the
ordered native formatter against every complete captured byte string and Python
character count, or the exact key-error message. Each case also goes through the
actual `execute_original_model_context` implementation, checking result artifacts,
original bytes and genuinely ordered, unchanged snapshots. The twelve reference
calls are not repeated by these native comparisons.

`declare_ordered_context_contract` separately verifies legacy JSON object order,
canonical reentry, signed integer metadata bounds, empty families, decoded-key
duplicates, missing fields, unchanged mined/map behavior and the existing response
type label. Existing byte-rich rules remain usable for known templates; unknown
templates now retain a truthful owned error instead of the previous fallback text.

Root freshly compiled both `original_model_context.cpp` and
`original_model_context_workbench_json.cpp`, then passed **1/1** assertion-enabled
CTest for the complete existing model-context suite. This includes the new DECLARE
checks and the previous Petri, temporal, locale/rounding, constructor and owned-error
regressions. Actual logs are `build/parity-closures/native-current/build-declare.log`
and `ctest-declare.log`. This is a focused integration result, not a fresh execution
of every native suite; no provider or browser calls were involved.

`declare-description-public.json.gz` retains the complete capture, own recipe and
snapshot helper, runtime packet and fresh opaque pin receipt. Each member includes
its original UTF-8 bytes, size and SHA-256. Root independently verified installed
wrapper/abstraction bytes against pinned Git blobs using only CRLF normalization;
no reference implementation text, AST, bytecode or private state was inspected.
Archive SHA-256:
`5823d5b8e9b72dd1e9ab1ce36fcb50338138fe86a5bfa43a411411964ca7035c`
(14,886 bytes).

The general contract follows ordered family/rule concatenation, a complete
eighteen-template display mapping and literal/Unicode-aware key rendering.
Arbitrary Python object-valued keys or metadata outside the native constructor
domain are not claimed. Native duplicate rule records are extensions to the
dictionary domain. DECLARE discovery/conformance, cached adapters, other abstraction
modules and the full catalogue retain their own statuses.
