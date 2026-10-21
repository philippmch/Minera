# OCEL graph numeric JSON transport

ETOT, OTG and OCDFG conformance JSON now use the classic locale for the entire
response. Decimal commas and grouped thousands in the process locale cannot
change floating values or signed integer differences into different JSON
tokens. Finite scalar doubles retain enough digits for exact binary64
roundtrip. Negative zero is written as `-0.0`, preserving its floating sign in
readers that distinguish integer and real tokens. Existing nonfinite
conformance scalar values continue to serialize as JSON `null`.

Annotated OCPN output also uses the classic locale. Its actual embedded OCDFG
writer, `write_ocpn_annotation_fields`, preserves negative zero in the legacy,
event-couple and total-object performance arrays. This repairs the separate
annotation path; the standalone discovery serializer already preserves these
properties. OCPN graph array acceptance covers finite performance samples.
This change does not revise the existing nonfinite graph-array policy.

The focused assertion-enabled test is
`minera_ocel_conformance_json_numbers_tests`, from
`src/backend/tests/test_ocel_conformance_json_numbers.cpp`. It verifies:

- Fourteen scalar states across seven conformance fields and two locales,
  including adjacent binary64 values, signed zero, subnormal and extreme
  finite values, infinities and NaN.
- Exact signed 64-bit minimum/maximum values and large negative differences,
  with decimal-comma and thousands-grouping locale facets active.
- All three embedded performance arrays parsed through the native OCDFG
  reader and compared bitwise with their actual source samples and the
  standalone graph projection. Different arrays prevent projection mixups.
- Locale-independent OCPN diagnostic counts and disabled annotations.

These are native transport checks, without new reference calls or reference
implementation inspection. They do not change mining/conformance algorithms,
close the wider OCDFG parity gate or establish rendered application behavior.

Fresh-source validation passed the focused numeric suite, the original OCEL
core suite and the existing OCDFG timing suite: **3/3 CTest targets passed**.
Logs are `build/parity-closures/native-current/build-numeric-json.log` and
`ctest-numeric-json.log`. The compressed evidence
`docs/ocel-numeric-json-checks.json.gz` retains the source/test and project
snapshots, assertion configuration, complete logs and SHA-256 bindings for
the fresh source object, test object, executable and link map. The server/CLI
rebuild and rendered application interactions are separate validation steps.
