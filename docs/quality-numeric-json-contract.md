# Quality JSON numeric preservation

The shared quality-result JSON number writer now emits shortest round-trip
binary64 text. Previously its stream's default six significant digits changed
values such as `1 / 3` from the computed double to `0.333333`. Finite values now
parse back to exactly the original double. Negative zero is `-0.0`; nonfinite
values retain the existing JSON `null` policy. Output is independent of the
stream's decimal locale and precision.

This affects replay-fitness, precision, generalization, simplicity, composite
scores/weights, EMD, and the other fields using `write_finite_or_null`. Existing
field names, result structures and computed/error flags remain intact. It does
not assert exact Python JSON byte formatting or promote an algorithm module.
Other numeric writers retain their individual contracts.

`minera_quality_json_numbers_tests` reads the actual exported numeric tokens
with the independent standard numeric parser and compares double bits. Its
13-value bank includes thirds, adjacent doubles, signed zero, ordinary decimals,
very small/large numbers, minimum subnormal/normal, maximum finite and all
nonfinite categories. It covers six result routes, nested composite weights,
and both classic and comma-decimal process locales.

The regression failed against the original writer on the first exact numeric
comparison. After the production fix it passed with assertions enabled.
An initial test-only composite field lookup used internal field names instead
of the existing nested `weights` JSON object; that assertion was corrected
without changing production behavior. Core, evaluation and full evaluation
contract suites passed against the fresh JSON object linked before the validated
baseline library. Logs remain under `build/parity-closures/quality-numbers`.
