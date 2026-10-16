# Functionality inventory

The pinned catalogue contains 86 algorithm families and 387 modules. The Feature60 source audit found existing application surfaces for 71 families and specific missing inputs, operations or controls in 15 families. Those figures describe static availability, not full behavior or parameter coverage.

Strict parity remains 46/86 packages and 199/387 modules, with 228 incomplete coverage units. Feature delivery does not promote these strict counts.

This is the frozen inventory before the original Declare workbench. Later delivery reports record completed gaps without rewriting this historical audit. Feature61 delivers original offline Declare conformance and playout; Feature62 delivers original log-skeleton conformance and standalone diagnostics. Original footprints remain the next normative-model input feature.

## Feature60 audit findings and later deliveries

| Missing input, operation or control | Scope |
| --- | --- |
| Convert a supplied genetic causal matrix without rediscovering it | Add exact causal-matrix input and native conversion dispatch; keep matrix discovery as a separate existing operation. Preserve matrix, net and markings. |
| Expose native top-k filtering as an EventLog operation | Add native filtering dispatch, keep_cases control, full raw returned log and XES/CSV export from the actual filtered result. |
| Accept the native weighted stochastic-language EMD input | Provide an exact original language-pair input in addition to existing log-derived evaluation. Do not invent a browser transport for arbitrary C++ callbacks. |
| Play out original OCPN and object-centric causal-net models | Add exact original graph/net input, separate from the existing log-discovery operation, and retain full native model and result snapshots. |
| Preserve caller-supplied target activity whitespace and ordering | Use an exact JSON string-array transport and leave semantic validation to the native operation; distinguish omitted/empty vector from an explicit empty string element. |
| Preserve feature attribute-list literals through the server | Transport each independent native vector exactly, preserving empty elements/order/duplicates where accepted by the actual native operation. |
| Allow native explicit empty ILP boundary labels | Keep omission/default distinct from supplied empty strings. Preserve exact ordered causal_relation pairs and native integer limits without presentation rewrites. |
| Expose independent DFG precision case-id and materialization-cap options | Add exact independent option transport and report configured options. Preserve omitted/null cap versus exact size_t values. |
| Supply an independent typed reference event source for edit-distance alignment | Add original input/reference source controls and typed constructors for both sides with independent import/selectors; preserve native anti-alignment behavior. |
| Expose the separate native table BOSE drift adapter | Use the already identified bounded Table BOSE contract; preserve the existing cached-log drift operation. |
| Complete original offline Declare/skeleton models and Declare playout | Delivered in Feature61 (Declare) and Feature62 (log-skeleton). Browser verification and strict reference parity remain separate gates. |
| Accept independent original footprints for conformance | Deliver the already identified exact original-footprints input contract with native comparison dispatch, retaining discovery separately. |
| Permit independent original logs for configured ILP/genetic discovery | Extend the original-source discovery workbench with separate ILP/genetic operations, exact native options, source/import controls and complete native results. |

The remaining review also covers every selector, typed input, native option and stored result field; a mounted endpoint alone is insufficient. Existing connector acquisition, original streaming Declare, normative temporal profiles, and object-centric graph conformance are already present.

## Complete family inventory

| Family | Feature60 static classification | Strict status |
| --- | --- | --- |
| `algo.analysis.extended_marking_equation` | app_available_existing | complete |
| `algo.analysis.marking_equation` | app_available_existing | complete |
| `algo.analysis.woflan` | app_available_existing | complete |
| `algo.analysis.workflow_net` | app_available_existing | complete |
| `algo.anonymization.pripel` | app_available_existing | partial |
| `algo.anonymization.trace_variant_query` | app_available_existing | partial |
| `algo.clustering.profiles` | app_available_existing | partial |
| `algo.clustering.trace_attribute_driven` | app_available_existing | partial |
| `algo.concept_drift` | confirmed_application_gap | complete |
| `algo.conformance.alignments.decomposed` | app_available_existing | partial |
| `algo.conformance.alignments.dfg` | app_available_existing | partial |
| `algo.conformance.alignments.edit_distance` | confirmed_application_gap | complete |
| `algo.conformance.alignments.petri_net` | app_available_existing | partial |
| `algo.conformance.alignments.process_tree` | app_available_existing | partial |
| `algo.conformance.antialignments` | app_available_existing | complete |
| `algo.conformance.declare` | confirmed_application_gap | complete |
| `algo.conformance.footprints` | confirmed_application_gap | complete |
| `algo.conformance.log_skeleton` | confirmed_application_gap | complete |
| `algo.conformance.multialignments` | app_available_existing | complete |
| `algo.conformance.ocel.etot` | app_available_existing | partial |
| `algo.conformance.ocel.ocdfg` | app_available_existing | partial |
| `algo.conformance.ocel.otg` | app_available_existing | partial |
| `algo.conformance.temporal_profile` | app_available_existing | complete |
| `algo.conformance.tokenreplay` | app_available_existing | partial |
| `algo.connectors` | app_available_existing | partial |
| `algo.decision_mining` | app_available_existing | partial |
| `algo.discovery.alpha` | app_available_existing | partial |
| `algo.discovery.batches` | app_available_existing | complete |
| `algo.discovery.causal` | app_available_existing | complete |
| `algo.discovery.correlation_mining` | app_available_existing | complete |
| `algo.discovery.declare` | app_available_existing | complete |
| `algo.discovery.dfg` | app_available_existing | complete |
| `algo.discovery.footprints` | app_available_existing | complete |
| `algo.discovery.genetic` | confirmed_application_gap | partial |
| `algo.discovery.heuristics` | app_available_existing | partial |
| `algo.discovery.ilp` | confirmed_application_gap | partial |
| `algo.discovery.inductive` | app_available_existing | complete |
| `algo.discovery.log_skeleton` | app_available_existing | complete |
| `algo.discovery.minimum_self_distance` | app_available_existing | complete |
| `algo.discovery.ocel.etot` | app_available_existing | partial |
| `algo.discovery.ocel.interleavings` | app_available_existing | partial |
| `algo.discovery.ocel.link_analysis` | app_available_existing | partial |
| `algo.discovery.ocel.ocdfg` | app_available_existing | partial |
| `algo.discovery.ocel.ocpn` | app_available_existing | partial |
| `algo.discovery.ocel.otg` | app_available_existing | partial |
| `algo.discovery.ocel.saw_nets` | app_available_existing | partial |
| `algo.discovery.performance_spectrum` | app_available_existing | complete |
| `algo.discovery.powl` | app_available_existing | complete |
| `algo.discovery.temporal_profile` | app_available_existing | complete |
| `algo.discovery.transition_system` | app_available_existing | complete |
| `algo.evaluation` | app_available_existing | partial |
| `algo.evaluation.earth_mover_distance` | confirmed_application_gap | complete |
| `algo.evaluation.generalization` | app_available_existing | partial |
| `algo.evaluation.precision` | app_available_existing | partial |
| `algo.evaluation.precision.dfg` | confirmed_application_gap | complete |
| `algo.evaluation.replay_fitness` | app_available_existing | partial |
| `algo.evaluation.simplicity` | app_available_existing | partial |
| `algo.label_splitting` | app_available_existing | partial |
| `algo.merging.case_relations` | app_available_existing | partial |
| `algo.organizational_mining.local_diagnostics` | app_available_existing | complete |
| `algo.organizational_mining.network_analysis` | app_available_existing | complete |
| `algo.organizational_mining.resource_profiles` | app_available_existing | complete |
| `algo.organizational_mining.roles` | app_available_existing | complete |
| `algo.organizational_mining.roles.common` | app_available_existing | complete |
| `algo.organizational_mining.sna` | app_available_existing | complete |
| `algo.querying.llm.injection` | app_available_existing | partial |
| `algo.querying.llm.injection.db_knowledge` | app_available_existing | partial |
| `algo.querying.llm.injection.pm_knowledge` | app_available_existing | partial |
| `algo.simulation.montecarlo` | app_available_existing | partial |
| `algo.simulation.playout.declare` | confirmed_application_gap | complete |
| `algo.simulation.playout.dfg` | app_available_existing | complete |
| `algo.simulation.playout.oc_causal_net` | confirmed_application_gap | partial |
| `algo.simulation.playout.ocpn` | confirmed_application_gap | partial |
| `algo.simulation.playout.petri_net` | app_available_existing | complete |
| `algo.simulation.playout.process_tree` | app_available_existing | complete |
| `algo.simulation.tree_generator` | app_available_existing | complete |
| `algo.transformation.log_to_features` | confirmed_application_gap | partial |
| `algo.transformation.log_to_interval_tree` | app_available_existing | complete |
| `algo.transformation.log_to_target` | confirmed_application_gap | complete |
| `algo.transformation.log_to_trie` | app_available_existing | complete |
| `algo.transformation.ocel.description` | app_available_existing | complete |
| `algo.transformation.ocel.features.events` | app_available_existing | complete |
| `algo.transformation.ocel.features.events_objects` | app_available_existing | complete |
| `algo.transformation.ocel.features.objects` | app_available_existing | complete |
| `algo.transformation.ocel.split_ocel` | app_available_existing | complete |
| `algo.transformation.to_embeddings` | confirmed_application_gap | partial |

[Full 86-family / 387-module inventory with source anchors](feature-first-functionality-inventory-feature60.json). The JSON retains its original source hashes and verification limits; those hashes refer to the audited snapshot.
