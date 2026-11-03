import {miningSourceHash} from "./active-mining-source";
import { backendFetch } from "./backend-fetch";
import {nativeObjectTables,type DiagnosticTable} from "./model-diagnostics";
import {nativeCell,nativeJson,parseNativeJson} from "./native-json";
import {appendNativeMultipartField,nativeStringBytes} from "./native-multipart";
import {nativeResponseJson,readNativeResponse} from "./native-response";
import {isOriginalEmbeddingFilterNativeString,isOriginalEmbeddingFilterRawLog,isOriginalEmbeddingFilterTable,originalEmbeddingFilterOriginalBytes,originalEmbeddingFilterStringBytes,originalEmbeddingFilterStringText,type OriginalEmbeddingFilterBytes,type OriginalEmbeddingFilterNativeString} from "./original-embedding-filter-workbench";
import {defaultOriginalEditDistanceControl,originalEditDistanceFileName,originalEditDistanceWireFilename,type OriginalEditDistanceControl,type OriginalEditDistanceRequestOriginal} from "./original-edit-distance-workbench";
import {isOriginalOcPlayoutNativeType} from "./original-object-centric-playout-workbench";
import {originalRootCauseEditMember,originalRootCauseRawMember} from "./original-root-cause-workbench";
export {OriginalEditDistanceRequestGate as OriginalQualityRequestGate} from "./original-edit-distance-workbench";
export type OriginalQualityNativeString=OriginalEmbeddingFilterNativeString;
export type OriginalQualityBytes=OriginalEmbeddingFilterBytes;
export type OriginalQualityControl=OriginalEditDistanceControl;
export const ORIGINAL_QUALITY_SPECS={
 "evaluate_replay_fitness": {
  "label": "Replay fitness",
  "options": "ReplayFitnessOptions",
  "result": "ReplayFitnessResult",
  "variant": "ReplayFitnessVariant"
 },
 "evaluate_precision": {
  "label": "Precision",
  "options": "PrecisionOptions",
  "result": "PrecisionResult",
  "variant": "PrecisionVariant"
 },
 "evaluate_generalization": {
  "label": "Generalization",
  "options": "GeneralizationOptions",
  "result": "GeneralizationResult",
  "variant": "GeneralizationVariant"
 },
 "evaluate_composite": {
  "label": "Composite quality",
  "options": "CompositeEvaluationOptions",
  "result": "CompositeEvaluationResult",
  "variant": null
 },
 "evaluate_simplicity": {
  "label": "Simplicity",
  "options": "SimplicityOptions",
  "result": "SimplicityResult",
  "variant": "SimplicityVariant"
 }
} as const;
export const ORIGINAL_QUALITY_DISPATCH={
 "evaluate_replay_fitness": [
  {
   "source_kind": "log",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "log",
   "model_kind": "petri_net",
   "call_form": "default_options"
  },
  {
   "source_kind": "view_log",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_stream",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_table",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_lazy",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "stream",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "table",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "lazy",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "log",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "log",
   "model_kind": "process_tree",
   "call_form": "default_options"
  },
  {
   "source_kind": "view_log",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_stream",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_table",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_lazy",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "stream",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "table",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "lazy",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "log",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_log",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_stream",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_table",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_lazy",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  }
 ],
 "evaluate_precision": [
  {
   "source_kind": "log",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "log",
   "model_kind": "petri_net",
   "call_form": "default_options"
  },
  {
   "source_kind": "view_log",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_stream",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_table",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_lazy",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "stream",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "table",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "lazy",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "log",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "log",
   "model_kind": "process_tree",
   "call_form": "default_options"
  },
  {
   "source_kind": "view_log",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_stream",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_table",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_lazy",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "stream",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "table",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "lazy",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "log",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_log",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_stream",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_table",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_lazy",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  }
 ],
 "evaluate_generalization": [
  {
   "source_kind": "log",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "log",
   "model_kind": "petri_net",
   "call_form": "default_options"
  },
  {
   "source_kind": "view_log",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_stream",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_table",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_lazy",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "stream",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "table",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "lazy",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "log",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "log",
   "model_kind": "process_tree",
   "call_form": "default_options"
  },
  {
   "source_kind": "view_log",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_stream",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_table",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_lazy",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "stream",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "table",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "lazy",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "log",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_log",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_stream",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_table",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_lazy",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  }
 ],
 "evaluate_composite": [
  {
   "source_kind": "log",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_log",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_stream",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_table",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_lazy",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "stream",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "table",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "lazy",
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": "log",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_log",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_stream",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_table",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_lazy",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "stream",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "table",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "lazy",
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": "log",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_log",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_stream",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_table",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  },
  {
   "source_kind": "view_lazy",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  },
  {
   "source_kind": "stream",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  },
  {
   "source_kind": "table",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  },
  {
   "source_kind": "lazy",
   "model_kind": "pnml_document",
   "call_form": "with_options"
  }
 ],
 "evaluate_simplicity": [
  {
   "source_kind": null,
   "model_kind": "petri_net",
   "call_form": "with_options"
  },
  {
   "source_kind": null,
   "model_kind": "process_tree",
   "call_form": "with_options"
  },
  {
   "source_kind": null,
   "model_kind": "pnml_document",
   "call_form": "with_options"
  },
  {
   "source_kind": null,
   "model_kind": "presentation",
   "call_form": "with_options"
  }
 ]
} as const;
export const ORIGINAL_QUALITY_PUBLIC_OVERLOADS=[
 {
  "function": "evaluate_replay_fitness",
  "result_type": "ReplayFitnessResult",
  "arguments": "const EventLog&, const PetriNet&, ReplayFitnessVariant = ReplayFitnessVariant::AUTO",
  "line": 336,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_replay_fitness",
  "result_type": "ReplayFitnessResult",
  "arguments": "const EventLog&, const ProcessTree&, ReplayFitnessVariant = ReplayFitnessVariant::AUTO",
  "line": 338,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_replay_fitness",
  "result_type": "ReplayFitnessResult",
  "arguments": "const EventLog&, const PetriNet&, ReplayFitnessVariant, const ReplayFitnessOptions&",
  "line": 340,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_replay_fitness",
  "result_type": "ReplayFitnessResult",
  "arguments": "const EventLog&, const ProcessTree&, ReplayFitnessVariant, const ReplayFitnessOptions&",
  "line": 343,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_replay_fitness",
  "result_type": "ReplayFitnessResult",
  "arguments": "const EventLog&, const PnmlDocument&, ReplayFitnessVariant = ReplayFitnessVariant::TOKEN_BASED, const ReplayFitnessOptions& = {}",
  "line": 346,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_replay_fitness",
  "result_type": "ReplayFitnessResult",
  "arguments": "const EventDataView&, const PnmlDocument&, ReplayFitnessVariant = ReplayFitnessVariant::TOKEN_BASED, const ReplayFitnessOptions& = {}",
  "line": 354,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_replay_fitness",
  "result_type": "ReplayFitnessResult",
  "arguments": "const EventDataView&, const PetriNet&, ReplayFitnessVariant = ReplayFitnessVariant::AUTO, const ReplayFitnessOptions& = {}",
  "line": 358,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_replay_fitness",
  "result_type": "ReplayFitnessResult",
  "arguments": "const EventDataView&, const ProcessTree&, ReplayFitnessVariant = ReplayFitnessVariant::AUTO, const ReplayFitnessOptions& = {}",
  "line": 361,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_replay_fitness",
  "result_type": "ReplayFitnessResult",
  "arguments": "const EventStream& data, const PetriNet& net, ReplayFitnessVariant variant = ReplayFitnessVariant::AUTO, const ReplayFitnessOptions& options = {}",
  "line": 365,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_replay_fitness",
  "result_type": "ReplayFitnessResult",
  "arguments": "const EventTable& data, const PetriNet& net, ReplayFitnessVariant variant = ReplayFitnessVariant::AUTO, const ReplayFitnessOptions& options = {}",
  "line": 371,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_replay_fitness",
  "result_type": "ReplayFitnessResult",
  "arguments": "const LazyEventTable& data, const PetriNet& net, ReplayFitnessVariant variant = ReplayFitnessVariant::AUTO, const ReplayFitnessOptions& options = {}",
  "line": 377,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_replay_fitness",
  "result_type": "ReplayFitnessResult",
  "arguments": "const EventStream& data, const ProcessTree& tree, ReplayFitnessVariant variant = ReplayFitnessVariant::AUTO, const ReplayFitnessOptions& options = {}",
  "line": 383,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_replay_fitness",
  "result_type": "ReplayFitnessResult",
  "arguments": "const EventTable& data, const ProcessTree& tree, ReplayFitnessVariant variant = ReplayFitnessVariant::AUTO, const ReplayFitnessOptions& options = {}",
  "line": 389,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_replay_fitness",
  "result_type": "ReplayFitnessResult",
  "arguments": "const LazyEventTable& data, const ProcessTree& tree, ReplayFitnessVariant variant = ReplayFitnessVariant::AUTO, const ReplayFitnessOptions& options = {}",
  "line": 395,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_precision",
  "result_type": "PrecisionResult",
  "arguments": "const EventLog&, const PetriNet&, PrecisionVariant = PrecisionVariant::AUTO",
  "line": 425,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_precision",
  "result_type": "PrecisionResult",
  "arguments": "const EventLog&, const PetriNet&, PrecisionVariant, const PrecisionOptions&",
  "line": 427,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_precision",
  "result_type": "PrecisionResult",
  "arguments": "const EventLog&, const PnmlDocument&, PrecisionVariant, const PrecisionOptions& = {}",
  "line": 429,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_precision",
  "result_type": "PrecisionResult",
  "arguments": "const EventDataView&, const PnmlDocument&, PrecisionVariant, const PrecisionOptions& = {}",
  "line": 432,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_precision",
  "result_type": "PrecisionResult",
  "arguments": "const EventDataView&, const PetriNet&, PrecisionVariant = PrecisionVariant::AUTO, const PrecisionOptions& = {}",
  "line": 435,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_precision",
  "result_type": "PrecisionResult",
  "arguments": "const EventDataView&, const ProcessTree&, PrecisionVariant = PrecisionVariant::AUTO, const PrecisionOptions& = {}",
  "line": 438,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_precision",
  "result_type": "PrecisionResult",
  "arguments": "const EventStream& data, const PetriNet& net, PrecisionVariant variant = PrecisionVariant::AUTO, const PrecisionOptions& options = {}",
  "line": 441,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_precision",
  "result_type": "PrecisionResult",
  "arguments": "const EventTable& data, const PetriNet& net, PrecisionVariant variant = PrecisionVariant::AUTO, const PrecisionOptions& options = {}",
  "line": 447,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_precision",
  "result_type": "PrecisionResult",
  "arguments": "const LazyEventTable& data, const PetriNet& net, PrecisionVariant variant = PrecisionVariant::AUTO, const PrecisionOptions& options = {}",
  "line": 453,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_precision",
  "result_type": "PrecisionResult",
  "arguments": "const EventStream& data, const ProcessTree& tree, PrecisionVariant variant = PrecisionVariant::AUTO, const PrecisionOptions& options = {}",
  "line": 459,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_precision",
  "result_type": "PrecisionResult",
  "arguments": "const EventTable& data, const ProcessTree& tree, PrecisionVariant variant = PrecisionVariant::AUTO, const PrecisionOptions& options = {}",
  "line": 465,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_precision",
  "result_type": "PrecisionResult",
  "arguments": "const LazyEventTable& data, const ProcessTree& tree, PrecisionVariant variant = PrecisionVariant::AUTO, const PrecisionOptions& options = {}",
  "line": 471,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_precision",
  "result_type": "PrecisionResult",
  "arguments": "const EventLog&, const ProcessTree&, PrecisionVariant = PrecisionVariant::AUTO",
  "line": 477,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_precision",
  "result_type": "PrecisionResult",
  "arguments": "const EventLog&, const ProcessTree&, PrecisionVariant, const PrecisionOptions&",
  "line": 479,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_generalization",
  "result_type": "GeneralizationResult",
  "arguments": "const EventLog&, const PetriNet&, GeneralizationVariant = GeneralizationVariant::GENERALIZATION_TOKEN",
  "line": 481,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_generalization",
  "result_type": "GeneralizationResult",
  "arguments": "const EventLog&, const ProcessTree&, GeneralizationVariant = GeneralizationVariant::GENERALIZATION_TOKEN",
  "line": 483,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_generalization",
  "result_type": "GeneralizationResult",
  "arguments": "const EventLog&, const PetriNet&, GeneralizationVariant, const GeneralizationOptions&",
  "line": 485,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_generalization",
  "result_type": "GeneralizationResult",
  "arguments": "const EventLog&, const PnmlDocument&, GeneralizationVariant = GeneralizationVariant::GENERALIZATION_TOKEN, const GeneralizationOptions& = {}",
  "line": 488,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_generalization",
  "result_type": "GeneralizationResult",
  "arguments": "const EventDataView&, const PnmlDocument&, GeneralizationVariant = GeneralizationVariant::GENERALIZATION_TOKEN, const GeneralizationOptions& = {}",
  "line": 492,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_generalization",
  "result_type": "GeneralizationResult",
  "arguments": "const EventLog&, const ProcessTree&, GeneralizationVariant, const GeneralizationOptions&",
  "line": 496,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_generalization",
  "result_type": "GeneralizationResult",
  "arguments": "const EventDataView&, const PetriNet&, GeneralizationVariant = GeneralizationVariant::GENERALIZATION_TOKEN, const GeneralizationOptions& = {}",
  "line": 499,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_generalization",
  "result_type": "GeneralizationResult",
  "arguments": "const EventDataView&, const ProcessTree&, GeneralizationVariant = GeneralizationVariant::GENERALIZATION_TOKEN, const GeneralizationOptions& = {}",
  "line": 503,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_generalization",
  "result_type": "GeneralizationResult",
  "arguments": "const EventStream& data, const PetriNet& net, GeneralizationVariant variant = GeneralizationVariant::GENERALIZATION_TOKEN, const GeneralizationOptions& options = {}",
  "line": 507,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_generalization",
  "result_type": "GeneralizationResult",
  "arguments": "const EventTable& data, const PetriNet& net, GeneralizationVariant variant = GeneralizationVariant::GENERALIZATION_TOKEN, const GeneralizationOptions& options = {}",
  "line": 513,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_generalization",
  "result_type": "GeneralizationResult",
  "arguments": "const LazyEventTable& data, const PetriNet& net, GeneralizationVariant variant = GeneralizationVariant::GENERALIZATION_TOKEN, const GeneralizationOptions& options = {}",
  "line": 519,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_generalization",
  "result_type": "GeneralizationResult",
  "arguments": "const EventStream& data, const ProcessTree& tree, GeneralizationVariant variant = GeneralizationVariant::GENERALIZATION_TOKEN, const GeneralizationOptions& options = {}",
  "line": 525,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_generalization",
  "result_type": "GeneralizationResult",
  "arguments": "const EventTable& data, const ProcessTree& tree, GeneralizationVariant variant = GeneralizationVariant::GENERALIZATION_TOKEN, const GeneralizationOptions& options = {}",
  "line": 531,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_generalization",
  "result_type": "GeneralizationResult",
  "arguments": "const LazyEventTable& data, const ProcessTree& tree, GeneralizationVariant variant = GeneralizationVariant::GENERALIZATION_TOKEN, const GeneralizationOptions& options = {}",
  "line": 537,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_simplicity",
  "result_type": "SimplicityResult",
  "arguments": "const PetriNet&, SimplicityVariant = SimplicityVariant::ARC_DEGREE, const SimplicityOptions& = {}",
  "line": 545,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_simplicity",
  "result_type": "SimplicityResult",
  "arguments": "const PetriNetPresentation&, SimplicityVariant = SimplicityVariant::ARC_DEGREE, const SimplicityOptions& = {}",
  "line": 551,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_simplicity",
  "result_type": "SimplicityResult",
  "arguments": "const PnmlDocument&, SimplicityVariant = SimplicityVariant::ARC_DEGREE, const SimplicityOptions& = {}",
  "line": 558,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_composite",
  "result_type": "CompositeEvaluationResult",
  "arguments": "const EventLog&, const PetriNet&, const CompositeEvaluationOptions& = {}",
  "line": 563,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_composite",
  "result_type": "CompositeEvaluationResult",
  "arguments": "const EventLog&, const PnmlDocument&, const CompositeEvaluationOptions& = {}",
  "line": 570,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_composite",
  "result_type": "CompositeEvaluationResult",
  "arguments": "const EventLog&, const ProcessTree&, const CompositeEvaluationOptions& = {}",
  "line": 573,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_composite",
  "result_type": "CompositeEvaluationResult",
  "arguments": "const EventDataView&, const PetriNet&, const CompositeEvaluationOptions& = {}",
  "line": 576,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_composite",
  "result_type": "CompositeEvaluationResult",
  "arguments": "const EventDataView&, const PnmlDocument&, const CompositeEvaluationOptions& = {}",
  "line": 579,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_composite",
  "result_type": "CompositeEvaluationResult",
  "arguments": "const EventDataView&, const ProcessTree&, const CompositeEvaluationOptions& = {}",
  "line": 582,
  "inline_wrapper": false
 },
 {
  "function": "evaluate_composite",
  "result_type": "CompositeEvaluationResult",
  "arguments": "const EventStream& data, const PetriNet& net, const CompositeEvaluationOptions& options = {}",
  "line": 585,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_composite",
  "result_type": "CompositeEvaluationResult",
  "arguments": "const EventTable& data, const PetriNet& net, const CompositeEvaluationOptions& options = {}",
  "line": 590,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_composite",
  "result_type": "CompositeEvaluationResult",
  "arguments": "const LazyEventTable& data, const PetriNet& net, const CompositeEvaluationOptions& options = {}",
  "line": 595,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_composite",
  "result_type": "CompositeEvaluationResult",
  "arguments": "const EventStream& data, const PnmlDocument& document, const CompositeEvaluationOptions& options = {}",
  "line": 600,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_composite",
  "result_type": "CompositeEvaluationResult",
  "arguments": "const EventTable& data, const PnmlDocument& document, const CompositeEvaluationOptions& options = {}",
  "line": 605,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_composite",
  "result_type": "CompositeEvaluationResult",
  "arguments": "const LazyEventTable& data, const PnmlDocument& document, const CompositeEvaluationOptions& options = {}",
  "line": 610,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_composite",
  "result_type": "CompositeEvaluationResult",
  "arguments": "const EventStream& data, const ProcessTree& tree, const CompositeEvaluationOptions& options = {}",
  "line": 615,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_composite",
  "result_type": "CompositeEvaluationResult",
  "arguments": "const EventTable& data, const ProcessTree& tree, const CompositeEvaluationOptions& options = {}",
  "line": 620,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_composite",
  "result_type": "CompositeEvaluationResult",
  "arguments": "const LazyEventTable& data, const ProcessTree& tree, const CompositeEvaluationOptions& options = {}",
  "line": 625,
  "inline_wrapper": true
 },
 {
  "function": "evaluate_simplicity",
  "result_type": "SimplicityResult",
  "arguments": "const ProcessTree&, SimplicityVariant = SimplicityVariant::ARC_DEGREE, const SimplicityOptions& = {}",
  "line": 630,
  "inline_wrapper": false
 }
] as const;
export const ORIGINAL_QUALITY_RECORD_SCHEMAS={
 "ReplayFitnessOptions": {
  "consider_activities_not_in_model_in_fitness": "bool",
  "activity_attribute": "std::string",
  "token_replay_variant": "TokenReplayVariant",
  "alignment_options": "AlignmentOptions",
  "decomposed_alignment_options": "std::optional<DecomposedAlignmentOptions>",
  "initial_marking": "std::vector<int>",
  "final_marking": "std::vector<int>",
  "case_id_attribute": "std::string",
  "max_rows_to_materialize": "std::optional<std::size_t>",
  "cleaning_token_flood": "bool",
  "exact_initial_marking": "std::optional<TokenMarking>",
  "exact_final_marking": "std::optional<TokenMarking>",
  "initial_marking_keys": "std::vector<bool>",
  "final_marking_keys": "std::vector<bool>",
  "place_names": "std::vector<std::string>",
  "transition_names": "std::vector<std::string>"
 },
 "PrecisionOptions": {
  "use_task_ids": "bool",
  "max_states": "std::size_t",
  "activity_attribute": "std::string",
  "token_replay_variant": "TokenReplayVariant",
  "initial_marking": "std::vector<int>",
  "final_marking": "std::vector<int>",
  "case_id_attribute": "std::string",
  "max_rows_to_materialize": "std::optional<std::size_t>",
  "exact_initial_marking": "std::optional<TokenMarking>",
  "exact_final_marking": "std::optional<TokenMarking>",
  "initial_marking_keys": "std::vector<bool>",
  "final_marking_keys": "std::vector<bool>",
  "place_names": "std::vector<std::string>",
  "transition_names": "std::vector<std::string>"
 },
 "GeneralizationOptions": {
  "activity_attribute": "std::string",
  "initial_marking": "std::vector<int>",
  "final_marking": "std::vector<int>",
  "case_id_attribute": "std::string",
  "max_rows_to_materialize": "std::optional<std::size_t>",
  "exact_initial_marking": "std::optional<TokenMarking>",
  "exact_final_marking": "std::optional<TokenMarking>",
  "initial_marking_keys": "std::vector<bool>",
  "final_marking_keys": "std::vector<bool>",
  "place_names": "std::vector<std::string>",
  "transition_names": "std::vector<std::string>"
 },
 "SimplicityOptions": {
  "initial_marking": "std::vector<int>",
  "final_marking": "std::vector<int>",
  "max_states": "std::size_t",
  "arc_degree_k": "double",
  "presentation_initial_marking": "std::optional<std::vector<PetriSparseMarkingEntry>>"
 },
 "CompositeEvaluationOptions": {
  "fitness_weight": "double",
  "precision_weight": "double",
  "simplicity_weight": "double",
  "generalization_weight": "double",
  "activity_attribute": "std::string",
  "initial_marking": "std::vector<int>",
  "final_marking": "std::vector<int>",
  "case_id_attribute": "std::string",
  "max_rows_to_materialize": "std::optional<std::size_t>",
  "exact_initial_marking": "std::optional<TokenMarking>",
  "exact_final_marking": "std::optional<TokenMarking>",
  "initial_marking_keys": "std::vector<bool>",
  "final_marking_keys": "std::vector<bool>",
  "place_names": "std::vector<std::string>",
  "transition_names": "std::vector<std::string>"
 },
 "ReplayFitnessResult": {
  "variant": "ReplayFitnessVariant",
  "fitness": "double",
  "log_fitness": "double",
  "average_trace_fitness": "double",
  "percentage_fit_traces": "double",
  "fitting_traces": "std::uint64_t",
  "trace_count": "std::uint64_t",
  "total_cost": "double",
  "computed": "bool",
  "exhausted": "bool",
  "error": "std::string",
  "token_result": "ConformanceResult",
  "alignment_result": "AlignmentResult",
  "decomposed_alignment_result": "std::optional<DecomposedAlignmentResult>",
  "log_fitness_is_integer_zero": "bool",
  "token_result_is_fitness_only": "bool"
 },
 "PrecisionResult": {
  "variant": "PrecisionVariant",
  "precision": "double",
  "computed": "bool",
  "exhausted": "bool",
  "error": "std::string",
  "token_result": "ConformanceResult",
  "alignment_result": "AlignmentResult",
  "alignment_detail_present": "bool",
  "token_result_is_precision_only": "bool"
 },
 "GeneralizationResult": {
  "variant": "GeneralizationVariant",
  "generalization": "double",
  "executed_transitions": "std::uint64_t",
  "transition_count": "std::uint64_t",
  "transition_frequencies": "std::vector<std::uint64_t>",
  "computed": "bool",
  "error": "std::string"
 },
 "SimplicityResult": {
  "variant": "SimplicityVariant",
  "simplicity": "double",
  "complexity": "double",
  "value": "double",
  "vertices": "std::size_t",
  "edges": "std::size_t",
  "strongly_connected_components": "std::size_t",
  "computed": "bool",
  "exhausted": "bool",
  "error": "std::string"
 },
 "CompositeEvaluationResult": {
  "fitness": "double",
  "precision": "double",
  "generalization": "double",
  "simplicity": "double",
  "metrics_average_weight": "double",
  "fscore": "double",
  "fitness_weight": "double",
  "precision_weight": "double",
  "simplicity_weight": "double",
  "generalization_weight": "double",
  "computed": "bool",
  "error": "std::string",
  "fitness_details": "ReplayFitnessResult",
  "fitness_score": "double"
 },
 "AlignmentOptions": {
  "variant": "AlignmentVariant",
  "cost_profile": "AlignmentCostProfile",
  "max_states": "std::size_t",
  "max_time_seconds": "double",
  "max_total_time_seconds": "double",
  "max_alignments": "std::size_t",
  "exponent": "double",
  "synchronous_dijkstra": "bool",
  "default_trace_cost": "std::optional<double>",
  "default_model_cost": "std::optional<double>",
  "default_sync_cost": "std::optional<double>",
  "default_silent_cost": "std::optional<double>",
  "trace_costs": "std::unordered_map<std::string, double>",
  "model_costs": "std::unordered_map<std::string, double>",
  "sync_costs": "std::unordered_map<std::string, double>",
  "trace_costs_by_position": "std::vector<double>",
  "model_costs_by_transition": "std::vector<double>",
  "sync_costs_by_transition": "std::vector<double>",
  "trace_cost_function": "std::function<double(const std::string&, std::size_t)>",
  "model_cost_function": "std::function<double(const std::string&, std::size_t)>",
  "sync_cost_function": "std::function<double(const std::string&, std::size_t)>",
  "activity_attribute": "std::string",
  "initial_marking": "std::vector<int>",
  "final_marking": "std::vector<int>"
 },
 "AlignmentDiagnostics": {
  "visited_states": "std::size_t",
  "queued_states": "std::size_t",
  "traversed_arcs": "std::size_t",
  "lp_solved": "std::size_t",
  "closed_set_length": "std::size_t",
  "num_visited_markings": "std::size_t"
 },
 "AlignmentMove": {
  "type": "AlignmentMoveType",
  "activity": "std::string",
  "transition": "std::size_t"
 },
 "ProjectedAlignmentMove": {
  "activity": "std::string",
  "transition": "std::size_t",
  "marking_before": "std::vector<int>",
  "marking_after": "std::vector<int>"
 },
 "VariantAlignment": {
  "activities": "std::vector<std::string>",
  "count": "std::uint64_t",
  "moves": "std::vector<AlignmentMove>",
  "cost": "double",
  "fitness": "double",
  "optimal_alignments": "std::vector<std::vector<AlignmentMove>>",
  "diagnostics": "AlignmentDiagnostics",
  "worst_cost": "double",
  "projected_model_moves": "std::vector<ProjectedAlignmentMove>",
  "complete_moves": "std::vector<AlignmentMove>"
 },
 "AlignmentResult": {
  "computed": "bool",
  "exhausted": "bool",
  "fitness": "double",
  "precision": "double",
  "fitting_traces": "std::uint64_t",
  "trace_count": "std::uint64_t",
  "total_cost": "double",
  "variant": "AlignmentVariant",
  "diagnostics": "AlignmentDiagnostics",
  "variants": "std::vector<VariantAlignment>",
  "activity_attribute": "std::string"
 },
 "VariantConformance": {
  "activities": "std::vector<std::string>",
  "count": "std::uint64_t",
  "fitness": "double",
  "null_activity_indices": "std::vector<std::size_t>"
 },
 "ConformanceResult": {
  "fitness": "double",
  "precision": "double",
  "generalization": "double",
  "simplicity": "double",
  "fitting_traces": "std::uint64_t",
  "trace_count": "std::uint64_t",
  "produced": "TokenInteger",
  "consumed": "TokenInteger",
  "missing": "TokenInteger",
  "remaining": "TokenInteger",
  "transition_frequencies": "std::vector<std::uint64_t>",
  "variants": "std::vector<VariantConformance>",
  "activity_attribute": "std::string",
  "trace_diagnostics": "std::optional<std::vector<TokenReplayTraceDiagnostics>>",
  "duration_diagnostics": "std::optional<TokenReplayDurationDiagnostics>",
  "root_cause_diagnostics": "std::optional<TokenReplayRootCauseDiagnostics>"
 },
 "TokenReplayTraceDiagnostics": {
  "trace_index": "std::size_t",
  "case_id": "std::string",
  "trace_is_fit": "bool",
  "trace_fitness": "double",
  "produced": "TokenInteger",
  "consumed": "TokenInteger",
  "missing": "TokenInteger",
  "remaining": "TokenInteger",
  "activated_transitions": "std::vector<std::size_t>",
  "transitions_with_problems": "std::vector<std::size_t>",
  "enabled_transitions": "std::vector<std::size_t>",
  "unknown_event_indices": "std::vector<std::size_t>",
  "unknown_activities": "std::vector<std::string>",
  "unknown_activity_is_null": "std::vector<bool>",
  "reached_marking": "TokenMarking",
  "places": "std::vector<TokenReplayPlaceDiagnostics>",
  "replay_stopped": "bool",
  "replayed_events": "std::size_t",
  "event_diagnostics": "std::vector<TokenReplayEventDiagnostics>",
  "reached_marking_keys": "std::vector<bool>",
  "cleaned_place_indices": "std::vector<std::size_t>",
  "marking_domain": "std::optional<TokenReplayMarkingDomain>",
  "reached_marking_order": "std::vector<std::size_t>"
 },
 "TokenReplayPlaceDiagnostics": {
  "missing": "TokenInteger",
  "remaining": "TokenInteger",
  "consumed": "TokenInteger",
  "produced": "TokenInteger"
 },
 "TokenReplayEventDiagnostics": {
  "event_index": "std::size_t",
  "activated_transitions": "std::vector<std::size_t>",
  "transitions_with_problems": "std::vector<std::size_t>"
 },
 "TokenReplayMarkingDomain": {
  "places": "std::vector<PetriMarkingPlace>",
  "detached_places": "std::vector<PetriDetachedMarkingPlace>"
 },
 "TokenReplayDurationComparison": {
  "fit_trace_indices": "std::vector<std::size_t>",
  "problem_trace_indices": "std::vector<std::size_t>",
  "fit_count": "std::size_t",
  "problem_count": "std::size_t",
  "fit_median_seconds": "double",
  "problem_median_seconds": "double",
  "relative_throughput": "double"
 },
 "TokenReplayTransitionDurationDiagnostics": {
  "fit_trace_indices": "std::vector<std::size_t>",
  "problem_trace_indices": "std::vector<std::size_t>",
  "fit_count": "std::size_t",
  "problem_count": "std::size_t",
  "fit_median_seconds": "double",
  "problem_median_seconds": "double",
  "relative_throughput": "double",
  "transition_index": "std::size_t"
 },
 "TokenReplayUnknownActivityDurationDiagnostics": {
  "fit_trace_indices": "std::vector<std::size_t>",
  "problem_trace_indices": "std::vector<std::size_t>",
  "fit_count": "std::size_t",
  "problem_count": "std::size_t",
  "fit_median_seconds": "double",
  "problem_median_seconds": "double",
  "relative_throughput": "double",
  "activity": "std::string",
  "activity_is_null": "bool"
 },
 "TokenReplayDurationDiagnostics": {
  "timestamp_attribute": "std::string",
  "transitions": "std::vector<TokenReplayTransitionDurationDiagnostics>",
  "unknown_activities": "std::vector<TokenReplayUnknownActivityDurationDiagnostics>"
 },
 "TokenReplayTransitionRootCauseDiagnostics": {
  "feature_names": "std::vector<std::string>",
  "classes": "std::vector<std::string>",
  "data": "std::vector<std::vector<double>>",
  "target": "std::vector<int>",
  "source_trace_indices": "std::vector<std::size_t>",
  "tree": "std::vector<RootCauseTreeNode>",
  "fit_count": "std::size_t",
  "problem_count": "std::size_t",
  "transition_index": "std::size_t"
 },
 "TokenReplayUnknownRootCauseDiagnostics": {
  "feature_names": "std::vector<std::string>",
  "classes": "std::vector<std::string>",
  "data": "std::vector<std::vector<double>>",
  "target": "std::vector<int>",
  "source_trace_indices": "std::vector<std::size_t>",
  "tree": "std::vector<RootCauseTreeNode>",
  "fit_count": "std::size_t",
  "problem_count": "std::size_t",
  "activity": "std::string",
  "activity_is_null": "bool",
  "activity_attribute": "std::optional<EventAttribute>"
 },
 "TokenReplayRootCauseDiagnostics": {
  "options": "RootCauseOptions",
  "transitions": "std::vector<TokenReplayTransitionRootCauseDiagnostics>",
  "unknown_activities": "std::vector<TokenReplayUnknownRootCauseDiagnostics>"
 },
 "DecomposedAlignmentOptions": {
  "variant": "DecomposedAlignmentVariant",
  "threshold_border_agreement": "double",
  "max_time_trace": "double",
  "max_time_total": "double",
  "max_states": "std::size_t",
  "max_total_states": "std::size_t",
  "use_cache": "bool",
  "clear_cache": "bool",
  "log_move_cost": "double",
  "model_move_cost": "double",
  "tau_model_cost": "double",
  "sync_cost": "double",
  "activity_log_costs": "std::unordered_map<std::string, double>",
  "activity_model_costs": "std::unordered_map<std::string, double>",
  "activity_sync_costs": "std::unordered_map<std::string, double>",
  "initial_marking": "std::vector<std::size_t>",
  "final_marking": "std::vector<std::size_t>",
  "activity_attribute": "std::string"
 },
 "DecomposedAlignmentMove": {
  "log": "std::optional<std::string>",
  "model": "std::optional<std::string>",
  "transition": "std::size_t"
 },
 "DecomposedAlignmentTrace": {
  "activities": "std::vector<std::string>",
  "count": "std::uint64_t",
  "complete": "bool",
  "timed_out": "bool",
  "exhausted": "bool",
  "cost": "double",
  "fitness": "double",
  "best_worst_cost": "double",
  "alignment": "std::vector<DecomposedAlignmentMove>"
 },
 "DecomposedAlignmentDiagnostics": {
  "decompositions": "std::size_t",
  "components": "std::size_t",
  "merges": "std::size_t",
  "cache_hits": "std::size_t",
  "cache_misses": "std::size_t",
  "border_disagreements": "std::size_t",
  "agreement_iterations": "std::size_t",
  "visited_states": "std::size_t",
  "queued_states": "std::size_t",
  "traversed_arcs": "std::size_t",
  "whole_net_fallback": "bool",
  "note": "std::string"
 },
 "DecomposedAlignmentResult": {
  "complete": "bool",
  "timed_out": "bool",
  "exhausted": "bool",
  "fitness": "double",
  "best_worst_cost": "double",
  "trace_count": "std::uint64_t",
  "total_cost": "double",
  "diagnostics": "DecomposedAlignmentDiagnostics",
  "traces": "std::vector<DecomposedAlignmentTrace>"
 },
 "PetriNetLiteralStorage": {
  "place_count": "std::size_t",
  "source_storage": "std::size_t",
  "sink_storage": "std::size_t",
  "has_source": "bool",
  "has_sink": "bool",
  "transitions": "std::vector<PetriTransition>",
  "activities": "std::vector<std::string>",
  "discovery_markings": "std::optional<PetriDiscoveryMarkings>",
  "transition_names": "std::optional<PetriTransitionNameList>"
 },
 "PetriTransition": {
  "activity": "std::optional<std::size_t>",
  "inputs": "std::vector<std::size_t>",
  "outputs": "std::vector<std::size_t>",
  "reset_inputs": "std::vector<std::size_t>",
  "inhibitor_inputs": "std::vector<std::size_t>",
  "reset_weights": "std::vector<PetriArcWeight>",
  "inhibitor_weights": "std::vector<PetriArcWeight>",
  "input_weights": "std::vector<PetriArcWeight>",
  "output_weights": "std::vector<PetriArcWeight>"
 },
 "PetriArcWeight": {
  "magnitude": "TokenInteger",
  "negative": "bool"
 },
 "PetriNetPresentation": {
  "name": "std::string",
  "properties": "HeuristicStructuredValue::Object",
  "places": "std::vector<PetriPresentedPlace>",
  "transitions": "std::vector<PetriPresentedTransition>",
  "arcs": "std::vector<PetriPresentedArc>",
  "initial_marking": "std::vector<PetriSparseMarkingEntry>",
  "final_marking": "std::vector<PetriSparseMarkingEntry>"
 },
 "PetriPresentedPlace": {
  "id": "std::size_t",
  "name": "std::string",
  "properties": "HeuristicStructuredValue::Object"
 },
 "PetriPresentedTransition": {
  "id": "std::size_t",
  "name": "std::string",
  "label": "std::optional<std::string>",
  "properties": "HeuristicStructuredValue::Object"
 },
 "PetriNodeRef": {
  "kind": "PetriNodeKind",
  "id": "std::size_t"
 },
 "PetriPresentedArc": {
  "source": "PetriNodeRef",
  "target": "PetriNodeRef",
  "weight": "TokenInteger",
  "properties": "HeuristicStructuredValue::Object",
  "negative_weight": "bool"
 },
 "PetriSparseMarkingEntry": {
  "place_id": "std::size_t",
  "tokens": "std::int64_t"
 },
 "PetriDiscoveryMarkings": {
  "detached_places": "std::vector<PetriDetachedMarkingPlace>",
  "initial": "std::vector<PetriMarkingEntry>",
  "final": "std::vector<PetriMarkingEntry>"
 },
 "PetriMarkingPlace": {
  "kind": "PetriMarkingPlaceKind",
  "id": "std::size_t"
 },
 "PetriMarkingEntry": {
  "place": "PetriMarkingPlace",
  "tokens": "TokenInteger"
 },
 "PetriDetachedMarkingPlace": {
  "id": "std::size_t",
  "name": "HeuristicStructuredValue"
 },
 "PnmlDocument": {
  "net": "PetriNet",
  "initial_marking": "TokenMarking",
  "final_marking": "TokenMarking",
  "stochastic_map": "std::optional<StochasticMap>",
  "place_ids": "std::vector<std::string>",
  "transition_ids": "std::vector<std::string>",
  "initial_marking_keys": "std::vector<bool>",
  "final_marking_keys": "std::vector<bool>",
  "arc_occurrences": "std::optional<std::vector<PnmlArcOccurrence>>"
 },
 "PnmlArcOccurrence": {
  "place": "std::size_t",
  "transition": "std::size_t",
  "input": "bool",
  "type": "PetriArcType",
  "weight": "TokenInteger",
  "id": "std::string",
  "negative_weight": "bool"
 },
 "StochasticMap": {
  "transitions": "std::vector<StochasticMapTransition>"
 },
 "StochasticMapTransition": {
  "transition": "std::size_t",
  "timing": "StochasticMapTiming",
  "distribution": "StochasticMapDistribution",
  "parameter": "double",
  "secondary_parameter": "double",
  "tertiary_parameter": "double",
  "activation_weight": "double",
  "priority": "int"
 },
 "EventStream": {
  "attributes": "std::vector<EventAttribute>",
  "events": "std::vector<EventRecord>",
  "metadata": "EventDataMetadata"
 },
 "EventRecord": {
  "fields": "std::vector<EventAttribute>"
 },
 "EventTable": {
  "columns": "std::vector<std::string>",
  "rows": "std::vector<Row>",
  "attributes": "std::vector<EventAttribute>",
  "metadata": "EventDataMetadata",
  "dtypes": "std::vector<std::string>",
  "index": "std::optional<EventTableIndex>",
  "categorical_dtypes": "std::vector<std::optional<EventTableCategoricalDtype>>"
 },
 "EventTableIndex": {
  "kind": "std::string",
  "dtype": "std::string",
  "names": "std::vector<std::optional<std::string>>",
  "labels": "std::vector<EventTableIndexLabel>"
 },
 "EventTableIndexLabel": {
  "value": "std::variant<EventAttribute, std::vector<EventTableIndexLabel>>"
 },
 "EventTableCategoricalDtype": {
  "categories_dtype": "std::string",
  "categories": "std::vector<EventAttribute>",
  "ordered": "bool"
 },
 "RootCauseOptions": {
  "string_attributes": "std::vector<std::string>",
  "numeric_attributes": "std::vector<std::string>",
  "enable_multiplier": "bool"
 },
 "RootCauseRepresentation": {
  "feature_names": "std::vector<std::string>",
  "data": "std::vector<std::vector<double>>",
  "target": "std::vector<int>",
  "source_trace_indices": "std::vector<std::size_t>"
 },
 "RootCauseModel": {
  "feature_names": "std::vector<std::string>",
  "classes": "std::vector<std::string>",
  "data": "std::vector<std::vector<double>>",
  "target": "std::vector<int>",
  "source_trace_indices": "std::vector<std::size_t>",
  "tree": "std::vector<RootCauseTreeNode>",
  "fit_count": "std::size_t",
  "problem_count": "std::size_t"
 },
 "RootCauseTreeNode": {
  "feature_index": "int",
  "threshold": "double",
  "left": "int",
  "right": "int",
  "class_counts": "std::array<std::size_t, 2>",
  "missing_go_to_left": "bool",
  "missing_only": "bool"
 }
} as const;
export const ORIGINAL_QUALITY_ENUMS={
 "ReplayFitnessVariant": {
  "AUTO": 0,
  "TOKEN_BASED": 1,
  "ALIGNMENT_BASED": 2
 },
 "PrecisionVariant": {
  "AUTO": 0,
  "ETCONFORMANCE_TOKEN": 1,
  "ALIGN_ETCONFORMANCE": 2,
  "AUTOMATON_AFTER_ALIGN": 3
 },
 "GeneralizationVariant": {
  "GENERALIZATION_TOKEN": 0
 },
 "SimplicityVariant": {
  "ARC_DEGREE": 0,
  "EXTENDED_CARDOSO": 1,
  "EXTENDED_CYCLOMATIC": 2
 },
 "TokenReplayVariant": {
  "FORWARD": 0,
  "BACKWARDS": 1
 },
 "AlignmentVariant": {
  "STATE_EQUATION_A_STAR": 0,
  "DIJKSTRA_NO_HEURISTICS": 1,
  "DIJKSTRA_LESS_MEMORY": 2,
  "DISCOUNTED_A_STAR": 3,
  "GENERATOR_DIJKSTRA_NO_HEURISTICS": 4,
  "GENERATOR_DIJKSTRA_LESS_MEMORY": 5
 },
 "AlignmentCostProfile": {
  "LEGACY_UNIT": 0,
  "STANDARD": 1
 },
 "DecomposedAlignmentVariant": {
  "RECOMPOS_MAXIMAL": 0
 },
 "ProcessTreeOperator": {
  "ACTIVITY": 0,
  "TAU": 1,
  "SEQUENCE": 2,
  "EXCLUSIVE": 3,
  "INCLUSIVE_CHOICE": 4,
  "PARALLEL": 5,
  "INTERLEAVING": 6,
  "LOOP": 7
 },
 "PetriArcType": {
  "NORMAL": 0,
  "RESET": 1,
  "INHIBITOR": 2
 },
 "PetriNodeKind": {
  "PLACE": 0,
  "TRANSITION": 1
 },
 "PetriMarkingPlaceKind": {
  "ROOT": 0,
  "DETACHED": 1
 },
 "StochasticMapTiming": {
  "TIMED": 0,
  "IMMEDIATE": 1
 },
 "StochasticMapDistribution": {
  "DETERMINISTIC": 0,
  "EXPONENTIAL": 1,
  "CONSTANT0": 2,
  "IMMEDIATE": 3,
  "NORMAL": 4,
  "UNIFORM": 5,
  "LOGNORMAL": 6,
  "GAMMA": 7
 },
 "AlignmentMoveType": {
  "SYNCHRONOUS": 0,
  "LOG": 1,
  "MODEL": 2
 }
} as const;
export const ORIGINAL_QUALITY_OPTION_MEMBERS=Object.fromEntries(Object.values(ORIGINAL_QUALITY_SPECS).map(spec=>[spec.options,Object.keys(ORIGINAL_QUALITY_RECORD_SCHEMAS[spec.options])])) as Record<string,string[]>;
export const ORIGINAL_QUALITY_OPERATIONS=Object.keys(ORIGINAL_QUALITY_SPECS) as OriginalQualityOperation[];
export const ORIGINAL_QUALITY_ARTIFACTS=["source_input_json","model_input_json","source_after_json","model_after_json","configured_options_json","configured_callback_tables_json","callback_invocations_json","full_result_json"] as const;
export const ORIGINAL_QUALITY_CALLBACK_SLOTS=["trace_cost_function","model_cost_function","sync_cost_function"] as const;
export type OriginalQualityOperation=keyof typeof ORIGINAL_QUALITY_SPECS;
export type OriginalQualityArtifact=typeof ORIGINAL_QUALITY_ARTIFACTS[number];
export type OriginalQualitySourceKind="log"|"stream"|"table"|"lazy"|"view_log"|"view_stream"|"view_table"|"view_lazy";
export type OriginalQualityModelKind="petri_net"|"process_tree"|"pnml_document"|"presentation";
export type OriginalQualityRole="source"|"model";
export type OriginalQualityCallbackSlot=typeof ORIGINAL_QUALITY_CALLBACK_SLOTS[number];
export interface OriginalQualityDispatch {source_kind:OriginalQualitySourceKind|null;model_kind:OriginalQualityModelKind;call_form:"with_options"|"default_options";}
export interface OriginalQualityForm extends OriginalQualityDispatch {operation:OriginalQualityOperation;variant:OriginalQualityControl;options:OriginalQualityControl;callbacks:OriginalQualityControl;}
export function defaultOriginalQualityForm(operation:OriginalQualityOperation="evaluate_replay_fitness"):OriginalQualityForm{return {operation,...ORIGINAL_QUALITY_DISPATCH[operation][0],variant:defaultOriginalEditDistanceControl(operation==="evaluate_simplicity"?'"ARC_DEGREE"':operation==="evaluate_generalization"?'"GENERALIZATION_TOKEN"':'"AUTO"',false),options:defaultOriginalEditDistanceControl("{}",false),callbacks:defaultOriginalEditDistanceControl("{}",false)};}
export interface OriginalQualitySubmittedControl {name:string;text:string;file:File|null;}
export function originalQualityControls(form:OriginalQualityForm):OriginalQualitySubmittedControl[]{
 if(!Object.hasOwn(ORIGINAL_QUALITY_SPECS,form.operation)||!(ORIGINAL_QUALITY_DISPATCH[form.operation] as readonly OriginalQualityDispatch[]).some(item=>item.source_kind===form.source_kind&&item.model_kind===form.model_kind&&item.call_form===form.call_form))throw new Error("Choose a declared quality overload.");
 const spec=ORIGINAL_QUALITY_SPECS[form.operation],controls:OriginalQualitySubmittedControl[]=[{name:"operation",text:form.operation,file:null}];
 if(form.source_kind!==null)controls.push({name:"source_kind",text:form.source_kind,file:null});
 controls.push({name:"model_kind",text:form.model_kind,file:null},{name:"call_form",text:form.call_form,file:null});
 if(form.variant.supplied){if(spec.variant===null)throw new Error("variant is inapplicable to composite quality.");controls.push({name:"variant",text:form.variant.text,file:form.variant.file});}
 if(form.call_form==="default_options"&&(form.options.supplied||form.callbacks.supplied))throw new Error("The default-options overload forbids options_json and callback_tables_json by presence.");
 if(form.options.supplied)controls.push({name:"options_json",text:form.options.text,file:form.options.file});
 if(form.callbacks.supplied){if(form.operation!=="evaluate_replay_fitness")throw new Error("Callback tables apply only to replay fitness with options.");controls.push({name:"callback_tables_json",text:form.callbacks.text,file:form.callbacks.file});}
 return controls;
}
const object=(value:unknown):value is Record<string,unknown>=>value!==null&&typeof value==="object"&&!Array.isArray(value);
const exact=(value:Record<string,unknown>,keys:readonly string[]):boolean=>Object.keys(value).length===keys.length&&keys.every(key=>Object.hasOwn(value,key));
const list=(value:unknown,predicate:(item:unknown)=>boolean):value is unknown[]=>Array.isArray(value)&&value.every(predicate);
const nullable=(value:unknown,predicate:(item:unknown)=>boolean):boolean=>value===null||predicate(value);
const strings=(value:unknown):value is string[]=>list(value,item=>typeof item==="string");
const nativeString=isOriginalEmbeddingFilterNativeString;
const equalBytes=(a:Uint8Array,b:Uint8Array):boolean=>a.length===b.length&&a.every((byte,index)=>byte===b[index]);
const sameString=(a:OriginalQualityNativeString,b:OriginalQualityNativeString):boolean=>equalBytes(originalEmbeddingFilterStringBytes(a),originalEmbeddingFilterStringBytes(b));
function integer(value:unknown,holder?:object,key?:string|number,signed=false,bits?:number):boolean{if(typeof value!=="number")return false;const token=String(holder&&key!==undefined?nativeCell(holder,key):value);if(!(signed?/^-?(?:0|[1-9]\d*)$/:/^(?:0|[1-9]\d*)$/).test(token))return false;if(bits===undefined)return true;const n=BigInt(token),limit=1n<<BigInt(bits-(signed?1:0));return n>=(signed?-limit:0n)&&n<limit;}
const double=(value:unknown):boolean=>typeof value==="number"&&Number.isFinite(value)||typeof value==="string"&&["NaN","INF","-INF"].includes(value);
function splitType(type:string):string[]{const result:string[]=[];let depth=0,start=0;for(let index=0;index<type.length;++index){if(type[index]==="<"||type[index]==="(")++depth;else if(type[index]===">"||type[index]===")")--depth;else if(type[index]===","&&depth===0){result.push(type.slice(start,index).trim());start=index+1;}}result.push(type.slice(start).trim());return result;}
export function isOriginalQualityEnum(type:string,value:unknown,intBits=32):boolean{const schema=(ORIGINAL_QUALITY_ENUMS as Record<string,Record<string,number>>)[type];if(!schema||!object(value)||!exact(value,["name","index"])||!integer(value.index,value,"index",true,intBits))return false;const token=BigInt(String(nativeCell(value,"index"))),known=Object.entries(schema).find(([,index])=>BigInt(index)===token);return value.name===(known?.[0]??null);}
function rawLogPart(field:"attributes"|"metadata",value:unknown,wrapped=false):boolean{return isOriginalEmbeddingFilterRawLog(wrapped?{native_byte_strings:"hex-v1",value:{attributes:field==="attributes"?value:[],metadata:field==="metadata"?value:DEFAULT_METADATA,traces:[]}}:{attributes:field==="attributes"?value:[],metadata:field==="metadata"?value:DEFAULT_METADATA,traces:[]});}
export function isOriginalQualityStream(value:unknown):boolean{const wrapped=object(value)&&value.native_byte_strings==="hex-v1"&&value.native_schema==="event-stream-raw-v1",stream=wrapped?(value as Record<string,unknown>).value:value;return object(stream)&&exact(stream,["attributes","metadata","events"])&&rawLogPart("attributes",stream.attributes,wrapped)&&rawLogPart("metadata",stream.metadata,wrapped)&&list(stream.events,item=>object(item)&&exact(item,["fields"])&&rawLogPart("attributes",item.fields,wrapped));}
export function isOriginalQualitySource(kind:OriginalQualitySourceKind,value:unknown,sizeBits=64):boolean{const base=kind.replace(/^view_/,"");if(base==="log")return isOriginalEmbeddingFilterRawLog(value);if(base==="stream")return isOriginalQualityStream(value);if(base==="table")return isOriginalEmbeddingFilterTable(value);return base==="lazy"&&object(value)&&exact(value,["table","valid","row_count"])&&isOriginalEmbeddingFilterTable(value.table)&&typeof value.valid==="boolean"&&(value.row_count===null||integer(value.row_count,value,"row_count",false,sizeBits));}
export function isOriginalQualityNativeType(type:string,value:unknown,holder?:object,key?:string|number,sizeBits=64,intBits=32):boolean{
 const check=(member:string,item:unknown,h?:object,k?:string|number)=>isOriginalQualityNativeType(member,item,h,k,sizeBits,intBits);
 if(type==="std::string"||type==="std::string_view")return nativeString(value);if(type==="double")return double(value);if(type==="bool")return typeof value==="boolean";
 if(type==="TokenInteger")return integer(value,holder,key,true);
 if(["std::size_t","int","std::int64_t","std::uint64_t"].includes(type))return integer(value,holder,key,type==="int"||type==="std::int64_t",type==="std::size_t"?sizeBits:type==="int"?intBits:64);
 if(Object.hasOwn(ORIGINAL_QUALITY_ENUMS,type))return isOriginalQualityEnum(type,value,intBits);
 if(type.startsWith("std::optional<"))return value===null||check(type.slice(14,-1),value,holder,key);
 if(type.startsWith("std::vector<"))return Array.isArray(value)&&value.every((item,index)=>check(type.slice(12,-1),item,value,index));
 if(type.startsWith("std::array<")){const [member,length]=splitType(type.slice(11,-1));return Array.isArray(value)&&value.length===Number(length)&&value.every((item,index)=>check(member,item,value,index));}
 if(type.startsWith("std::variant<")){const arms=splitType(type.slice(13,-1));return object(value)&&exact(value,["index","value"])&&integer(value.index,value,"index",false,intBits)&&Number(value.index)<arms.length&&check(arms[Number(value.index)],value.value,value,"value");}
 if(type.startsWith("std::unordered_map<")||type.startsWith("std::map<")){const [kt,vt]=splitType(type.slice(type.indexOf("<")+1,-1)),seen=new Set<string>();return list(value,item=>{if(!object(item)||!exact(item,["key","value"])||!check(kt,item.key,item,"key")||!check(vt,item.value,item,"value"))return false;const id=kt==="std::string"?hex(originalEmbeddingFilterStringBytes(item.key as OriginalQualityNativeString)):nativeJson(item.key);if(seen.has(id))return false;seen.add(id);return true;});}
 if(type.startsWith("std::function<"))return value===null||object(value)&&exact(value,["bound"])&&value.bound===true;
 if(type==="TokenMarking")return check("std::vector<TokenInteger>",value);
 if(type==="PetriTransitionNameList")return check("std::vector<std::optional<std::string>>",value);
 if(type==="PetriNet")return isOriginalOcPlayoutNativeType("PetriNet",value);
 if(type==="ProcessTree"){const node=(item:unknown):boolean=>object(item)&&exact(item,["op","activity","children"])&&isOriginalQualityEnum("ProcessTreeOperator",item.op,intBits)&&integer(item.activity,item,"activity",false,sizeBits)&&list(item.children,node);return object(value)&&exact(value,["activities","root"])&&list(value.activities,nativeString)&&node(value.root);}
 if(type==="EventLog")return isOriginalEmbeddingFilterRawLog(value);
 if(type==="EventAttribute")return isOriginalQualityAttribute(value,sizeBits,intBits);if(type==="EventDataMetadata")return rawLogPart("metadata",value);
 if(type==="Row")return check("std::vector<std::optional<EventAttribute>>",value);
 if(type==="HeuristicStructuredValue"){const arms=["null","bool","std::int64_t","std::uint64_t","double","std::string","std::vector<HeuristicStructuredValue>","HeuristicStructuredValue::Object"];return object(value)&&exact(value,["index","value"])&&integer(value.index,value,"index",false,intBits)&&Number(value.index)<arms.length&&(value.index===0?value.value===null:check(arms[Number(value.index)],value.value,value,"value"));}
 if(type==="HeuristicStructuredValue::Object")return check("std::map<std::string,HeuristicStructuredValue>",value);
 const schema=(ORIGINAL_QUALITY_RECORD_SCHEMAS as Record<string,Record<string,string>>)[type];return !!schema&&object(value)&&exact(value,Object.keys(schema))&&Object.entries(schema).every(([name,member])=>check(member,value[name],value,name));
}
export function isOriginalQualityAttribute(value:unknown,sizeBits=64,intBits=32):boolean{
 if(!object(value)||!exact(value,["key","value","kind_index","container_index","null","scalar_wrapper","scalar_children_list","first_class_role_index","first_class_state_index","first_class_source_key","typed_value","children"])||!["key","value","first_class_source_key"].every(key=>nativeString(value[key]))||!["kind_index","container_index","first_class_role_index","first_class_state_index"].every(key=>integer(value[key],value,key,true,intBits))||!["null","scalar_wrapper","scalar_children_list"].every(key=>typeof value[key]==="boolean")||!list(value.children,item=>isOriginalQualityAttribute(item,sizeBits,intBits))||!object(value.typed_value)||!exact(value.typed_value,["index","value"])||!integer(value.typed_value.index,value.typed_value,"index",false,intBits))return false;
 const typed=value.typed_value,index=Number(typed.index);if(index===0)return typed.value===null;if(index===6)return typed.value==="NA";
 if(index===4)return object(typed.value)&&exact(typed.value,["value","resolution_index","timezone"])&&nativeString(typed.value.value)&&integer(typed.value.resolution_index,typed.value,"resolution_index",true,intBits)&&nullable(typed.value.timezone,nativeString);
 return index>=1&&index<=5&&isOriginalQualityNativeType(index===1?"std::int64_t":index===2?"double":index===3?"bool":"std::uint64_t",typed.value,typed,"value",sizeBits,intBits);
}
export function isOriginalQualityModel(kind:OriginalQualityModelKind,value:unknown,sizeBits=64,intBits=32):boolean{return isOriginalQualityNativeType(({petri_net:"PetriNet",process_tree:"ProcessTree",pnml_document:"PnmlDocument",presentation:"PetriNetPresentation"})[kind],value,undefined,undefined,sizeBits,intBits);}
export function isOriginalQualityBytes(value:unknown):value is OriginalQualityBytes{return object(value)&&exact(value,["dataset","encoding","data","bytes","sha256"])&&nativeString(value.dataset)&&typeof value.data==="string"&&integer(value.bytes,value,"bytes")&&typeof value.sha256==="string"&&/^[a-f0-9]{64}$/.test(value.sha256)&&(value.encoding==="utf8"||value.encoding==="hex"&&/^(?:[a-fA-F0-9]{2})*$/.test(value.data));}
export interface OriginalQualityError {phase:string;exception_type:OriginalQualityNativeString;message:OriginalQualityNativeString;}
const error=(value:unknown):value is OriginalQualityError=>object(value)&&exact(value,["phase","exception_type","message"])&&typeof value.phase==="string"&&nativeString(value.exception_type)&&nativeString(value.message);
export interface OriginalQualityCall {name:OriginalQualityNativeString;stage:"preparation"|"primary";input_kind:OriginalQualityNativeString;returned:boolean;error:OriginalQualityError|null;}
const call=(value:unknown):value is OriginalQualityCall=>object(value)&&exact(value,["name","stage","input_kind","returned","error"])&&nativeString(value.name)&&["preparation","primary"].includes(String(value.stage))&&nativeString(value.input_kind)&&typeof value.returned==="boolean"&&nullable(value.error,error);
export const isOriginalQualityOutcome=(value:unknown):boolean=>object(value)&&(exact(value,["value"])&&double(value.value)||exact(value,["throw"])&&nativeString(value.throw));
export function isOriginalQualityCallbackTables(value:unknown,sizeBits=64):boolean{return object(value)&&exact(value,ORIGINAL_QUALITY_CALLBACK_SLOTS)&&ORIGINAL_QUALITY_CALLBACK_SLOTS.every(slot=>nullable(value[slot],table=>object(table)&&exact(table,["entries","fallback"])&&list(table.entries,entry=>object(entry)&&exact(entry,["label","index","outcome"])&&nativeString(entry.label)&&integer(entry.index,entry,"index",false,sizeBits)&&isOriginalQualityOutcome(entry.outcome))&&isOriginalQualityOutcome(table.fallback)));}
export function isOriginalQualityCallbackInvocation(value:unknown,sizeBits=64):boolean{return object(value)&&exact(value,["slot","label","index","entry_index","used_fallback","outcome"])&&ORIGINAL_QUALITY_CALLBACK_SLOTS.includes(value.slot as OriginalQualityCallbackSlot)&&nativeString(value.label)&&integer(value.index,value,"index",false,sizeBits)&&(value.entry_index===null||integer(value.entry_index,value,"entry_index",false,sizeBits))&&typeof value.used_fallback==="boolean"&&value.used_fallback===(value.entry_index===null)&&isOriginalQualityOutcome(value.outcome);}
type Snapshot={source:unknown;model:unknown};
export interface OriginalQualityWorkbench extends Record<string,unknown>{family:"ORIGINAL_QUALITY_WORKBENCH";version:"original-quality-workbench-v1";operation:OriginalQualityNativeString;source_kind:OriginalQualityNativeString|null;model_kind:OriginalQualityNativeString;call_form:OriginalQualityNativeString;executed:boolean;native_entered:boolean;native_returned:boolean;native_calls:OriginalQualityCall[];error:OriginalQualityError|null;originals:Array<{field:OriginalQualityNativeString;filename:OriginalQualityNativeString|null;file:boolean;original:OriginalQualityBytes}>;parsed_controls:string[];supplied_controls:Array<{name:OriginalQualityNativeString;origin:"query"|"multipart"}>;input_context:{size_t_bytes:number;int_bytes:number;input_type:OriginalQualityNativeString|null;model_type:OriginalQualityNativeString|null;source_prepared:boolean;model_prepared:boolean;source_unchanged:boolean|null;model_unchanged:boolean|null;options_unchanged:boolean|null;callback_binding_presence:Record<OriginalQualityCallbackSlot,boolean>;lazy_scans:Array<{scan_index:number;rows:number[];returned:boolean;error:OriginalQualityError|null}>};input:{before:Snapshot;after:Snapshot};submitted_options:OriginalQualityBytes|null;configured_options:Record<string,unknown>|null;supplied_options:string[];configured_callback_tables:Record<OriginalQualityCallbackSlot,unknown>|null;callback_invocations:unknown[];result:{type:string;value:unknown}|null;artifact_bytes:Record<OriginalQualityArtifact,OriginalQualityBytes|null>;artifact_errors:unknown[];}

export function originalQualitySelectedTypes(selection:OriginalQualityDispatch):{source:string|null;model:string}{return {source:selection.source_kind===null?null:selection.source_kind.startsWith("view_")?"EventDataView":({log:"EventLog",stream:"EventStream",table:"EventTable",lazy:"LazyEventTable"})[selection.source_kind as "log"|"stream"|"table"|"lazy"],model:({petri_net:"PetriNet",process_tree:"ProcessTree",pnml_document:"PnmlDocument",presentation:"PetriNetPresentation"})[selection.model_kind]};}
export function originalQualityPrimaryCall(operation:OriginalQualityOperation,selection:OriginalQualityDispatch):string{const types=originalQualitySelectedTypes(selection),spec=ORIGINAL_QUALITY_SPECS[operation],arguments_=[...(types.source===null?[]:["const "+types.source+"&"]),"const "+types.model+"&",...(spec.variant===null?[]:[spec.variant]),...(selection.call_form==="default_options"?[]:["const "+spec.options+"&"])];return operation+"("+arguments_.join(",")+")";}
export function isOriginalQualityWorkbench(value:unknown):value is OriginalQualityWorkbench{
 const fields=["family","version","operation","source_kind","model_kind","call_form","executed","native_entered","native_returned","native_calls","error","originals","parsed_controls","supplied_controls","input_context","input","submitted_options","configured_options","supplied_options","configured_callback_tables","callback_invocations","result","artifact_bytes","artifact_errors"];
 if(!object(value)||!exact(value,fields)||value.family!=="ORIGINAL_QUALITY_WORKBENCH"||value.version!=="original-quality-workbench-v1"||!nativeString(value.operation)||!nullable(value.source_kind,nativeString)||!nativeString(value.model_kind)||!nativeString(value.call_form)||!["executed","native_entered","native_returned"].every(key=>typeof value[key]==="boolean")||value.executed!==value.native_returned||value.native_returned&&!value.native_entered||!list(value.native_calls,call)||!nullable(value.error,error)||!list(value.originals,item=>object(item)&&exact(item,["field","filename","file","original"])&&nativeString(item.field)&&nullable(item.filename,nativeString)&&typeof item.file==="boolean"&&item.file===(item.filename!==null)&&isOriginalQualityBytes(item.original))||!strings(value.parsed_controls)||!strings(value.supplied_options)||!list(value.supplied_controls,item=>object(item)&&exact(item,["name","origin"])&&nativeString(item.name)&&["query","multipart"].includes(String(item.origin)))||!object(value.input_context)||!object(value.input)||!nullable(value.submitted_options,isOriginalQualityBytes)||!nullable(value.configured_options,object)||!object(value.artifact_bytes)||!exact(value.artifact_bytes,ORIGINAL_QUALITY_ARTIFACTS)||!ORIGINAL_QUALITY_ARTIFACTS.every(name=>nullable((value.artifact_bytes as Record<string,unknown>)[name],item=>isOriginalQualityBytes(item)&&sameString(item.dataset,name+".json")))||!Array.isArray(value.artifact_errors)||value.artifact_errors.length!==0)return false;
 const c=value.input_context,sizeBits=c.size_t_bytes===4?32:c.size_t_bytes===8?64:0,intBits=c.int_bytes===4?32:c.int_bytes===8?64:0;
 if(!sizeBits||!intBits||!exact(c,["size_t_bytes","int_bytes","input_type","model_type","source_prepared","model_prepared","source_unchanged","model_unchanged","options_unchanged","callback_binding_presence","lazy_scans"])||!nullable(c.input_type,nativeString)||!nullable(c.model_type,nativeString)||!["source_prepared","model_prepared"].every(key=>typeof c[key]==="boolean")||!["source_unchanged","model_unchanged","options_unchanged"].every(key=>c[key]===null||typeof c[key]==="boolean")||!object(c.callback_binding_presence)||!exact(c.callback_binding_presence,ORIGINAL_QUALITY_CALLBACK_SLOTS)||!ORIGINAL_QUALITY_CALLBACK_SLOTS.every(slot=>typeof (c.callback_binding_presence as Record<string,unknown>)[slot]==="boolean")||!list(c.lazy_scans,item=>object(item)&&exact(item,["scan_index","rows","returned","error"])&&integer(item.scan_index,item,"scan_index",false,sizeBits)&&Array.isArray(item.rows)&&item.rows.every((row,index)=>integer(row,item.rows as unknown[],index,false,sizeBits))&&typeof item.returned==="boolean"&&nullable(item.error,error))||!nullable(value.configured_callback_tables,item=>isOriginalQualityCallbackTables(item,sizeBits))||!list(value.callback_invocations,item=>isOriginalQualityCallbackInvocation(item,sizeBits)))return false;
 const operation=typeof value.operation==="string"&&Object.hasOwn(ORIGINAL_QUALITY_SPECS,value.operation)?value.operation as OriginalQualityOperation:null;
 const sourceKind=typeof value.source_kind==="string"&&["log","stream","table","lazy","view_log","view_stream","view_table","view_lazy"].includes(value.source_kind)?value.source_kind as OriginalQualitySourceKind:null;
 const modelKind=typeof value.model_kind==="string"&&["petri_net","process_tree","pnml_document","presentation"].includes(value.model_kind)?value.model_kind as OriginalQualityModelKind:null;
 const snapshot=(item:unknown):item is Snapshot=>object(item)&&exact(item,["source","model"])&&(item.source===null||sourceKind!==null&&isOriginalQualitySource(sourceKind,item.source,sizeBits))&&(item.model===null||modelKind!==null&&isOriginalQualityModel(modelKind,item.model,sizeBits,intBits));
 if(!exact(value.input,["before","after"])||!snapshot(value.input.before)||!snapshot(value.input.after)||!nullable(value.result,item=>object(item)&&exact(item,["type","value"])&&typeof item.type==="string"&&Object.values(ORIGINAL_QUALITY_SPECS).some(spec=>spec.result===item.type)&&isOriginalQualityNativeType(item.type,item.value,undefined,undefined,sizeBits,intBits)))return false;
 if(value.configured_options!==null&&(!operation||!isOriginalQualityNativeType(ORIGINAL_QUALITY_SPECS[operation].options,value.configured_options,undefined,undefined,sizeBits,intBits)))return false;
 if(value.configured_callback_tables!==null&&!ORIGINAL_QUALITY_CALLBACK_SLOTS.every(slot=>(c.callback_binding_presence as Record<string,unknown>)[slot]===((value.configured_callback_tables as Record<string,unknown>)[slot]!==null)))return false;
 if(value.error!==null)return true;
 if(!operation||!modelKind||!(ORIGINAL_QUALITY_DISPATCH[operation] as readonly OriginalQualityDispatch[]).some(item=>item.source_kind===value.source_kind&&item.model_kind===modelKind&&item.call_form===value.call_form)||!value.native_entered||!value.native_returned||!object(value.result)||value.result.type!==ORIGINAL_QUALITY_SPECS[operation].result||value.input.before.model===null||value.input.after.model===null||c.model_prepared!==true||c.model_unchanged!==true)return false;
 if(sourceKind===null?(operation!=="evaluate_simplicity"||value.input.before.source!==null||value.input.after.source!==null||c.source_prepared!==false||c.source_unchanged!==null):(value.input.before.source===null||value.input.after.source===null||c.source_prepared!==true||c.source_unchanged!==true))return false;
 if(value.call_form==="default_options"?(value.configured_options!==null||value.submitted_options!==null||value.supplied_options.length!==0||c.options_unchanged!==null):(value.configured_options===null||c.options_unchanged!==true))return false;
 if((operation==="evaluate_replay_fitness"&&value.call_form==="with_options")!==(value.configured_callback_tables!==null))return false;
 const primary=(value.native_calls as OriginalQualityCall[]).filter(item=>item.stage==="primary");
 const originals=value.originals as OriginalQualityWorkbench["originals"],artifacts=value.artifact_bytes as OriginalQualityWorkbench["artifact_bytes"];
 const selection={source_kind:sourceKind,model_kind:modelKind,call_form:value.call_form as OriginalQualityDispatch["call_form"]},types=originalQualitySelectedTypes(selection);
 return primary.length===1&&sameString(primary[0].name,originalQualityPrimaryCall(operation,selection))&&sameString(primary[0].input_kind,types.source===null?types.model:types.source+"+"+types.model)&&c.input_type===types.source&&c.model_type===types.model&&primary[0].returned&&primary[0].error===null&&originals.some(item=>sameString(item.field,"model_file"))&&(sourceKind===null||originals.some(item=>sameString(item.field,"source_file")))&&artifacts.full_result_json!==null&&artifacts.model_input_json!==null&&artifacts.model_after_json!==null&&(sourceKind===null||artifacts.source_input_json!==null&&artifacts.source_after_json!==null);
}
const hex=(bytes:Uint8Array):string=>Array.from(bytes,byte=>byte.toString(16).padStart(2,"0")).join("");
const fromHex=(raw:string):Uint8Array<ArrayBuffer>=>Uint8Array.from(raw.match(/../g)??[],pair=>parseInt(pair,16));
export interface OriginalQualityRequestOriginal extends OriginalEditDistanceRequestOriginal {content_type:string|null;}
export interface OriginalQualityRun {value:OriginalQualityWorkbench;raw:string;http_status:number;tables:DiagnosticTable[];request:{path:"/api/quality-workbench";operation:OriginalQualityOperation;source_kind:OriginalQualitySourceKind|null;model_kind:OriginalQualityModelKind;call_form:OriginalQualityDispatch["call_form"];originals:OriginalQualityRequestOriginal[]};uploads:Partial<Record<OriginalQualityRole,File>>;controls:OriginalQualitySubmittedControl[];}
export async function runOriginalQualityWorkbench(uploads:Partial<Record<OriginalQualityRole,File>>,form:OriginalQualityForm,signal?:AbortSignal):Promise<OriginalQualityRun>{
 signal?.throwIfAborted();const operation=form.operation,controls=originalQualityControls(form),files={...uploads},roles:OriginalQualityRole[]=form.source_kind===null?["model"]:["source","model"];
 if(!(files.model instanceof File)||form.source_kind!==null&&!(files.source instanceof File))throw new Error("Choose the independent required source and model Files.");
 if(form.source_kind===null&&files.source)throw new Error("source_file is inapplicable to model-only simplicity.");
 const body=new FormData(),originals:OriginalQualityRequestOriginal[]=[];
 const capture=async(field:string,bytes:Uint8Array<ArrayBuffer>,localFile:File|null)=>{const transported=body.get(field),wireFile=typeof transported!=="string"&&transported!==null?transported:null;originals.push({field,filename:wireFile?await originalEditDistanceWireFilename(wireFile.name):null,local_filename:localFile?.name??null,local_last_modified:localFile?.lastModified??null,content_type:wireFile?wireFile.type||"application/octet-stream":null,bytes:bytes.length,sha256:await miningSourceHash(new File([bytes],field)),encoding:"hex",data:hex(bytes)});signal?.throwIfAborted();};
 for(const control of controls){const bytes=control.file?new Uint8Array(await control.file.arrayBuffer()):nativeStringBytes(control.text);if(control.file)body.append(control.name,control.file,control.file.name);else appendNativeMultipartField(body,control.name,control.text);await capture(control.name,bytes,control.file);}
 for(const role of roles){const file=files[role]!;body.append(role+"_file",file,file.name);await capture(role+"_file",new Uint8Array(await file.arrayBuffer()),file);}
 const request:OriginalQualityRun["request"]={path:"/api/quality-workbench",operation,source_kind:form.source_kind,model_kind:form.model_kind,call_form:form.call_form,originals},response=await backendFetch(request.path,{method:"POST",body,signal}),value=await readNativeResponse(response,{path:request.path,fields:controls.filter(item=>!item.file).map(item=>[item.name,item.text])});signal?.throwIfAborted();
 if(!isOriginalQualityWorkbench(value))throw new Error(object(value)&&typeof value.error==="string"?value.error:"The server did not return a complete owned quality response (HTTP "+response.status+").");
 const transportFailure=!response.ok&&value.error?.phase==="configuration_error"&&!value.native_entered&&!value.native_returned&&value.originals.length===0&&value.native_calls.length===0&&value.parsed_controls.length===0&&value.supplied_controls.length===0&&value.configured_options===null&&value.submitted_options===null&&value.result===null&&Object.values(value.input.before).every(item=>item===null)&&Object.values(value.input.after).every(item=>item===null)&&Object.values(value.artifact_bytes).every(item=>item===null);
 if(!transportFailure&&(value.operation!==operation||value.source_kind!==form.source_kind||value.model_kind!==form.model_kind||value.call_form!==form.call_form||value.originals.length!==originals.length||value.supplied_controls.length!==controls.length||value.supplied_controls.some((item,index)=>item.origin!=="multipart"||!sameString(item.name,controls[index].name))))throw new Error("The quality response changed selected overload or control ownership.");
 if(!transportFailure)for(let index=0;index<originals.length;++index){const submitted=originals[index],actual=value.originals[index];if(!sameString(actual.field,submitted.field)||actual.original.sha256!==submitted.sha256||BigInt(String(nativeCell(actual.original,"bytes")))!==BigInt(submitted.bytes)||!equalBytes(originalEmbeddingFilterOriginalBytes(actual.original),fromHex(submitted.data))||actual.file!==(submitted.filename!==null)||(submitted.filename===null?actual.filename!==null:actual.filename===null||!sameString(actual.filename,submitted.filename))||!sameString(actual.original.dataset,submitted.filename??submitted.field))throw new Error("The quality response changed original bytes, hash, order, dataset or filename presence.");}
 if(!transportFailure){const submitted=originals.find(item=>item.field==="options_json"),actual=value.submitted_options;if(submitted?actual===null||actual.sha256!==submitted.sha256||!sameString(actual.dataset,submitted.filename??submitted.field)||!equalBytes(originalEmbeddingFilterOriginalBytes(actual),fromHex(submitted.data)):actual!==null)throw new Error("The quality response changed submitted options bytes or presence.");}
 for(const carrier of [...Object.values(value.artifact_bytes),value.submitted_options].filter((item):item is OriginalQualityBytes=>item!==null)){const bytes=originalEmbeddingFilterOriginalBytes(carrier);if(BigInt(String(nativeCell(carrier,"bytes")))!==BigInt(bytes.length)||await miningSourceHash(new File([bytes],"carrier"))!==carrier.sha256)throw new Error("The quality response changed authoritative carrier bytes or hash.");signal?.throwIfAborted();}
 const raw=nativeResponseJson(value),paths:Record<OriginalQualityArtifact,string[]>={source_input_json:["input","before","source"],model_input_json:["input","before","model"],source_after_json:["input","after","source"],model_after_json:["input","after","model"],configured_options_json:["configured_options"],configured_callback_tables_json:["configured_callback_tables"],callback_invocations_json:["callback_invocations"],full_result_json:["result"]};
 for(const name of ORIGINAL_QUALITY_ARTIFACTS){const fragment=originalRootCauseRawMember(raw,paths[name]),carrier=value.artifact_bytes[name],applicable=name!=="callback_invocations_json"||value.configured_callback_tables!==null;if(fragment==="null"?carrier!==null:applicable?(carrier===null||!equalBytes(originalEmbeddingFilterOriginalBytes(carrier),nativeStringBytes(fragment))):carrier!==null)throw new Error("The canonical "+name+" artifact differs from its actual response fragment or presence.");}
 if(!response.ok&&value.error===null)throw new Error("The server returned a native result with failure status "+response.status+".");
 return {value,raw,http_status:response.status,request,uploads:files,controls,tables:originalQualityTables(value,originals)};
}
export function originalQualityTables(value:OriginalQualityWorkbench,originals?:OriginalQualityRequestOriginal[]):DiagnosticTable[]{const tables=nativeObjectTables(value);for(const table of tables)if(table.records?.some(item=>item===null||typeof item!=="object"))table.records=table.records.map((item,index)=>object(item)||Array.isArray(item)?item:parseNativeJson('{"value":'+(typeof item==="number"?nativeCell(table.records!,index):nativeJson(item))+'}'));if(value.result)tables.push({id:"complete_native_result",title:"Complete "+value.result.type+" storage",columns:["type","value"],rows:[[value.result.type,value.result.value]],records:[value.result]});if(originals)tables.push({id:"exact_request_originals",title:"Every original control and role byte",columns:["field","wire_filename","local_filename","content_type","bytes","sha256","hex"],rows:originals.map(item=>[item.field,item.filename,item.local_filename,item.content_type,item.bytes,item.sha256,item.data]),records:originals});return tables;}
export const originalQualityReport=(run:OriginalQualityRun):string=>nativeJson({family:"ORIGINAL_QUALITY_WORKBENCH_REPORT",http_status:run.http_status,request:run.request,raw_native_response:run.raw,native_response:run.value},2);
export function originalQualityCarrierFile(carrier:OriginalQualityBytes):File{return new File([originalEmbeddingFilterOriginalBytes(carrier)],"minera-quality-"+originalEditDistanceFileName(originalEmbeddingFilterStringText(carrier.dataset)),{type:"application/json"});}
export function originalQualityRequestFile(run:OriginalQualityRun,index:number):File{const item=run.request.originals[index];if(!item)throw new Error("Choose an owned request part.");return new File([fromHex(item.data)],item.local_filename??item.filename??item.field+".txt",{type:item.content_type??"application/octet-stream",...(item.local_last_modified===null?{}:{lastModified:item.local_last_modified})});}
export function originalQualityResultFile(run:OriginalQualityRun):File{const carrier=run.value.artifact_bytes.full_result_json;if(!carrier)throw new Error("No complete canonical result was returned.");return originalQualityCarrierFile(carrier);}
export interface OriginalQualityReusable {label:string;operation:OriginalQualityOperation;dispatch:OriginalQualityDispatch;role:OriginalQualityRole|"options"|"callbacks";file:File;}
export function originalQualityReusableInputs(run:OriginalQualityRun):OriginalQualityReusable[]{const entries:OriginalQualityReusable[]=[],a=run.value.artifact_bytes;for(const name of ["source_input_json","source_after_json","model_input_json","model_after_json"] as const){const carrier=a[name];if(!carrier)continue;const role:OriginalQualityRole=name.startsWith("source")?"source":"model";for(const operation of ORIGINAL_QUALITY_OPERATIONS)for(const dispatch of ORIGINAL_QUALITY_DISPATCH[operation] as readonly OriginalQualityDispatch[]){const compatible=role==="source"?dispatch.source_kind!==null&&dispatch.source_kind.replace(/^view_/,"")===run.request.source_kind?.replace(/^view_/,""):dispatch.model_kind===run.request.model_kind;if(compatible)entries.push({label:"Use actual "+name+" as "+role+" for "+operation+" · "+(dispatch.source_kind??"model only")+" / "+dispatch.model_kind+" / "+dispatch.call_form,operation,dispatch,role,file:originalQualityCarrierFile(carrier)});}}
 for(const [artifact,role] of [["configured_options_json","options"],["configured_callback_tables_json","callbacks"]] as const){const carrier=a[artifact];if(!carrier)continue;const dispatch=(ORIGINAL_QUALITY_DISPATCH[run.request.operation] as readonly OriginalQualityDispatch[]).find(item=>item.source_kind===run.request.source_kind&&item.model_kind===run.request.model_kind&&item.call_form==="with_options");if(dispatch)entries.push({label:"Reuse complete actual "+role+" as File",operation:run.request.operation,dispatch,role,file:originalQualityCarrierFile(carrier)});}return entries;
}
export const originalQualityEditMember=originalRootCauseEditMember;
export function originalQualityEditPath(raw:string,path:readonly string[],replacement?:string):string{if(path.length===0)throw new Error("Choose an option member path.");const value=parseNativeJson(raw);if(!object(value))throw new Error("Native options must be a JSON object.");const name=path[0],next=path.length===1?replacement:originalQualityEditPath(Object.hasOwn(value,name)&&value[name]!==null?nativeJson(value[name]):"{}",path.slice(1),replacement);return originalRootCauseEditMember(raw,name,[name],next);}
export const DEFAULT_METADATA={extensions:[],classifiers:[],trace_globals:[],event_globals:[],properties:[],globals:[],compatibility_snapshot_valid:false,compatibility_globals_snapshot:[],compatibility_trace_globals_snapshot:[],compatibility_event_globals_snapshot:[]};
export const ORIGINAL_QUALITY_SOURCE_EXAMPLES={
 "log": {
  "attributes": [],
  "metadata": {
   "extensions": [],
   "classifiers": [],
   "trace_globals": [],
   "event_globals": [],
   "properties": [],
   "globals": [],
   "compatibility_snapshot_valid": false,
   "compatibility_globals_snapshot": [],
   "compatibility_trace_globals_snapshot": [],
   "compatibility_event_globals_snapshot": []
  },
  "traces": [
   {
    "case_id": "case-1",
    "case_id_state_index": 1,
    "case_id_source_key": "case:concept:name",
    "case_id_attribute": null,
    "attributes": [],
    "properties": [],
    "events": [
     {
      "case_id": "case-1",
      "activity": "A",
      "timestamp": "",
      "resource": "",
      "lifecycle": "",
      "input_order": 0,
      "attributes": [],
      "case_id_state_index": 1,
      "activity_state_index": 1,
      "timestamp_state_index": 0,
      "resource_state_index": 0,
      "lifecycle_state_index": 0,
      "case_id_source_key": "case:concept:name",
      "activity_source_key": "concept:name",
      "timestamp_source_key": "",
      "resource_source_key": "",
      "lifecycle_source_key": "",
      "activity_attribute": null,
      "timestamp_attribute": null,
      "resource_attribute": null,
      "lifecycle_attribute": null
     }
    ]
   }
  ]
 },
 "stream": {
  "attributes": [],
  "metadata": {
   "extensions": [],
   "classifiers": [],
   "trace_globals": [],
   "event_globals": [],
   "properties": [],
   "globals": [],
   "compatibility_snapshot_valid": false,
   "compatibility_globals_snapshot": [],
   "compatibility_trace_globals_snapshot": [],
   "compatibility_event_globals_snapshot": []
  },
  "events": [
   {
    "fields": [
     {
      "key": "case:concept:name",
      "value": "case-1",
      "kind_index": 0,
      "container_index": 0,
      "null": false,
      "scalar_wrapper": false,
      "scalar_children_list": false,
      "first_class_role_index": 0,
      "first_class_state_index": 0,
      "first_class_source_key": "",
      "typed_value": {
       "index": 0,
       "value": null
      },
      "children": []
     },
     {
      "key": "concept:name",
      "value": "A",
      "kind_index": 0,
      "container_index": 0,
      "null": false,
      "scalar_wrapper": false,
      "scalar_children_list": false,
      "first_class_role_index": 0,
      "first_class_state_index": 0,
      "first_class_source_key": "",
      "typed_value": {
       "index": 0,
       "value": null
      },
      "children": []
     }
    ]
   }
  ]
 },
 "table": {
  "native_byte_strings": "hex-v1",
  "native_schema": "event-table-raw-v1",
  "value": {
   "columns": [
    "636173653a636f6e636570743a6e616d65",
    "636f6e636570743a6e616d65"
   ],
   "rows": [
    [
     {
      "key": "636173653a636f6e636570743a6e616d65",
      "value": "636173652d31",
      "kind_index": 0,
      "container_index": 0,
      "null": false,
      "scalar_wrapper": false,
      "scalar_children_list": false,
      "first_class_role_index": 0,
      "first_class_state_index": 0,
      "first_class_source_key": "",
      "typed_value": {
       "index": 0,
       "value": null
      },
      "children": []
     },
     {
      "key": "636f6e636570743a6e616d65",
      "value": "41",
      "kind_index": 0,
      "container_index": 0,
      "null": false,
      "scalar_wrapper": false,
      "scalar_children_list": false,
      "first_class_role_index": 0,
      "first_class_state_index": 0,
      "first_class_source_key": "",
      "typed_value": {
       "index": 0,
       "value": null
      },
      "children": []
     }
    ]
   ],
   "attributes": [],
   "metadata": {
    "extensions": [],
    "classifiers": [],
    "trace_globals": [],
    "event_globals": [],
    "properties": [],
    "globals": [],
    "compatibility_snapshot_valid": false,
    "compatibility_globals_snapshot": [],
    "compatibility_trace_globals_snapshot": [],
    "compatibility_event_globals_snapshot": []
   },
   "dtypes": [
    "6f626a656374",
    "6f626a656374"
   ],
   "index": null,
   "categorical_dtypes": []
  }
 },
 "lazy": {
  "table": {
   "native_byte_strings": "hex-v1",
   "native_schema": "event-table-raw-v1",
   "value": {
    "columns": [
     "636173653a636f6e636570743a6e616d65",
     "636f6e636570743a6e616d65"
    ],
    "rows": [
     [
      {
       "key": "636173653a636f6e636570743a6e616d65",
       "value": "636173652d31",
       "kind_index": 0,
       "container_index": 0,
       "null": false,
       "scalar_wrapper": false,
       "scalar_children_list": false,
       "first_class_role_index": 0,
       "first_class_state_index": 0,
       "first_class_source_key": "",
       "typed_value": {
        "index": 0,
        "value": null
       },
       "children": []
      },
      {
       "key": "636f6e636570743a6e616d65",
       "value": "41",
       "kind_index": 0,
       "container_index": 0,
       "null": false,
       "scalar_wrapper": false,
       "scalar_children_list": false,
       "first_class_role_index": 0,
       "first_class_state_index": 0,
       "first_class_source_key": "",
       "typed_value": {
        "index": 0,
        "value": null
       },
       "children": []
      }
     ]
    ],
    "attributes": [],
    "metadata": {
     "extensions": [],
     "classifiers": [],
     "trace_globals": [],
     "event_globals": [],
     "properties": [],
     "globals": [],
     "compatibility_snapshot_valid": false,
     "compatibility_globals_snapshot": [],
     "compatibility_trace_globals_snapshot": [],
     "compatibility_event_globals_snapshot": []
    },
    "dtypes": [
     "6f626a656374",
     "6f626a656374"
    ],
    "index": null,
    "categorical_dtypes": []
   }
  },
  "valid": true,
  "row_count": 1
 }
} as const;
export const ORIGINAL_QUALITY_MODEL_EXAMPLES={
 "petri_net": {
  "place_count": 2,
  "source_storage": 0,
  "sink_storage": 1,
  "has_source": true,
  "has_sink": true,
  "activities": [
   "A"
  ],
  "transitions": [
   {
    "activity": 0,
    "inputs": [
     0
    ],
    "outputs": [
     1
    ],
    "reset_inputs": [],
    "inhibitor_inputs": [],
    "reset_weights": [],
    "inhibitor_weights": [],
    "input_weights": [
     {
      "magnitude": 1,
      "negative": false
     }
    ],
    "output_weights": [
     {
      "magnitude": 1,
      "negative": false
     }
    ]
   }
  ],
  "discovery_markings": null,
  "transition_names": null
 },
 "process_tree": {
  "activities": [
   "A"
  ],
  "root": {
   "op": {
    "name": "ACTIVITY",
    "index": 0
   },
   "activity": 0,
   "children": []
  }
 },
 "pnml_document": {
  "net": {
   "place_count": 2,
   "source_storage": 0,
   "sink_storage": 1,
   "has_source": true,
   "has_sink": true,
   "activities": [
    "A"
   ],
   "transitions": [
    {
     "activity": 0,
     "inputs": [
      0
     ],
     "outputs": [
      1
     ],
     "reset_inputs": [],
     "inhibitor_inputs": [],
     "reset_weights": [],
     "inhibitor_weights": [],
     "input_weights": [
      {
       "magnitude": 1,
       "negative": false
      }
     ],
     "output_weights": [
      {
       "magnitude": 1,
       "negative": false
      }
     ]
    }
   ],
   "discovery_markings": null,
   "transition_names": null
  },
  "initial_marking": [
   1,
   0
  ],
  "final_marking": [
   0,
   1
  ],
  "stochastic_map": null,
  "place_ids": [
   "p0",
   "p1"
  ],
  "transition_ids": [
   "t0"
  ],
  "initial_marking_keys": [
   true,
   false
  ],
  "final_marking_keys": [
   false,
   true
  ],
  "arc_occurrences": null
 },
 "presentation": {
  "name": "A",
  "properties": [],
  "places": [
   {
    "id": 0,
    "name": "p0",
    "properties": []
   },
   {
    "id": 1,
    "name": "p1",
    "properties": []
   }
  ],
  "transitions": [
   {
    "id": 0,
    "name": "t0",
    "label": "A",
    "properties": []
   }
  ],
  "arcs": [
   {
    "source": {
     "kind": {
      "name": "PLACE",
      "index": 0
     },
     "id": 0
    },
    "target": {
     "kind": {
      "name": "TRANSITION",
      "index": 1
     },
     "id": 0
    },
    "weight": 1,
    "properties": [],
    "negative_weight": false
   },
   {
    "source": {
     "kind": {
      "name": "TRANSITION",
      "index": 1
     },
     "id": 0
    },
    "target": {
     "kind": {
      "name": "PLACE",
      "index": 0
     },
     "id": 1
    },
    "weight": 1,
    "properties": [],
    "negative_weight": false
   }
  ],
  "initial_marking": [
   {
    "place_id": 0,
    "tokens": 1
   }
  ],
  "final_marking": [
   {
    "place_id": 1,
    "tokens": 1
   }
  ]
 }
} as const;
export const ORIGINAL_QUALITY_NATIVE_DEFAULTS={
 "ReplayFitnessOptions": {
  "consider_activities_not_in_model_in_fitness": "false",
  "activity_attribute": "\"concept:name\"",
  "token_replay_variant": "TokenReplayVariant::kForward",
  "alignment_options": "{ AlignmentVariant::kDijkstraLessMemory, AlignmentCostProfile::kStandard}",
  "decomposed_alignment_options": "value-initialized native storage",
  "initial_marking": "value-initialized native storage",
  "final_marking": "value-initialized native storage",
  "case_id_attribute": "\"case:concept:name\"",
  "max_rows_to_materialize": "value-initialized native storage",
  "cleaning_token_flood": "false",
  "exact_initial_marking": "value-initialized native storage",
  "exact_final_marking": "value-initialized native storage",
  "initial_marking_keys": "value-initialized native storage",
  "final_marking_keys": "value-initialized native storage",
  "place_names": "value-initialized native storage",
  "transition_names": "value-initialized native storage"
 },
 "PrecisionOptions": {
  "use_task_ids": "false",
  "max_states": "100000",
  "activity_attribute": "\"concept:name\"",
  "token_replay_variant": "TokenReplayVariant::kForward",
  "initial_marking": "value-initialized native storage",
  "final_marking": "value-initialized native storage",
  "case_id_attribute": "\"case:concept:name\"",
  "max_rows_to_materialize": "value-initialized native storage",
  "exact_initial_marking": "value-initialized native storage",
  "exact_final_marking": "value-initialized native storage",
  "initial_marking_keys": "value-initialized native storage",
  "final_marking_keys": "value-initialized native storage",
  "place_names": "value-initialized native storage",
  "transition_names": "value-initialized native storage"
 },
 "GeneralizationOptions": {
  "activity_attribute": "\"concept:name\"",
  "initial_marking": "value-initialized native storage",
  "final_marking": "value-initialized native storage",
  "case_id_attribute": "\"case:concept:name\"",
  "max_rows_to_materialize": "value-initialized native storage",
  "exact_initial_marking": "value-initialized native storage",
  "exact_final_marking": "value-initialized native storage",
  "initial_marking_keys": "value-initialized native storage",
  "final_marking_keys": "value-initialized native storage",
  "place_names": "value-initialized native storage",
  "transition_names": "value-initialized native storage"
 },
 "SimplicityOptions": {
  "initial_marking": "value-initialized native storage",
  "final_marking": "value-initialized native storage",
  "max_states": "100000",
  "arc_degree_k": "2.0",
  "presentation_initial_marking": "value-initialized native storage"
 },
 "CompositeEvaluationOptions": {
  "fitness_weight": "0.25",
  "precision_weight": "0.25",
  "simplicity_weight": "0.25",
  "generalization_weight": "0.25",
  "activity_attribute": "\"concept:name\"",
  "initial_marking": "value-initialized native storage",
  "final_marking": "value-initialized native storage",
  "case_id_attribute": "\"case:concept:name\"",
  "max_rows_to_materialize": "value-initialized native storage",
  "exact_initial_marking": "value-initialized native storage",
  "exact_final_marking": "value-initialized native storage",
  "initial_marking_keys": "value-initialized native storage",
  "final_marking_keys": "value-initialized native storage",
  "place_names": "value-initialized native storage",
  "transition_names": "value-initialized native storage"
 },
 "ReplayFitnessResult": {
  "variant": "ReplayFitnessVariant::TOKEN_BASED",
  "fitness": "0.0",
  "log_fitness": "0.0",
  "average_trace_fitness": "0.0",
  "percentage_fit_traces": "0.0",
  "fitting_traces": "0",
  "trace_count": "0",
  "total_cost": "0.0",
  "computed": "true",
  "exhausted": "false",
  "error": "value-initialized native storage",
  "token_result": "value-initialized native storage",
  "alignment_result": "value-initialized native storage",
  "decomposed_alignment_result": "value-initialized native storage",
  "log_fitness_is_integer_zero": "false",
  "token_result_is_fitness_only": "false"
 },
 "PrecisionResult": {
  "variant": "PrecisionVariant::ETCONFORMANCE_TOKEN",
  "precision": "1.0",
  "computed": "true",
  "exhausted": "false",
  "error": "value-initialized native storage",
  "token_result": "value-initialized native storage",
  "alignment_result": "value-initialized native storage",
  "alignment_detail_present": "false",
  "token_result_is_precision_only": "false"
 },
 "GeneralizationResult": {
  "variant": "GeneralizationVariant::GENERALIZATION_TOKEN",
  "generalization": "0.0",
  "executed_transitions": "0",
  "transition_count": "0",
  "transition_frequencies": "value-initialized native storage",
  "computed": "true",
  "error": "value-initialized native storage"
 },
 "SimplicityResult": {
  "variant": "SimplicityVariant::ARC_DEGREE",
  "simplicity": "0.0",
  "complexity": "0.0",
  "value": "0.0",
  "vertices": "0",
  "edges": "0",
  "strongly_connected_components": "0",
  "computed": "true",
  "exhausted": "false",
  "error": "value-initialized native storage"
 },
 "CompositeEvaluationResult": {
  "fitness": "0.0",
  "precision": "0.0",
  "generalization": "0.0",
  "simplicity": "0.0",
  "metrics_average_weight": "0.0",
  "fscore": "0.0",
  "fitness_weight": "0.25",
  "precision_weight": "0.25",
  "simplicity_weight": "0.25",
  "generalization_weight": "0.25",
  "computed": "true",
  "error": "value-initialized native storage",
  "fitness_details": "value-initialized native storage",
  "fitness_score": "0.0"
 },
 "AlignmentOptions": {
  "variant": "AlignmentVariant::kDijkstraLessMemory",
  "cost_profile": "AlignmentCostProfile::kLegacyUnit",
  "max_states": "200000",
  "max_time_seconds": "0.0",
  "max_total_time_seconds": "0.0",
  "max_alignments": "32",
  "exponent": "2.0",
  "synchronous_dijkstra": "true",
  "default_trace_cost": "value-initialized native storage",
  "default_model_cost": "value-initialized native storage",
  "default_sync_cost": "value-initialized native storage",
  "default_silent_cost": "value-initialized native storage",
  "trace_costs": "value-initialized native storage",
  "model_costs": "value-initialized native storage",
  "sync_costs": "value-initialized native storage",
  "trace_costs_by_position": "value-initialized native storage",
  "model_costs_by_transition": "value-initialized native storage",
  "sync_costs_by_transition": "value-initialized native storage",
  "trace_cost_function": "value-initialized native storage",
  "model_cost_function": "value-initialized native storage",
  "sync_cost_function": "value-initialized native storage",
  "activity_attribute": "\"concept:name\"",
  "initial_marking": "value-initialized native storage",
  "final_marking": "value-initialized native storage"
 },
 "AlignmentDiagnostics": {
  "visited_states": "0",
  "queued_states": "0",
  "traversed_arcs": "0",
  "lp_solved": "0",
  "closed_set_length": "0",
  "num_visited_markings": "0"
 },
 "AlignmentMove": {
  "type": "value-initialized native storage",
  "activity": "value-initialized native storage",
  "transition": "static_cast<std::size_t>(-1)"
 },
 "ProjectedAlignmentMove": {
  "activity": "value-initialized native storage",
  "transition": "static_cast<std::size_t>(-1)",
  "marking_before": "value-initialized native storage",
  "marking_after": "value-initialized native storage"
 },
 "VariantAlignment": {
  "activities": "value-initialized native storage",
  "count": "0",
  "moves": "value-initialized native storage",
  "cost": "0.0",
  "fitness": "1.0",
  "optimal_alignments": "value-initialized native storage",
  "diagnostics": "value-initialized native storage",
  "worst_cost": "0.0",
  "projected_model_moves": "value-initialized native storage",
  "complete_moves": "value-initialized native storage"
 },
 "AlignmentResult": {
  "computed": "true",
  "exhausted": "false",
  "fitness": "1.0",
  "precision": "1.0",
  "fitting_traces": "0",
  "trace_count": "0",
  "total_cost": "0.0",
  "variant": "AlignmentVariant::kDijkstraLessMemory",
  "diagnostics": "value-initialized native storage",
  "variants": "value-initialized native storage",
  "activity_attribute": "\"concept:name\""
 },
 "VariantConformance": {
  "activities": "value-initialized native storage",
  "count": "0",
  "fitness": "1.0",
  "null_activity_indices": "value-initialized native storage"
 },
 "ConformanceResult": {
  "fitness": "1.0",
  "precision": "1.0",
  "generalization": "1.0",
  "simplicity": "1.0",
  "fitting_traces": "0",
  "trace_count": "0",
  "produced": "0",
  "consumed": "0",
  "missing": "0",
  "remaining": "0",
  "transition_frequencies": "value-initialized native storage",
  "variants": "value-initialized native storage",
  "activity_attribute": "\"concept:name\"",
  "trace_diagnostics": "value-initialized native storage",
  "duration_diagnostics": "value-initialized native storage",
  "root_cause_diagnostics": "value-initialized native storage"
 },
 "TokenReplayTraceDiagnostics": {
  "trace_index": "0",
  "case_id": "value-initialized native storage",
  "trace_is_fit": "true",
  "trace_fitness": "1.0",
  "produced": "0",
  "consumed": "0",
  "missing": "0",
  "remaining": "0",
  "activated_transitions": "value-initialized native storage",
  "transitions_with_problems": "value-initialized native storage",
  "enabled_transitions": "value-initialized native storage",
  "unknown_event_indices": "value-initialized native storage",
  "unknown_activities": "value-initialized native storage",
  "unknown_activity_is_null": "value-initialized native storage",
  "reached_marking": "value-initialized native storage",
  "places": "value-initialized native storage",
  "replay_stopped": "false",
  "replayed_events": "0",
  "event_diagnostics": "value-initialized native storage",
  "reached_marking_keys": "value-initialized native storage",
  "cleaned_place_indices": "value-initialized native storage",
  "marking_domain": "value-initialized native storage",
  "reached_marking_order": "value-initialized native storage"
 },
 "TokenReplayPlaceDiagnostics": {
  "missing": "0",
  "remaining": "0",
  "consumed": "0",
  "produced": "0"
 },
 "TokenReplayEventDiagnostics": {
  "event_index": "0",
  "activated_transitions": "value-initialized native storage",
  "transitions_with_problems": "value-initialized native storage"
 },
 "TokenReplayMarkingDomain": {
  "places": "value-initialized native storage",
  "detached_places": "value-initialized native storage"
 },
 "TokenReplayDurationComparison": {
  "fit_trace_indices": "value-initialized native storage",
  "problem_trace_indices": "value-initialized native storage",
  "fit_count": "0",
  "problem_count": "0",
  "fit_median_seconds": "0.0",
  "problem_median_seconds": "0.0",
  "relative_throughput": "0.0"
 },
 "TokenReplayTransitionDurationDiagnostics": {
  "transition_index": "0"
 },
 "TokenReplayUnknownActivityDurationDiagnostics": {
  "activity": "value-initialized native storage",
  "activity_is_null": "false"
 },
 "TokenReplayDurationDiagnostics": {
  "timestamp_attribute": "\"time:timestamp\"",
  "transitions": "value-initialized native storage",
  "unknown_activities": "value-initialized native storage"
 },
 "TokenReplayTransitionRootCauseDiagnostics": {
  "transition_index": "0"
 },
 "TokenReplayUnknownRootCauseDiagnostics": {
  "activity": "value-initialized native storage",
  "activity_is_null": "false",
  "activity_attribute": "value-initialized native storage"
 },
 "TokenReplayRootCauseDiagnostics": {
  "options": "value-initialized native storage",
  "transitions": "value-initialized native storage",
  "unknown_activities": "value-initialized native storage"
 },
 "DecomposedAlignmentOptions": {
  "variant": "DecomposedAlignmentVariant::kRecomposMaximal",
  "threshold_border_agreement": "100000000.0",
  "max_time_trace": "0.0",
  "max_time_total": "0.0",
  "max_states": "200000",
  "max_total_states": "2000000",
  "use_cache": "true",
  "clear_cache": "false",
  "log_move_cost": "10000.0",
  "model_move_cost": "10000.0",
  "tau_model_cost": "1.0",
  "sync_cost": "0.0",
  "activity_log_costs": "value-initialized native storage",
  "activity_model_costs": "value-initialized native storage",
  "activity_sync_costs": "value-initialized native storage",
  "initial_marking": "value-initialized native storage",
  "final_marking": "value-initialized native storage",
  "activity_attribute": "\"concept:name\""
 },
 "DecomposedAlignmentMove": {
  "log": "value-initialized native storage",
  "model": "value-initialized native storage",
  "transition": "static_cast<std::size_t>(-1)"
 },
 "DecomposedAlignmentTrace": {
  "activities": "value-initialized native storage",
  "count": "1",
  "complete": "false",
  "timed_out": "false",
  "exhausted": "false",
  "cost": "0.0",
  "fitness": "0.0",
  "best_worst_cost": "0.0",
  "alignment": "value-initialized native storage"
 },
 "DecomposedAlignmentDiagnostics": {
  "decompositions": "0",
  "components": "0",
  "merges": "0",
  "cache_hits": "0",
  "cache_misses": "0",
  "border_disagreements": "0",
  "agreement_iterations": "0",
  "visited_states": "0",
  "queued_states": "0",
  "traversed_arcs": "0",
  "whole_net_fallback": "false",
  "note": "value-initialized native storage"
 },
 "DecomposedAlignmentResult": {
  "complete": "true",
  "timed_out": "false",
  "exhausted": "false",
  "fitness": "1.0",
  "best_worst_cost": "0.0",
  "trace_count": "0",
  "total_cost": "0.0",
  "diagnostics": "value-initialized native storage",
  "traces": "value-initialized native storage"
 },
 "PetriNetLiteralStorage": {
  "place_count": "0",
  "source_storage": "0",
  "sink_storage": "0",
  "has_source": "false",
  "has_sink": "false",
  "transitions": "value-initialized native storage",
  "activities": "value-initialized native storage",
  "discovery_markings": "value-initialized native storage",
  "transition_names": "value-initialized native storage"
 },
 "PetriTransition": {
  "activity": "value-initialized native storage",
  "inputs": "value-initialized native storage",
  "outputs": "value-initialized native storage",
  "reset_inputs": "value-initialized native storage",
  "inhibitor_inputs": "value-initialized native storage",
  "reset_weights": "value-initialized native storage",
  "inhibitor_weights": "value-initialized native storage",
  "input_weights": "value-initialized native storage",
  "output_weights": "value-initialized native storage"
 },
 "PetriArcWeight": {
  "magnitude": "1",
  "negative": "false"
 },
 "PetriNetPresentation": {
  "name": "value-initialized native storage",
  "properties": "value-initialized native storage",
  "places": "value-initialized native storage",
  "transitions": "value-initialized native storage",
  "arcs": "value-initialized native storage",
  "initial_marking": "value-initialized native storage",
  "final_marking": "value-initialized native storage"
 },
 "PetriPresentedPlace": {
  "id": "value-initialized native storage",
  "name": "value-initialized native storage",
  "properties": "value-initialized native storage"
 },
 "PetriPresentedTransition": {
  "id": "value-initialized native storage",
  "name": "value-initialized native storage",
  "label": "value-initialized native storage",
  "properties": "value-initialized native storage"
 },
 "PetriNodeRef": {
  "kind": "value-initialized native storage",
  "id": "value-initialized native storage"
 },
 "PetriPresentedArc": {
  "source": "value-initialized native storage",
  "target": "value-initialized native storage",
  "weight": "1",
  "properties": "value-initialized native storage",
  "negative_weight": "false"
 },
 "PetriSparseMarkingEntry": {
  "place_id": "value-initialized native storage",
  "tokens": "value-initialized native storage"
 },
 "PetriDiscoveryMarkings": {
  "detached_places": "value-initialized native storage",
  "initial": "value-initialized native storage",
  "final": "value-initialized native storage"
 },
 "PetriMarkingPlace": {
  "kind": "value-initialized native storage",
  "id": "value-initialized native storage"
 },
 "PetriMarkingEntry": {
  "place": "value-initialized native storage",
  "tokens": "value-initialized native storage"
 },
 "PetriDetachedMarkingPlace": {
  "id": "value-initialized native storage",
  "name": "value-initialized native storage"
 },
 "PnmlDocument": {
  "net": "value-initialized native storage",
  "initial_marking": "value-initialized native storage",
  "final_marking": "value-initialized native storage",
  "stochastic_map": "value-initialized native storage",
  "place_ids": "value-initialized native storage",
  "transition_ids": "value-initialized native storage",
  "initial_marking_keys": "value-initialized native storage",
  "final_marking_keys": "value-initialized native storage",
  "arc_occurrences": "value-initialized native storage"
 },
 "PnmlArcOccurrence": {
  "place": "value-initialized native storage",
  "transition": "value-initialized native storage",
  "input": "true",
  "type": "PetriArcType::kNormal",
  "weight": "1",
  "id": "value-initialized native storage",
  "negative_weight": "false"
 },
 "StochasticMap": {
  "transitions": "value-initialized native storage"
 },
 "StochasticMapTransition": {
  "transition": "0",
  "timing": "StochasticMapTiming::kTimed",
  "distribution": "StochasticMapDistribution::kExponential",
  "parameter": "1.0",
  "secondary_parameter": "0.0",
  "tertiary_parameter": "0.0",
  "activation_weight": "1.0",
  "priority": "0"
 },
 "EventStream": {
  "attributes": "value-initialized native storage",
  "events": "value-initialized native storage",
  "metadata": "value-initialized native storage"
 },
 "EventRecord": {
  "fields": "value-initialized native storage"
 },
 "EventTable": {
  "columns": "value-initialized native storage",
  "rows": "value-initialized native storage",
  "attributes": "value-initialized native storage",
  "metadata": "value-initialized native storage",
  "dtypes": "value-initialized native storage",
  "index": "value-initialized native storage",
  "categorical_dtypes": "value-initialized native storage"
 },
 "EventTableIndex": {
  "kind": "\"Index\"",
  "dtype": "\"int64\"",
  "names": "{std::nullopt}",
  "labels": "value-initialized native storage"
 },
 "EventTableIndexLabel": {
  "value": "value-initialized native storage"
 },
 "EventTableCategoricalDtype": {
  "categories_dtype": "value-initialized native storage",
  "categories": "value-initialized native storage",
  "ordered": "false"
 },
 "RootCauseOptions": {
  "string_attributes": "value-initialized native storage",
  "numeric_attributes": "value-initialized native storage",
  "enable_multiplier": "false"
 },
 "RootCauseRepresentation": {
  "feature_names": "value-initialized native storage",
  "data": "value-initialized native storage",
  "target": "value-initialized native storage",
  "source_trace_indices": "value-initialized native storage"
 },
 "RootCauseModel": {
  "feature_names": "value-initialized native storage",
  "classes": "value-initialized native storage",
  "data": "value-initialized native storage",
  "target": "value-initialized native storage",
  "source_trace_indices": "value-initialized native storage",
  "tree": "value-initialized native storage",
  "fit_count": "0",
  "problem_count": "0"
 },
 "RootCauseTreeNode": {
  "feature_index": "-1",
  "threshold": "-2.0",
  "left": "-1",
  "right": "-1",
  "class_counts": "{}",
  "missing_go_to_left": "false",
  "missing_only": "false"
 }
} as const;

export const originalQualityDefaultSource=(kind:OriginalQualitySourceKind="log"):string=>nativeJson(ORIGINAL_QUALITY_SOURCE_EXAMPLES[kind.replace(/^view_/,"") as keyof typeof ORIGINAL_QUALITY_SOURCE_EXAMPLES],2);
export const originalQualityDefaultModel=(kind:OriginalQualityModelKind="petri_net"):string=>nativeJson(ORIGINAL_QUALITY_MODEL_EXAMPLES[kind],2);
export const ORIGINAL_QUALITY_CALLBACK_EXAMPLE='{"trace_cost_function":{"entries":[{"label":"A","index":0,"outcome":{"value":1}}],"fallback":{"value":1}},"model_cost_function":null,"sync_cost_function":null}';
export interface OriginalQualityMemberGuide {path:string[];type:string;native_default:string;example:string;}
export function originalQualityMemberGuides(operation:OriginalQualityOperation):OriginalQualityMemberGuide[]{const result:OriginalQualityMemberGuide[]=[];const visit=(type:string,prefix:string[])=>{const schema=(ORIGINAL_QUALITY_RECORD_SCHEMAS as Record<string,Record<string,string>>)[type],defaults=(ORIGINAL_QUALITY_NATIVE_DEFAULTS as Record<string,Record<string,string>>)[type]??{};if(!schema)return;for(const [name,member] of Object.entries(schema)){const path=[...prefix,name],native_default=defaults[name]??"native value-initialized storage";let example=member.startsWith("std::optional<")?"null":member==="bool"?(native_default==="true"?"true":"false"):member==="std::string"?(native_default.startsWith('"')?native_default:'""'):Object.hasOwn(ORIGINAL_QUALITY_ENUMS,member)?nativeJson(Object.keys((ORIGINAL_QUALITY_ENUMS as Record<string,Record<string,number>>)[member])[0]):member.startsWith("std::vector<")||member.startsWith("std::unordered_map<")||member.startsWith("std::map<")||member==="TokenMarking"?"[]":member==="double"||member==="int"||member==="std::size_t"||member.endsWith("int64_t")?(/^-?[\d.]+$/.test(native_default)?native_default:"0"):member.startsWith("std::function<")?"null":"{}";if(name==="alignment_options")example="{}";result.push({path,type:member,native_default,example});const inner=member.startsWith("std::optional<")?member.slice(14,-1):member;if(Object.hasOwn(ORIGINAL_QUALITY_RECORD_SCHEMAS,inner)&&!prefix.includes(name))visit(inner,path);}};visit(ORIGINAL_QUALITY_SPECS[operation].options,[]);return result;}
