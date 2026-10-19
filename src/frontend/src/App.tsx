import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
  type Alignments,
  type AlignmentOptions,
  type DfgAlignmentOptions,
  type EditDistanceOptions,
  type EvaluationOptions,
  type ModelEvaluation,
  type PetriAnalysisOptions,
  type PetriAnalysisResult,
  type EmdOptions,
  type ConceptDriftResult,
  type ProcessTreeAlignmentOptions,
  type DecomposedAlignmentOptions,
  type PetriAntiAlignmentOptions,
  type PetriMultiAlignmentOptions,
  type ProcessTreeReductionOptions,
  DEFAULT_ALIGNMENT_OPTIONS,
  type CaseAttributeStat,
  type Decisions,
  type EventAttributeStat,
  type MineraExport,
  type OcelDiscoveryResult,
  numberFormat,
} from "./types";
import { computeLayout } from "./layout";
import { computeTreeLayout } from "./tree-layout";
import { computePetriLayout } from "./petri-layout";
import { TopBar } from "./components/TopBar";
import { Kpis } from "./components/Kpis";
import { ProcessMap, variantHighlight } from "./components/ProcessMap";
import { ProcessModel } from "./components/ProcessModel";
import { Conformance } from "./components/Conformance";
import { Resources } from "./components/Resources";
import { VariantList } from "./components/VariantList";
import { DottedChartView } from "./components/DottedChart";
import { PerformanceSpectrumView } from "./components/PerformanceSpectrum";
import { Batches } from "./components/Batches";
import { TimingDiscovery } from "./components/TimingDiscovery";
import { BehaviorAnalysis } from "./components/BehaviorAnalysis";
import { buildDottedChart } from "./dotted-chart";
import { ActivityTable } from "./components/ActivityTable";
import { Landing } from "./components/Landing";
import { EventLogView } from "./components/EventLogView";
import { EventuallyFollows } from "./components/EventuallyFollows";
import { DfgVariants } from "./components/DfgVariants";
import { DfgDiscovery } from "./components/DfgDiscovery";
import { DfgDataframe } from "./components/DfgDataframe";
import { CorrelationMining } from "./components/CorrelationMining";
import { Filters } from "./components/Filters";
import { OcelWorkspaceHub } from "./components/OcelWorkspaceHub";
import { OcelIoWorkspace } from "./components/OcelIoWorkspace";
import { RelatedLogsWorkspace } from "./components/RelatedLogsWorkspace";
import { SourceAcquisitionWorkspace } from "./components/SourceAcquisitionWorkspace";
import { StreamingWorkspace } from "./components/StreamingWorkspace";
import { ModelExchangeWorkspace } from "./components/ModelExchangeWorkspace";
import { ProcessCubeWorkspace } from "./components/ProcessCubeWorkspace";
import { TreeGenerationWorkspace } from "./components/TreeGenerationWorkspace";
import { TokenReplayWorkspace } from "./components/TokenReplayWorkspace";
import { PetriAlignmentWorkspace } from "./components/PetriAlignmentWorkspace";
import { WorkflowInputWorkspace } from "./components/WorkflowInputWorkspace";
import { GraphAlignmentWorkspace } from "./components/GraphAlignmentWorkspace";
import { InductiveInputWorkspace } from "./components/InductiveInputWorkspace";
import { ProcessTreeAlignmentWorkspace } from "./components/ProcessTreeAlignmentWorkspace";
import { PetriAntiMultiWorkspace } from "./components/PetriAntiMultiWorkspace";
import { ModelConversionWorkspace } from "./components/ModelConversionWorkspace";
import { MonteCarloWorkspace } from "./components/MonteCarloWorkspace";
import { PlayoutWorkspace } from "./components/PlayoutWorkspace";
import { OriginalDecisionWorkspace } from "./components/OriginalDecisionWorkspace";
import { OriginalDeclareWorkspace } from "./components/OriginalDeclareWorkspace";
import { OriginalLogSkeletonWorkspace } from "./components/OriginalLogSkeletonWorkspace";
import { OriginalFootprintsWorkspace } from "./components/OriginalFootprintsWorkspace";
import { OriginalGeneticMatrixWorkspace } from "./components/OriginalGeneticMatrixWorkspace";
import { OriginalEmbeddingFilterWorkspace } from "./components/OriginalEmbeddingFilterWorkspace";
import { OriginalEmdWorkspace } from "./components/OriginalEmdWorkspace";
import {OriginalBoseWorkspace} from "./components/OriginalBoseWorkspace";
import {OriginalDiscoveryWorkspace} from "./components/OriginalDiscoveryWorkspace";
import { OriginalPetriAnalysisWorkspace } from "./components/OriginalPetriAnalysisWorkspace";
import { OriginalRootCauseWorkspace } from "./components/OriginalRootCauseWorkspace";
import { OriginalAnonymizationWorkspace } from "./components/OriginalAnonymizationWorkspace";
import {OriginalQualityWorkspace} from "./components/OriginalQualityWorkspace";
import {OriginalClusteringWorkspace} from "./components/OriginalClusteringWorkspace";
import { OriginalEditDistanceWorkspace } from "./components/OriginalEditDistanceWorkspace";
import { OriginalObjectCentricPlayoutWorkspace } from "./components/OriginalObjectCentricPlayoutWorkspace";
import {ActiveMiningSource} from "./components/ActiveMiningSource";
import {activeDatasetAnalysis, activeMiningSourceText, assertMiningSourceBinding, cancelMiningRequest, classifyMiningJson, isActiveSourceMining, miningSourceDescriptor, miningSourceHash, miningSourceOptions, MiningSourceRequestGate, suggestMiningSourceForm, type ActiveMiningSourceDescriptor, type ActiveSourceMining} from "./active-mining-source";
import { requestLogQuerySchema } from "./log-query";
import { EmbeddingSearch } from "./components/EmbeddingSearch";
import { AskModel,PromptChat } from "./components/AskModel";
import { QueryWorkspace } from "./components/QueryWorkspace";
import { AnonymizationWorkspace } from "./components/AnonymizationWorkspace";
import { ClusteringWorkspace } from "./components/ClusteringWorkspace";
import { TransformationHub } from "./components/TransformationHub";
import { VisualizationExplain } from "./components/VisualizationExplain";
import { type VisualizationDraft } from "./visualization-explanation";
import {
  alignModel,
  alignDfg,
  alignEditDistance,
  evaluateModel,
  analyzePetriModel,
  petriAnalysisQuery,
  evaluationQuery,
  evaluateEmd,
  emdQuery,
  alignProcessTree,
  processTreeAlignmentQuery,
  alignDecomposed,
  alignPetriAnti,
  alignPetriMulti,
  reduceProcessTree,
  decomposedAlignmentQuery,
  petriAntiAlignmentQuery,
  petriMultiAlignmentQuery,
  processTreeReductionQuery,
  conceptDriftQuery,
  requestConceptDrift,
  type ConceptDriftOptions,
  type DecisionMiningOptions,
  checkModel,
  decideModel,
  mineActiveDataset,
  requestOcelDiscovery,
  type FilterOptions,
  type CsvOptions,
  type MinerOptions,
  type MineOptions,
  type OcelDiscoveryOptions,
} from "./api";
import {
  delimiterForFile,
  hasActiveFilters,
  hasActiveMinerOptions,
  initialTab,
  isJsonExport,
  logLifecycles,
  maxEdgeCount,
  type Tab,
} from "./app-logic";

export default function App() {
  const [data, setData] = useState<MineraExport | null>(null);
  const [ocelResult, setOcelResult] = useState<OcelDiscoveryResult | null>(null);
  const [showOcelIo, setShowOcelIo] = useState(false);
  const [showRelatedLogs, setShowRelatedLogs] = useState(false);
  const [showStreaming, setShowStreaming] = useState(false);
  const [showModelExchange, setShowModelExchange] = useState(false);
  const [showProcessCube, setShowProcessCube] = useState(false);
  const [showTreeGeneration, setShowTreeGeneration] = useState(false);
  const [showAcquisition, setShowAcquisition] = useState(false);
  const [showTokenReplay, setShowTokenReplay] = useState(false);
  const [showPetriAlignment, setShowPetriAlignment] = useState(false);
  const [showWorkflowInputs, setShowWorkflowInputs] = useState(false);
  const [showGraphAlignment, setShowGraphAlignment] = useState(false);
  const [showInductiveInputs, setShowInductiveInputs] = useState(false);
  const [showTreeAlignment, setShowTreeAlignment] = useState(false);
  const [showPetriAntiMulti, setShowPetriAntiMulti] = useState(false);
  const [showModelConversion, setShowModelConversion] = useState(false);
  const [showMonteCarlo, setShowMonteCarlo] = useState(false);
  const [showPlayout, setShowPlayout] = useState(false);
  const [showOriginalDecision, setShowOriginalDecision] = useState(false);
  const [showOriginalDeclare, setShowOriginalDeclare] = useState(false);
  const [showOriginalLogSkeleton, setShowOriginalLogSkeleton] = useState(false);
  const [showOriginalFootprints, setShowOriginalFootprints] = useState(false);
  const [showOriginalGeneticMatrix, setShowOriginalGeneticMatrix] = useState(false);
  const [showOriginalEmbeddingFilter, setShowOriginalEmbeddingFilter] = useState(false);
  const [showOriginalEmd, setShowOriginalEmd] = useState(false);
  const [showOriginalBose, setShowOriginalBose] = useState(false);
  const [showOriginalDiscovery, setShowOriginalDiscovery] = useState(false);
  const [showOriginalPetriAnalysis, setShowOriginalPetriAnalysis] = useState(false);
  const [showOriginalRootCause, setShowOriginalRootCause] = useState(false);
  const [showOriginalAnonymization, setShowOriginalAnonymization] = useState(false);
  const [showOriginalQuality, setShowOriginalQuality] = useState(false);
  const [showOriginalClustering, setShowOriginalClustering] = useState(false);
  const [showOriginalEditDistance, setShowOriginalEditDistance] = useState(false);
  const [showOriginalOcPlayout, setShowOriginalOcPlayout] = useState(false);
  const [showActiveSource, setShowActiveSource] = useState(false);
  const [pendingSourceFile, setPendingSourceFile] = useState<File | null>(null);
  const [sourceOutcome, setSourceOutcome] = useState<ActiveSourceMining | null>(null);
  const [liveSourceId, setLiveSourceId] = useState<string | null>(null);
  const liveSource = useRef<string | null>(null), sourceGate = useRef(new MiningSourceRequestGate());
  const sourceRequest = useRef<{controller: AbortController; request_id: string; posted: boolean} | null>(null);
  const ocelRequest = useRef<AbortController | null>(null);
  const [ocelFile, setOcelFile] = useState<File | null>(null);
  const [ocelDataset, setOcelDataset] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [fontsReady, setFontsReady] = useState(false);
  const [threshold, setThreshold] = useState(1);
  const [selectedVariant, setSelectedVariant] = useState<number | null>(null);
  const [dragging, setDragging] = useState(false);
  const [tab, setTab] = useState<Tab>("log");
  const [mining, setMining] = useState(false);
  const [visualizationDraft,setVisualizationDraft]=useState<{generation:number;draft:VisualizationDraft}|null>(null);
  const [discoveryModelDraft,setDiscoveryModelDraft]=useState<{generation:number;file:File;sourceId:string;activityKey?:string;inferLegacyBoundaries?:boolean}|null>(null);
  const ocelQuestions=useRef<HTMLDetailsElement|null>(null);
  // The CSV upload behind the current view, kept so filters can re-mine it.
  // null when the data came from a pre-mined JSON export (not re-minable).
  const [source, setSource] = useState<ActiveMiningSourceDescriptor & {
    delimiter?: string;
    xesActivityAttribute?: string;
    xesOptions?: CsvOptions;
    activities: string[];
    resources: string[];
    lifecycles: string[];
    attributes: EventAttributeStat[];
    caseAttributes: CaseAttributeStat[];
  } | null>(null);
  const [filtersActive, setFiltersActive] = useState(false);
  // Alignments the backend computed on demand, cached per miner for the current
  // analysis so revisiting a model never refetches. Reset whenever data changes.
  const alignCache = useRef<Record<string, Alignments>>({});
  // Decision mining, deferred and cached the same way.
  const decideCache = useRef<Record<string, Decisions>>({});
  const dfgCache = useRef<Record<string, import("./types").DfgAlignmentResult>>({});
  const editCache = useRef<Record<string, import("./types").EditDistanceResult>>({});
  const processTreeCache = useRef<Record<string, import("./types").ProcessTreeAlignmentResult>>({});
  const decomposedCache = useRef<Record<string, import("./types").DecomposedAlignmentResult>>({});
  const petriAntiCache = useRef<Record<string, import("./types").PetriAntiAlignmentResult>>({});
  const petriMultiCache = useRef<Record<string, import("./types").PetriMultiAlignmentResult>>({});
  const reductionCache = useRef<Record<string, import("./types").ProcessTreeReductionResult>>({});
  const evaluationCache = useRef<Record<string, ModelEvaluation>>({});
  const petriAnalysisCache = useRef<Record<string, PetriAnalysisResult>>({});
  const emdCache = useRef<Record<string, import("./types").EmdResult>>({});
  const conceptDriftCache = useRef<Record<string, ConceptDriftResult>>({});
  // Any result started under an older backend session may still resolve, but
  // must never repopulate caches cleared for the new session.
  const cacheGeneration = useRef(0);
  const sourceAvailable = source !== null && liveSourceId === source.source_id;
  const clearSourceCaches = () => {cacheGeneration.current += 1; alignCache.current = {}; decideCache.current = {}; dfgCache.current = {}; editCache.current = {}; processTreeCache.current = {}; decomposedCache.current = {}; petriAntiCache.current = {}; petriMultiCache.current = {}; reductionCache.current = {}; evaluationCache.current = {}; petriAnalysisCache.current = {}; emdCache.current = {}; conceptDriftCache.current = {};};
  const cancelSourceLoad = () => {sourceGate.current.invalidate(); const previous = sourceRequest.current; sourceRequest.current = null; previous?.controller.abort(); if (previous?.posted) cancelMiningRequest(previous.request_id); ocelRequest.current?.abort(); ocelRequest.current = null; setMining(false);};
  const beginSourceLoad = () => {cancelSourceLoad(); clearSourceCaches(); liveSource.current = null; setLiveSourceId(null); const ticket = sourceGate.current.issue(), controller = new AbortController(); sourceRequest.current = {controller, request_id: ticket.request_id, posted: false}; setMining(true); setError(null); return {ticket, controller};};
  useEffect(() => () => {sourceGate.current.invalidate(); const pending = sourceRequest.current; pending?.controller.abort(); if (pending?.posted) cancelMiningRequest(pending.request_id); ocelRequest.current?.abort();}, []);

  useEffect(() => {
    document.fonts.ready.then(() => setFontsReady(true));
  }, []);

  // Returns a model's alignments: embedded (CLI exports), cached, or fetched
  // from the backend (server mines defer it). Stable except across data loads.
  const requestAlignment = useCallback(
    async (minerId: string, options: AlignmentOptions = {}): Promise<Alignments> => {
      const effective = { ...DEFAULT_ALIGNMENT_OPTIONS, ...options };
      const cacheKey = `${liveSource.current ?? 'offline'}|${minerId}|${JSON.stringify(effective)}`;
      const embedded = data?.conformance?.[minerId]?.alignments;
      const isDefault =
        effective.variant === DEFAULT_ALIGNMENT_OPTIONS.variant &&
        effective.cost_profile === DEFAULT_ALIGNMENT_OPTIONS.cost_profile &&
        Object.keys(effective).length === 2;
      if (embedded && isDefault) return embedded;
      const cached = alignCache.current[cacheKey];
      if (cached) return cached;
      const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
      const result = await alignModel(minerId, options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
      alignCache.current = { ...alignCache.current, [cacheKey]: result };
      return result;
    },
    [data],
  );

  // Decision mining for a model, fetched on demand and cached like alignments.
  const requestDecisions = useCallback(async (minerId: string, options: DecisionMiningOptions = {}): Promise<Decisions> => {
    const cacheKey = `${liveSource.current ?? 'offline'}|${minerId}|${JSON.stringify(options)}`;
    const cached = decideCache.current[cacheKey];
    if (cached) return cached;
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const result = await decideModel(minerId, options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    decideCache.current = { ...decideCache.current, [cacheKey]: result };
    return result;
  }, []);

  const requestDfg = useCallback(async (options: DfgAlignmentOptions = {}) => {
    const key = String(liveSource.current) + '|' + JSON.stringify(options);
    if (dfgCache.current[key]) return dfgCache.current[key];
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const result = await alignDfg(options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    dfgCache.current = { ...dfgCache.current, [key]: result };
    return result;
  }, []);
  const requestEditDistance = useCallback(async (reference: File, options: EditDistanceOptions = {}) => {
    const key = `${liveSource.current ?? 'offline'}|${reference.name}|${reference.size}|${reference.lastModified}|${JSON.stringify(options)}`;
    if (editCache.current[key]) return editCache.current[key];
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const result = await alignEditDistance(reference, options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    editCache.current = { ...editCache.current, [key]: result };
    return result;
  }, []);
  const requestEvaluation = useCallback(async (miner: string, options: EvaluationOptions = {}) => {
    const key = `${liveSource.current ?? 'offline'}|${miner}|${evaluationQuery(options)}`;
    const cached = evaluationCache.current[key];
    if (cached) return cached;
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const result = await evaluateModel(miner, options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    evaluationCache.current = { ...evaluationCache.current, [key]: result };
    return result;
  }, []);
  const requestPetriAnalysis = useCallback(async (options: PetriAnalysisOptions = {}): Promise<PetriAnalysisResult> => {
    const key = String(liveSource.current) + '|' + petriAnalysisQuery(options);
    const cached = petriAnalysisCache.current[key];
    if (cached) return cached;
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const result = await analyzePetriModel(options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    petriAnalysisCache.current = { ...petriAnalysisCache.current, [key]: result };
    return result;
  }, []);
  const requestEmd = useCallback(async (reference: File, options: EmdOptions = {}) => {
    const key = `${liveSource.current ?? 'offline'}|${reference.name}|${reference.size}|${reference.lastModified}|${emdQuery(options)}`;
    const cached = emdCache.current[key];
    if (cached) return cached;
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const result = await evaluateEmd(reference, options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    emdCache.current = { ...emdCache.current, [key]: result };
    return result;
  }, []);
  const requestConceptDriftCached = useCallback(async (options: ConceptDriftOptions = {}): Promise<ConceptDriftResult> => {
    const key = String(liveSource.current) + '|' + conceptDriftQuery(options);
    const cached = conceptDriftCache.current[key];
    if (cached) return cached;
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const result = await requestConceptDrift(options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    if (generation !== cacheGeneration.current) throw new Error("analysis changed while concept-drift request was pending");
    conceptDriftCache.current = { ...conceptDriftCache.current, [key]: result };
    return result;
  }, []);
  const requestProcessTree = useCallback(async (options: ProcessTreeAlignmentOptions = {}) => {
    const key = String(liveSource.current) + '|' + processTreeAlignmentQuery(options);
    if (processTreeCache.current[key]) return processTreeCache.current[key];
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const result = await alignProcessTree(options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    processTreeCache.current = { ...processTreeCache.current, [key]: result };
    return result;
  }, []);
  const requestDecomposed = useCallback(async (miner: string, options: DecomposedAlignmentOptions = {}) => {
    const key = `${liveSource.current ?? 'offline'}|${miner}|${decomposedAlignmentQuery(options)}`;
    if (decomposedCache.current[key]) return decomposedCache.current[key];
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const result = await alignDecomposed(miner, options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current); decomposedCache.current = { ...decomposedCache.current, [key]: result }; return result;
  }, []);
  const requestPetriAnti = useCallback(async (miner: string, options: PetriAntiAlignmentOptions = {}) => {
    const key = `${liveSource.current ?? 'offline'}|${miner}|${petriAntiAlignmentQuery(options)}`;
    if (petriAntiCache.current[key]) return petriAntiCache.current[key];
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const result = await alignPetriAnti(miner, options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current); petriAntiCache.current = { ...petriAntiCache.current, [key]: result }; return result;
  }, []);
  const requestPetriMulti = useCallback(async (miner: string, options: PetriMultiAlignmentOptions = {}) => {
    const key = `${liveSource.current ?? 'offline'}|${miner}|${petriMultiAlignmentQuery(options)}`;
    if (petriMultiCache.current[key]) return petriMultiCache.current[key];
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const result = await alignPetriMulti(miner, options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current); petriMultiCache.current = { ...petriMultiCache.current, [key]: result }; return result;
  }, []);
  const requestReduction = useCallback(async (traceIndex: number, options: ProcessTreeReductionOptions = {}) => {
    const key = `${liveSource.current ?? 'offline'}|${traceIndex}|${processTreeReductionQuery(options)}`;
    if (reductionCache.current[key]) return reductionCache.current[key];
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const result = await reduceProcessTree(traceIndex, options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current); reductionCache.current = { ...reductionCache.current, [key]: result }; return result;
  }, []);

  // Checks an uploaded PNML or BPMN model against the mined log. A new upload
  // replaces the server's cached "imported" net, so its stale alignments and
  // decisions are dropped.
  const requestModelCheck = useCallback(async (file: File, sourceId?: string, activityKey?: string, inferLegacyBoundaries?: boolean) => {
    const generation = cacheGeneration.current, expectedSource = sourceId ?? liveSource.current;
    assertMiningSourceBinding(expectedSource, liveSource.current, generation, cacheGeneration.current);
    const result = await checkModel(file, {sourceId: expectedSource ?? undefined,activityKey,pnmlInferLegacyBoundaries:inferLegacyBoundaries});
    assertMiningSourceBinding(expectedSource, liveSource.current, generation, cacheGeneration.current);
    if (generation !== cacheGeneration.current)
      throw new Error("The active analysis changed during model checking; choose the model again.");
    cacheGeneration.current += 1;
    alignCache.current = Object.fromEntries(
      Object.entries(alignCache.current).filter(([key]) => !key.includes("|imported|")),
    );
    decideCache.current = Object.fromEntries(
      Object.entries(decideCache.current).filter(([key]) => !key.includes("|imported|")),
    );
    decomposedCache.current = Object.fromEntries(Object.entries(decomposedCache.current).filter(([key]) => !key.includes("|imported|")));
    petriAntiCache.current = Object.fromEntries(Object.entries(petriAntiCache.current).filter(([key]) => !key.includes("|imported|")));
    petriMultiCache.current = Object.fromEntries(Object.entries(petriMultiCache.current).filter(([key]) => !key.includes("|imported|")));
    evaluationCache.current = Object.fromEntries(Object.entries(evaluationCache.current).filter(([key]) => !key.includes("|imported|")));
    petriAnalysisCache.current = {};
    return result;
  }, []);

  const openData = (parsed: MineraExport) => {
    cacheGeneration.current += 1;
    setData(parsed);
    setOcelResult(null);
    setOcelFile(null);
    setOcelDataset(null);
    alignCache.current = {};
    decideCache.current = {};
    dfgCache.current = {};
    editCache.current = {};
    processTreeCache.current = {};
    decomposedCache.current = {};
    petriAntiCache.current = {};
    petriMultiCache.current = {};
    reductionCache.current = {};
    evaluationCache.current = {};
    petriAnalysisCache.current = {};
    emdCache.current = {};
    conceptDriftCache.current = {};
    setSelectedVariant(null);
    setThreshold(1);
    setError(null);
    setTab(initialTab(parsed));
  };

  // Object-centric uploads stay available so the graph projection can be
  // switched without asking the user to upload the same file again.
  const changeOcelModel = useCallback((options: OcelDiscoveryOptions) => {
    if (!ocelFile) return;
    const {ticket, controller} = beginSourceLoad();
    ocelRequest.current = controller;
    setMining(true);
    requestOcelDiscovery(ocelFile, options, controller.signal)
      .then((result) => {
        if (ocelRequest.current !== controller || !sourceGate.current.accepts(ticket, controller.signal)) return;
        cacheGeneration.current += 1;
        setOcelResult(result);
        setError(null);
      })
      .catch((cause: Error) => {
        if (ocelRequest.current === controller && !controller.signal.aborted) setError(`Could not discover ${options.variant}: ${cause.message}`);
      })
      .finally(() => { if (sourceRequest.current?.controller === controller) {sourceRequest.current = null; ocelRequest.current = null; setMining(false);} });
  }, [ocelFile]);

  const loadFile = (file: File, xesActivityAttribute?: string, xesOptions?: CsvOptions, miningOptions?: MineOptions) => {
    setShowOcelIo(false);
    setShowRelatedLogs(false);
    setShowStreaming(false);
    setShowModelExchange(false);
    setShowProcessCube(false);
    setShowTreeGeneration(false);
    setShowAcquisition(false);
    setShowTokenReplay(false);
    setShowPetriAlignment(false);
    setShowWorkflowInputs(false);
    setShowGraphAlignment(false);
    setShowInductiveInputs(false);
    setShowTreeAlignment(false);
    setShowPetriAntiMulti(false);
    setShowModelConversion(false);
    setShowMonteCarlo(false);
    setShowPlayout(false);
    setShowOriginalDecision(false);
    setShowOriginalDeclare(false);
    setShowOriginalLogSkeleton(false);
    setShowOriginalFootprints(false);
    setShowOriginalGeneticMatrix(false);
    setShowOriginalEmbeddingFilter(false);
    setShowOriginalEmd(false);
    setShowOriginalOcPlayout(false);
    setShowOriginalAnonymization(false);
    setShowActiveSource(false);
    setPendingSourceFile(file);
    const {ticket, controller} = beginSourceLoad();
    const accepts = () => sourceRequest.current?.controller === controller && sourceGate.current.accepts(ticket, controller.signal);
    (async () => {
      // Minera JSON exports open directly; object-centric JSON is discovered
      // through the local backend, and other files are mined as event logs.
      let options: MineOptions = {...xesOptions, delimiter: delimiterForFile(file.name), xesActivityAttribute, ...miningOptions};
      if (isJsonExport(file.name) && options.miningInputFormat !== "table") {
        const classified = classifyMiningJson(activeMiningSourceText(await file.arrayBuffer()));
        if (!accepts()) return;
        if (classified.kind === "offline") {
          setSource(null);
          setSourceOutcome(null);
          setPendingSourceFile(null);
          openData(classified.data);
          return;
        } else if (classified.kind === "ocel") {
          ocelRequest.current = controller;
          const result = await requestOcelDiscovery(file, { variant: "ocdfg" }, controller.signal);
          if (!accepts()) return;
          cacheGeneration.current += 1;
          setData(null);
          setOcelResult(result);
          setOcelFile(file);
          setOcelDataset(file.name);
          setSource(null);
          setSourceOutcome(null);
          setPendingSourceFile(null);
          setFiltersActive(false);
          setError(null);
          return;
        } else {
          options = {...options, ...miningSourceOptions(suggestMiningSourceForm(classified.table)), ...miningOptions, miningInputFormat: "table"};
        }
      }
        options.miningRequestId = ticket.request_id;
        const sha256 = await miningSourceHash(file);
        if (!accepts()) return;
        sourceRequest.current!.posted = true;
        const result = await mineActiveDataset(file, options, controller.signal);
        if (!accepts()) return;
        const descriptor = miningSourceDescriptor(file, sha256, options, result), analysis = activeDatasetAnalysis(result);
        liveSource.current = descriptor.source_id;
        setLiveSourceId(descriptor.source_id);
        clearSourceCaches();
        setSource({
          ...descriptor,
          delimiter: options.delimiter,
          xesActivityAttribute,
          xesOptions,
          activities: analysis?.activities.map((a) => a.name) ?? [],
          resources: analysis?.resources?.resources.map((r) => r.name) ?? [],
          lifecycles: analysis ? logLifecycles(analysis) : [],
          attributes: analysis?.event_attributes ?? [],
          caseAttributes: analysis?.case_attributes ?? [],
        });
        setSourceOutcome(isActiveSourceMining(result) ? result : null);
        setPendingSourceFile(null);
        setFiltersActive(false);
        if (analysis) openData(analysis); else {setData(null); setOcelResult(null); setOcelFile(null); setOcelDataset(null); setShowActiveSource(true); setError(null);}
    })()
      .catch((cause: Error) => {
        if (accepts()) {setError(`Could not load ${file.name}: ${cause.message}`); if (/\.json$/i.test(file.name)) setShowActiveSource(true);}
      })
      .finally(() => {if (sourceRequest.current?.controller === controller) {sourceRequest.current = null; ocelRequest.current = null; setMining(false);}});
  };

  const loadSample = () => {
    loadSampleFile("sample-data.json", "application/json");
  };

  const loadOcelSample = () => {
    loadSampleFile("sample-ocel.json", "application/json");
  };

  const loadResourceSample = () => {
    loadSampleFile("sample-resource-log.csv", "text/csv");
  };
  const loadSampleFile = (name: string, type: string) => {const {ticket, controller} = beginSourceLoad(); fetch(`${import.meta.env.BASE_URL}${name}`, {signal: controller.signal}).then(response => {if (!response.ok) throw new Error("sample not found"); return response.arrayBuffer();}).then(bytes => {if (sourceGate.current.accepts(ticket, controller.signal)) loadFile(new File([bytes], name, {type}));}).catch(cause => {if (sourceGate.current.accepts(ticket, controller.signal)) setError(`Could not load ${name}: ${cause instanceof Error ? cause.message : String(cause)}`);}).finally(() => {if (sourceRequest.current?.controller === controller) {sourceRequest.current = null; setMining(false);}});};

  // Re-mines the original upload with the given filters and miner settings,
  // keeping the active tab. Always mines from source.file so nothing compounds.
  const applyFilters = (filters: FilterOptions & MinerOptions) => {
    if (!source) return;
    const original = source, {ticket, controller} = beginSourceLoad(), options: MineOptions = {...original.options, ...filters, miningRequestId: ticket.request_id};
    sourceRequest.current!.posted = true;
    mineActiveDataset(original.file, options, controller.signal)
      .then((result) => {
        if (!sourceGate.current.accepts(ticket, controller.signal) || sourceRequest.current?.controller !== controller) return;
        const descriptor = miningSourceDescriptor(original.file, original.sha256, options, result), analysis = activeDatasetAnalysis(result);
        liveSource.current = descriptor.source_id; setLiveSourceId(descriptor.source_id);
        setSource({...original, ...descriptor}); setSourceOutcome(isActiveSourceMining(result) ? result : null);
        setFiltersActive(hasActiveFilters(filters) || hasActiveMinerOptions(filters));
        clearSourceCaches(); setData(analysis); if (!analysis) setShowActiveSource(true);
        // The re-mine replaced the server session, so cached on-demand
        // results belong to the previous analysis.
        alignCache.current = {};
        decideCache.current = {};
        dfgCache.current = {};
        editCache.current = {};
        processTreeCache.current = {};
        decomposedCache.current = {};
        petriAntiCache.current = {};
        petriMultiCache.current = {};
        reductionCache.current = {};
        evaluationCache.current = {};
        petriAnalysisCache.current = {};
        emdCache.current = {};
        conceptDriftCache.current = {};
        setSelectedVariant(null);
        setThreshold(1);
        setError(null);
      })
      .catch((cause: Error) => {if (sourceGate.current.accepts(ticket, controller.signal)) setError(`Could not filter ${original.file.name}: ${cause.message}`);})
      .finally(() => {if (sourceRequest.current?.controller === controller) {sourceRequest.current = null; setMining(false);}});
  };

  useEffect(() => {
    let depth = 0;
    const enter = (event: DragEvent) => {
      event.preventDefault();
      if (++depth === 1) setDragging(true);
    };
    const over = (event: DragEvent) => event.preventDefault();
    const leave = (event: DragEvent) => {
      event.preventDefault();
      if (--depth === 0) setDragging(false);
    };
    const drop = (event: DragEvent) => {
      event.preventDefault();
      depth = 0;
      setDragging(false);
      const file = event.dataTransfer?.files[0];
      if (file) loadFile(file);
    };
    window.addEventListener("dragenter", enter);
    window.addEventListener("dragover", over);
    window.addEventListener("dragleave", leave);
    window.addEventListener("drop", drop);
    return () => {
      window.removeEventListener("dragenter", enter);
      window.removeEventListener("dragover", over);
      window.removeEventListener("dragleave", leave);
      window.removeEventListener("drop", drop);
    };
  }, []);

  // fontsReady re-runs the layout once the display fonts are available, so
  // node boxes are sized against the real text metrics.
  const layout = useMemo(
    () => (data ? computeLayout(data) : null),
    [data, fontsReady],
  );

  const treeLayout = useMemo(
    () =>
      data && data.process_tree
        ? computeTreeLayout(data.process_tree, data)
        : null,
    [data, fontsReady],
  );

  const petriLayout = useMemo(
    () =>
      data && data.petri_net ? computePetriLayout(data.petri_net, data) : null,
    [data, fontsReady],
  );

  const inductivePetriLayout = useMemo(
    () =>
      data && data.inductive_petri_net
        ? computePetriLayout(data.inductive_petri_net, data)
        : null,
    [data, fontsReady],
  );

  const heuristicPetriLayout = useMemo(
    () =>
      data && data.heuristic_petri_net
        ? computePetriLayout(data.heuristic_petri_net, data)
        : null,
    [data, fontsReady],
  );

  const powlPetriLayout = useMemo(
    () =>
      data && data.powl_petri_net
        ? computePetriLayout(data.powl_petri_net, data)
        : null,
    [data, fontsReady],
  );

  // Whether the log has any timestamped events to plot. Built once per load so
  // the Dotted Chart tab only appears when it would have something to show.
  const hasDottedChart = useMemo(
    () => Boolean(
      data && (
        (data.traces ? buildDottedChart(data.traces).rowCount > 0 : false) ||
        data.performance_spectrum || data.batch_discovery
      ),
    ),
    [data],
  );

  const selection = useMemo(
    () =>
      data && selectedVariant !== null
        ? variantHighlight(data.variants[selectedVariant].activities)
        : null,
    [data, selectedVariant],
  );

  if (showOriginalOcPlayout) return <><TopBar dataset="Original object-centric model" datasetKind="independent object-centric playout" onOpenFile={loadFile} onHome={()=>setShowOriginalOcPlayout(false)}/><main className="layout-log original-oc-playout-workspace"><button className="btn-ghost" onClick={()=>setShowOriginalOcPlayout(false)}>Return to current workspace</button><OriginalObjectCentricPlayoutWorkspace onOpenOcel={loadFile}/></main></>;
  if (showOriginalPetriAnalysis) return <><TopBar dataset="Independent complete Petri net" datasetKind="original Petri analysis" onOpenFile={loadFile} onHome={()=>setShowOriginalPetriAnalysis(false)}/><main className="layout-log original-petri-analysis-workspace"><button className="btn-ghost" onClick={()=>setShowOriginalPetriAnalysis(false)}>Return to current workspace</button><OriginalPetriAnalysisWorkspace standalone/></main></>;
  if (showOriginalAnonymization) return <><TopBar dataset="Independent original source and released query" datasetKind="original anonymization" onOpenFile={loadFile} onHome={()=>setShowOriginalAnonymization(false)}/><main className="layout-log original-anonymization-workspace"><button className="btn-ghost" onClick={()=>setShowOriginalAnonymization(false)}>Return to current workspace</button><OriginalAnonymizationWorkspace standalone/></main></>;
  if (showOriginalClustering) return <><TopBar dataset="Independent original EventLog or stored result" datasetKind="original clustering" onOpenFile={loadFile} onHome={()=>setShowOriginalClustering(false)}/><main className="layout-log original-clustering-workspace"><button className="btn-ghost" onClick={()=>setShowOriginalClustering(false)}>Return to current workspace</button><OriginalClusteringWorkspace standalone/></main></>;
  if (showOriginalQuality) return <><TopBar dataset="Independent original source and model" datasetKind="original quality" onOpenFile={loadFile} onHome={()=>setShowOriginalQuality(false)}/><main className="layout-log original-quality-workspace"><button className="btn-ghost" onClick={()=>setShowOriginalQuality(false)}>Return to current workspace</button><OriginalQualityWorkspace standalone/></main></>;
  if (showOriginalRootCause) return <><TopBar dataset="Independent original cohorts and shared contexts" datasetKind="original root cause" onOpenFile={loadFile} onHome={()=>setShowOriginalRootCause(false)}/><main className="layout-log original-root-cause-workspace"><button className="btn-ghost" onClick={()=>setShowOriginalRootCause(false)}>Return to current workspace</button><OriginalRootCauseWorkspace standalone/></main></>;
  if (showOriginalDiscovery) return <><TopBar dataset="Independent original discovery inputs" datasetKind="original ILP and genetic discovery" onOpenFile={loadFile} onHome={()=>setShowOriginalDiscovery(false)}/><main className="layout-log original-discovery-workspace"><button className="btn-ghost" onClick={()=>setShowOriginalDiscovery(false)}>Return to current workspace</button><OriginalDiscoveryWorkspace standalone/></main></>;
  if (showOriginalBose) return <><TopBar dataset="Independent original BOSE inputs" datasetKind="original concept drift and public helpers" onOpenFile={loadFile} onHome={()=>setShowOriginalBose(false)}/><main className="layout-log original-bose-workspace"><button className="btn-ghost" onClick={()=>setShowOriginalBose(false)}>Return to current workspace</button><OriginalBoseWorkspace standalone/></main></>;
  if (showOriginalEditDistance) return <><TopBar dataset="Independent original event inputs" datasetKind="original edit-distance alignment and projection" onOpenFile={loadFile} onHome={()=>setShowOriginalEditDistance(false)}/><main className="layout-log original-edit-distance-workspace"><button className="btn-ghost" onClick={()=>setShowOriginalEditDistance(false)}>Return to current workspace</button><OriginalEditDistanceWorkspace standalone/></main></>;
  if (showOriginalEmd) return <><TopBar dataset="Original weighted stochastic languages" datasetKind="independent original-language EMD" onOpenFile={loadFile} onHome={()=>setShowOriginalEmd(false)}/><main className="layout-log original-emd-workspace"><button className="btn-ghost" onClick={()=>setShowOriginalEmd(false)}>Return to current workspace</button><OriginalEmdWorkspace/></main></>;
  if (showOriginalEmbeddingFilter) return <><TopBar dataset={data?.dataset??ocelDataset??'Original embedding inputs'} datasetKind="independent original-log embedding and filtering" onOpenFile={loadFile} onHome={()=>setShowOriginalEmbeddingFilter(false)}/><main className="layout-log original-embedding-filter-workspace"><button className="btn-ghost" onClick={()=>setShowOriginalEmbeddingFilter(false)}>Return to current workspace</button><OriginalEmbeddingFilterWorkspace file={source?.file??null} source={source} onOpenLog={loadFile}/></main></>;
  if (showOriginalGeneticMatrix) return <><TopBar dataset={data?.dataset??ocelDataset??'Original genetic causal matrix'} datasetKind="independent supplied-matrix Petri conversion" onOpenFile={loadFile} onHome={()=>setShowOriginalGeneticMatrix(false)}/><main className="layout-log original-genetic-matrix-workspace"><button className="btn-ghost" onClick={()=>setShowOriginalGeneticMatrix(false)}>Return to current workspace</button><OriginalGeneticMatrixWorkspace/></main></>;
  if (showOriginalFootprints) return <><TopBar dataset={data?.dataset??ocelDataset??'Original Footprints'} datasetKind="independent original-footprint conformance, metrics and projection" onOpenFile={loadFile} onHome={()=>setShowOriginalFootprints(false)}/><main className="layout-log original-footprints-workspace"><button className="btn-ghost" onClick={()=>setShowOriginalFootprints(false)}>Return to current workspace</button><OriginalFootprintsWorkspace file={source?.file??null} source={source} onOpenLog={loadFile}/></main></>;
  if (showOriginalLogSkeleton) return <><TopBar dataset={data?.dataset??ocelDataset??'Original LogSkeleton model'} datasetKind="independent original-model LogSkeleton conformance and diagnostics" onOpenFile={loadFile} onHome={()=>setShowOriginalLogSkeleton(false)}/><main className="layout-log original-log-skeleton-workspace"><button className="btn-ghost" onClick={()=>setShowOriginalLogSkeleton(false)}>Return to current workspace</button><OriginalLogSkeletonWorkspace file={source?.file??null} source={source} onOpenLog={loadFile}/></main></>;
  if (showOriginalDeclare) return <><TopBar dataset={data?.dataset??ocelDataset??'Original Declare model'} datasetKind="independent original-model Declare conformance and playout" onOpenFile={loadFile} onHome={()=>setShowOriginalDeclare(false)}/><main className="layout-log original-declare-workspace"><button className="btn-ghost" onClick={()=>setShowOriginalDeclare(false)}>Return to current workspace</button><OriginalDeclareWorkspace file={source?.file??null} source={source} onOpenLog={loadFile}/></main></>;
  if (showOriginalDecision) return <><TopBar dataset={data?.dataset??ocelDataset??'Original-model decisions'} datasetKind="independent original-PNML decision mining" onOpenFile={loadFile} onHome={()=>setShowOriginalDecision(false)}/><main className="layout-log original-decision-workspace"><button className="btn-ghost" onClick={()=>setShowOriginalDecision(false)}>Return to current workspace</button><OriginalDecisionWorkspace file={source?.file??null} source={source}/></main></>;
  if (showPlayout) return <><TopBar dataset={data?.dataset??ocelDataset??'Original-model playout'} datasetKind="independent model playout and stochastic fitting" onOpenFile={loadFile} onHome={()=>setShowPlayout(false)}/><main className="layout-log playout-workspace"><button className="btn-ghost" onClick={()=>setShowPlayout(false)}>Return to current workspace</button><PlayoutWorkspace file={source?.file??null} source={source} onOpenLog={loadFile}/></main></>;
  if (showMonteCarlo) return <><TopBar dataset={data?.dataset??ocelDataset??'Original-model Monte Carlo'} datasetKind="independent Monte Carlo simulation and fitting" onOpenFile={loadFile} onHome={()=>setShowMonteCarlo(false)}/><main className="layout-log monte-carlo-workspace"><button className="btn-ghost" onClick={()=>setShowMonteCarlo(false)}>Return to current workspace</button><MonteCarloWorkspace file={source?.file??null} source={source} onOpenLog={loadFile}/></main></>;
  if (showModelConversion) return <><TopBar dataset={data?.dataset??ocelDataset??'Original model conversion'} datasetKind="independent model conversion and reduction" onOpenFile={loadFile} onHome={()=>setShowModelConversion(false)}/><main className="layout-log model-conversion-workspace"><button className="btn-ghost" onClick={()=>setShowModelConversion(false)}>Return to current workspace</button><ModelConversionWorkspace/></main></>;
  if (showPetriAntiMulti) return <><TopBar dataset={data?.dataset??ocelDataset??'Anti/multi alignment inputs'} datasetKind="independent Petri anti/multi alignment" onOpenFile={loadFile} onHome={()=>setShowPetriAntiMulti(false)}/><main className="layout-log petri-anti-multi-workspace"><button className="btn-ghost" onClick={()=>setShowPetriAntiMulti(false)}>Return to current workspace</button><PetriAntiMultiWorkspace file={source?.file??null} source={source}/></main></>;
  if (showTreeAlignment) return <><TopBar dataset={data?.dataset??ocelDataset??'Tree alignment inputs'} datasetKind="independent process-tree alignment" onOpenFile={loadFile} onHome={()=>setShowTreeAlignment(false)}/><main className="layout-log process-tree-alignment-workspace"><button className="btn-ghost" onClick={()=>setShowTreeAlignment(false)}>Return to current workspace</button><ProcessTreeAlignmentWorkspace file={source?.file??null} source={source}/></main></>;
  if (showInductiveInputs) return <><TopBar dataset={data?.dataset??ocelDataset??'Inductive inputs'} datasetKind="independent Inductive discovery" onOpenFile={loadFile} onHome={()=>setShowInductiveInputs(false)}/><main className="layout-log inductive-input-workspace"><button className="btn-ghost" onClick={()=>setShowInductiveInputs(false)}>Return to current workspace</button><InductiveInputWorkspace file={source?.file??null} source={source}/></main></>;
  if (showGraphAlignment) return <><TopBar dataset={data?.dataset??ocelDataset??'Graph alignment inputs'} datasetKind="independent graph alignment" onOpenFile={loadFile} onHome={()=>setShowGraphAlignment(false)}/><main className="layout-log graph-alignment-workspace"><button className="btn-ghost" onClick={()=>setShowGraphAlignment(false)}>Return to current workspace</button><GraphAlignmentWorkspace file={source?.file??null} source={source}/></main></>;
  if (showWorkflowInputs) return <><TopBar dataset={data?.dataset??ocelDataset??'Workflow inputs'} datasetKind="independent workflow discovery" onOpenFile={loadFile} onHome={()=>setShowWorkflowInputs(false)}/><main className="layout-log workflow-input-workspace"><button className="btn-ghost" onClick={()=>setShowWorkflowInputs(false)}>Return to current workspace</button><WorkflowInputWorkspace file={source?.file??null} source={source}/></main></>;
  if (showPetriAlignment) return <><TopBar dataset={data?.dataset??ocelDataset??'Alignment inputs'} datasetKind="Petri alignment" onOpenFile={loadFile} onHome={()=>setShowPetriAlignment(false)}/><main className="layout-log"><button className="btn-ghost" onClick={()=>setShowPetriAlignment(false)}>Return to current workspace</button><PetriAlignmentWorkspace file={source?.file??null} source={source}/></main></>;
  if (showTokenReplay) return <><TopBar dataset={data?.dataset??ocelDataset??'Replay inputs'} datasetKind="token replay" onOpenFile={loadFile} onHome={()=>setShowTokenReplay(false)}/><main className="layout-log"><button className="btn-ghost" onClick={()=>setShowTokenReplay(false)}>Return to current workspace</button><TokenReplayWorkspace file={source?.file??null} source={source}/></main></>;
  if (showOcelIo) return <><TopBar dataset={ocelDataset??data?.dataset??'OCEL file exchange'} datasetKind="file exchange" onOpenFile={loadFile} onHome={()=>setShowOcelIo(false)}/><main className="layout-log"><button className="btn-ghost" onClick={()=>setShowOcelIo(false)}>Return to current workspace</button><OcelIoWorkspace file={ocelFile} busy={mining} onOpen={loadFile}/></main></>;
  if (showAcquisition) return <><TopBar dataset={data?.dataset??ocelDataset??'Activity sources'} datasetKind="source acquisition" onOpenFile={loadFile} onHome={()=>setShowAcquisition(false)}/><main className="layout-log"><button className="btn-ghost" onClick={()=>setShowAcquisition(false)}>Return to current workspace</button><SourceAcquisitionWorkspace busy={mining} onOpen={loadFile}/></main></>;

  if (showModelExchange) return <><TopBar dataset={data?.dataset??ocelDataset??'Model files'} datasetKind="model exchange" onOpenFile={loadFile} onHome={()=>setShowModelExchange(false)}/><main className="layout-log"><button className="btn-ghost" onClick={()=>setShowModelExchange(false)}>Return to current workspace</button><ModelExchangeWorkspace available={sourceAvailable} busy={mining} onUseModel={(file,sourceId,activityKey)=>{setDiscoveryModelDraft({generation:cacheGeneration.current,file,sourceId,activityKey,inferLegacyBoundaries:false});setShowModelExchange(false);setTab("conformance");}}/></main></>;
  if (showProcessCube) return <><TopBar dataset={data?.dataset??ocelDataset??'Process cubes'} datasetKind="process cube" onOpenFile={loadFile} onHome={()=>setShowProcessCube(false)}/><main className="layout-log"><button className="btn-ghost" onClick={()=>setShowProcessCube(false)}>Return to current workspace</button><ProcessCubeWorkspace busy={mining}/></main></>;
  if (showTreeGeneration) return <><TopBar dataset="Generated models" datasetKind="tree generation" onOpenFile={loadFile} onHome={()=>setShowTreeGeneration(false)}/><main className="layout-log"><button className="btn-ghost" onClick={()=>setShowTreeGeneration(false)}>Return to current workspace</button><TreeGenerationWorkspace onOpenLog={loadFile} sourceAvailable={sourceAvailable} onUseModel={async file=>{const generation=cacheGeneration.current;const schema=await requestLogQuerySchema();if(generation!==cacheGeneration.current)throw new Error("The active log changed; check this model again.");setDiscoveryModelDraft({generation,file,sourceId:schema.source_id,inferLegacyBoundaries:false});setShowTreeGeneration(false);setTab("conformance");}}/></main></>;
  if (showStreaming) return <><TopBar dataset={data?.dataset??ocelDataset??'Event streams'} datasetKind="streaming replay" onOpenFile={loadFile} onHome={()=>setShowStreaming(false)}/><main className="layout-log"><button className="btn-ghost" onClick={()=>setShowStreaming(false)}>Return to current workspace</button><StreamingWorkspace file={source?.file??null} sourceFormat={source?.format} busy={mining} data={data??undefined} onOpen={loadFile}/></main></>;
  if (showRelatedLogs) return <><TopBar dataset={data?.dataset??ocelDataset??'Related logs'} datasetKind="log relationships" onOpenFile={loadFile} onHome={()=>setShowRelatedLogs(false)}/><main className="layout-log"><button className="btn-ghost" onClick={()=>setShowRelatedLogs(false)}>Return to current workspace</button><RelatedLogsWorkspace file={source?.file??null} sourceFormat={source?.format} available={sourceAvailable} mining={mining} dataset={data?.dataset??source?.file.name} onOpen={loadFile}/></main></>;

  if (showActiveSource || (!data && source)) return <><TopBar dataset={source?.file.name??pendingSourceFile?.name??"Original native source"} datasetKind="native event source" onOpenFile={loadFile} onHome={()=>{if(data||ocelResult)setShowActiveSource(false);else{cancelSourceLoad();liveSource.current=null;setLiveSourceId(null);setSource(null);setSourceOutcome(null);setPendingSourceFile(null);setShowActiveSource(false);setError(null);}}} onWorkflowInputs={()=>setShowWorkflowInputs(true)} onGraphAlignment={()=>setShowGraphAlignment(true)} onInductiveInputs={()=>setShowInductiveInputs(true)} onTreeAlignment={()=>setShowTreeAlignment(true)} onPetriAntiMulti={()=>setShowPetriAntiMulti(true)} onTokenReplay={()=>setShowTokenReplay(true)} onPetriAlignment={()=>setShowPetriAlignment(true)} onModelConversion={()=>setShowModelConversion(true)} onMonteCarlo={()=>setShowMonteCarlo(true)} onPlayout={()=>setShowPlayout(true)} onOriginalDecision={()=>setShowOriginalDecision(true)} onOriginalDeclare={()=>setShowOriginalDeclare(true)} onOriginalLogSkeleton={()=>setShowOriginalLogSkeleton(true)} onOriginalFootprints={()=>setShowOriginalFootprints(true)} onOriginalGeneticMatrix={()=>setShowOriginalGeneticMatrix(true)} onOriginalEmbeddingFilter={()=>setShowOriginalEmbeddingFilter(true)} onOriginalEmd={()=>setShowOriginalEmd(true)} onOriginalEditDistance={()=>setShowOriginalEditDistance(true)} onOriginalBose={()=>setShowOriginalBose(true)} onOriginalDiscovery={()=>setShowOriginalDiscovery(true)} onOriginalPetriAnalysis={()=>setShowOriginalPetriAnalysis(true)} onOriginalRootCause={()=>setShowOriginalRootCause(true)} onOriginalAnonymization={()=>setShowOriginalAnonymization(true)} onOriginalQuality={()=>setShowOriginalQuality(true)} onOriginalClustering={()=>setShowOriginalClustering(true)} onOriginalOcPlayout={()=>setShowOriginalOcPlayout(true)}/><main className="layout-log active-mining-source-workspace">{(data||ocelResult)&&<button className="btn-ghost" onClick={()=>setShowActiveSource(false)}>Return to current workspace</button>}{error&&<div className="error-banner" role="alert">{error}</div>}<ActiveMiningSource source={source} initialFile={pendingSourceFile} data={data} outcome={sourceOutcome} available={sourceAvailable} mining={mining} onOpen={(file,options)=>{loadFile(file,undefined,undefined,options);setShowActiveSource(true);}} onCancel={cancelSourceLoad}/>{source&&<QueryWorkspace key={cacheGeneration.current} sourceId={source.source_id} dataset={source.file.name} sourceOptions={{miningInputShape:source.shape,miningSelectors:source.selectors}} available={sourceAvailable} mining={mining} onOpenFiltered={(file,options)=>loadFile(file,undefined,undefined,options)}/>}</main></>;

  if (!data && ocelResult) {
    return (
      <>
        <TopBar
          dataset={ocelDataset ?? "object-centric log"}
          datasetKind="object-centric"
          onOpenFile={loadFile}
          onImportOcel={()=>setShowOcelIo(true)}
          onRelateLogs={()=>setShowRelatedLogs(true)}
          onStreaming={()=>setShowStreaming(true)}
          onModelExchange={()=>setShowModelExchange(true)}
          onProcessCube={()=>setShowProcessCube(true)}
          onTreeGeneration={()=>setShowTreeGeneration(true)}
          onAcquisition={()=>setShowAcquisition(true)}
          onTokenReplay={()=>setShowTokenReplay(true)}
          onPetriAlignment={()=>setShowPetriAlignment(true)}
          onWorkflowInputs={()=>setShowWorkflowInputs(true)}
          onGraphAlignment={()=>setShowGraphAlignment(true)}
          onInductiveInputs={()=>setShowInductiveInputs(true)}
          onTreeAlignment={()=>setShowTreeAlignment(true)}
          onPetriAntiMulti={()=>setShowPetriAntiMulti(true)}
          onModelConversion={()=>setShowModelConversion(true)}
          onMonteCarlo={()=>setShowMonteCarlo(true)} onPlayout={()=>setShowPlayout(true)} onOriginalDecision={()=>setShowOriginalDecision(true)} onOriginalDeclare={()=>setShowOriginalDeclare(true)} onOriginalLogSkeleton={()=>setShowOriginalLogSkeleton(true)} onOriginalFootprints={()=>setShowOriginalFootprints(true)} onOriginalGeneticMatrix={()=>setShowOriginalGeneticMatrix(true)} onOriginalEmbeddingFilter={()=>setShowOriginalEmbeddingFilter(true)} onOriginalEmd={()=>setShowOriginalEmd(true)} onOriginalEditDistance={()=>setShowOriginalEditDistance(true)} onOriginalBose={()=>setShowOriginalBose(true)} onOriginalDiscovery={()=>setShowOriginalDiscovery(true)} onOriginalPetriAnalysis={()=>setShowOriginalPetriAnalysis(true)} onOriginalRootCause={()=>setShowOriginalRootCause(true)} onOriginalAnonymization={()=>setShowOriginalAnonymization(true)} onOriginalQuality={()=>setShowOriginalQuality(true)} onOriginalClustering={()=>setShowOriginalClustering(true)} onOriginalOcPlayout={()=>setShowOriginalOcPlayout(true)}
          onActiveSource={()=>setShowActiveSource(true)}
          onHome={() => {
            cancelSourceLoad();
            liveSource.current = null; setLiveSourceId(null); setSourceOutcome(null); setPendingSourceFile(null);
            cacheGeneration.current += 1;
            setData(null);
            setOcelResult(null);
            setOcelFile(null);
            setOcelDataset(null);
            setError(null);
            setSource(null);
          }}
        />
        {error && <div className="error-banner">{error}</div>}
        <VisualizationExplain key={cacheGeneration.current+":"+ocelResult.variant} dataset={ocelDataset??"object-centric log"} busy={mining} onUse={draft=>{setVisualizationDraft({generation:cacheGeneration.current,draft});if(ocelQuestions.current)ocelQuestions.current.open=true;}} />
        <main className="layout-log">
          <OcelWorkspaceHub
            result={ocelResult}
            file={ocelFile}
            busy={mining}
            onDiscover={changeOcelModel}
            onOpen={loadFile}
          />
          <details ref={ocelQuestions} className="ocel-model-question"><summary>Ask a model about this object-centric log</summary><PromptChat key={cacheGeneration.current+":"+(visualizationDraft?.generation===cacheGeneration.current?visualizationDraft.draft.id:"")} mining={mining} available={ocelFile!==null} ocelFile={ocelFile} dataset={ocelDataset??undefined} visualization={visualizationDraft?.generation===cacheGeneration.current?visualizationDraft.draft:null} /></details>
        </main>
        <footer className="footer reveal">
          <span>{ocelDataset ?? "object-centric log"} · object-centric model</span>
        </footer>
        <div className={dragging ? "dropveil active" : "dropveil"}>
          <div className="dropveil-card">Drop an event source, Minera export or OCEL file</div>
        </div>
      </>
    );
  }

  if (!data || !layout) {
    return (
      <>
        <Landing
          onOpenFile={loadFile}
          onLoadSample={loadSample}
          onLoadOcelSample={loadOcelSample}
          onLoadResourceSample={loadResourceSample}
          onRelateLogs={()=>setShowRelatedLogs(true)}
          onStreaming={()=>setShowStreaming(true)}
          onModelExchange={()=>setShowModelExchange(true)}
          onProcessCube={()=>setShowProcessCube(true)}
          onTreeGeneration={()=>setShowTreeGeneration(true)}
          onAcquisition={()=>setShowAcquisition(true)}
          onTokenReplay={()=>setShowTokenReplay(true)}
          onPetriAlignment={()=>setShowPetriAlignment(true)}
          onWorkflowInputs={()=>setShowWorkflowInputs(true)}
          onGraphAlignment={()=>setShowGraphAlignment(true)}
          onInductiveInputs={()=>setShowInductiveInputs(true)}
          onTreeAlignment={()=>setShowTreeAlignment(true)}
          onPetriAntiMulti={()=>setShowPetriAntiMulti(true)}
          onModelConversion={()=>setShowModelConversion(true)}
          onMonteCarlo={()=>setShowMonteCarlo(true)} onPlayout={()=>setShowPlayout(true)} onOriginalDecision={()=>setShowOriginalDecision(true)} onOriginalDeclare={()=>setShowOriginalDeclare(true)} onOriginalLogSkeleton={()=>setShowOriginalLogSkeleton(true)} onOriginalFootprints={()=>setShowOriginalFootprints(true)} onOriginalGeneticMatrix={()=>setShowOriginalGeneticMatrix(true)} onOriginalEmbeddingFilter={()=>setShowOriginalEmbeddingFilter(true)} onOriginalEmd={()=>setShowOriginalEmd(true)} onOriginalEditDistance={()=>setShowOriginalEditDistance(true)} onOriginalBose={()=>setShowOriginalBose(true)} onOriginalDiscovery={()=>setShowOriginalDiscovery(true)} onOriginalPetriAnalysis={()=>setShowOriginalPetriAnalysis(true)} onOriginalRootCause={()=>setShowOriginalRootCause(true)} onOriginalAnonymization={()=>setShowOriginalAnonymization(true)} onOriginalQuality={()=>setShowOriginalQuality(true)} onOriginalClustering={()=>setShowOriginalClustering(true)} onOriginalOcPlayout={()=>setShowOriginalOcPlayout(true)}
          onActiveSource={()=>setShowActiveSource(true)}
          onImportOcel={()=>setShowOcelIo(true)}
          error={error}
          mining={mining}
        />
        <div className={dragging ? "dropveil active" : "dropveil"}>
          <div className="dropveil-card">Drop an event source, Minera export or OCEL file</div>
        </div>
      </>
    );
  }

  const maxEdge = maxEdgeCount(data);

  return (
    <>
      <TopBar
        dataset={data.dataset}
        generator={data.generator}
        onOpenFile={loadFile}
        onImportOcel={()=>setShowOcelIo(true)}
        onRelateLogs={()=>setShowRelatedLogs(true)}
        onStreaming={()=>setShowStreaming(true)}
        onModelExchange={()=>setShowModelExchange(true)}
          onProcessCube={()=>setShowProcessCube(true)}
          onTreeGeneration={()=>setShowTreeGeneration(true)}
          onAcquisition={()=>setShowAcquisition(true)}
          onTokenReplay={()=>setShowTokenReplay(true)}
          onPetriAlignment={()=>setShowPetriAlignment(true)}
          onWorkflowInputs={()=>setShowWorkflowInputs(true)}
          onGraphAlignment={()=>setShowGraphAlignment(true)}
          onInductiveInputs={()=>setShowInductiveInputs(true)}
          onTreeAlignment={()=>setShowTreeAlignment(true)}
          onPetriAntiMulti={()=>setShowPetriAntiMulti(true)}
          onModelConversion={()=>setShowModelConversion(true)}
          onMonteCarlo={()=>setShowMonteCarlo(true)} onPlayout={()=>setShowPlayout(true)} onOriginalDecision={()=>setShowOriginalDecision(true)} onOriginalDeclare={()=>setShowOriginalDeclare(true)} onOriginalLogSkeleton={()=>setShowOriginalLogSkeleton(true)} onOriginalFootprints={()=>setShowOriginalFootprints(true)} onOriginalGeneticMatrix={()=>setShowOriginalGeneticMatrix(true)} onOriginalEmbeddingFilter={()=>setShowOriginalEmbeddingFilter(true)} onOriginalEmd={()=>setShowOriginalEmd(true)} onOriginalEditDistance={()=>setShowOriginalEditDistance(true)} onOriginalBose={()=>setShowOriginalBose(true)} onOriginalDiscovery={()=>setShowOriginalDiscovery(true)} onOriginalPetriAnalysis={()=>setShowOriginalPetriAnalysis(true)} onOriginalRootCause={()=>setShowOriginalRootCause(true)} onOriginalAnonymization={()=>setShowOriginalAnonymization(true)} onOriginalQuality={()=>setShowOriginalQuality(true)} onOriginalClustering={()=>setShowOriginalClustering(true)} onOriginalOcPlayout={()=>setShowOriginalOcPlayout(true)}
          onActiveSource={()=>setShowActiveSource(true)}
        onHome={() => {
          cancelSourceLoad(); liveSource.current = null; setLiveSourceId(null); setSourceOutcome(null); setPendingSourceFile(null);
          cacheGeneration.current += 1;
          setData(null);
          setOcelResult(null);
          setOcelFile(null);
          setOcelDataset(null);
          setError(null);
          setSource(null);
          setFiltersActive(false);
          conceptDriftCache.current = {};
        }}
      />
      {error && <div className="error-banner">{error}</div>}
      <nav className="tabs reveal">
        {data.traces && data.traces.length > 0 && (
          <button
            className={tab === "log" ? "tab active" : "tab"}
            onClick={() => setTab("log")}
          >
            Event Log
          </button>
        )}
        {hasDottedChart && (
          <button
            className={tab === "dotted" ? "tab active" : "tab"}
            onClick={() => setTab("dotted")}
          >
            Dotted Chart
          </button>
        )}
        <button
          className={tab === "dfg" ? "tab active" : "tab"}
          onClick={() => setTab("dfg")}
        >
          Directly-Follows Graph
        </button>
        <button className={tab === "timing" ? "tab active" : "tab"} onClick={()=>setTab("timing")}>Timing discovery</button>
        <button className={tab === "behavior" ? "tab active" : "tab"} onClick={()=>setTab("behavior")}>Behavior analysis</button>
        <button
          className={tab === "model" ? "tab active" : "tab"}
          onClick={() => setTab("model")}
        >
          Mine a Process Model
        </button>
        {data.conformance && (
          <button
            className={tab === "conformance" ? "tab active" : "tab"}
            onClick={() => setTab("conformance")}
          >
            Conformance
          </button>
        )}
        {(source || (data.resources && data.resources.resources.length > 0)) && (
          <button
            className={tab === "resources" ? "tab active" : "tab"}
            onClick={() => setTab("resources")}
          >
            Resources
          </button>
        )}
        <button className={tab === "embeddings" ? "tab active" : "tab"} onClick={() => setTab("embeddings")}>
          Search &amp; embeddings
        </button>
        <button className={tab === "llm" ? "tab active" : "tab"} onClick={() => setTab("llm")}>Ask a model</button>
        <button className={tab === "query" ? "tab active" : "tab"} onClick={() => setTab("query")}>Query &amp; filter</button>
        <button className={tab === "anonymization" ? "tab active" : "tab"} onClick={() => setTab("anonymization")}>Anonymize</button>
        <button className={tab === "clustering" ? "tab active" : "tab"} onClick={() => setTab("clustering")}>Clustering</button>
        <button className={tab === "transformation" ? "tab active" : "tab"} onClick={() => setTab("transformation")}>Transform</button>
      </nav>
      <Kpis data={data} />
      {tab!=="llm"&&tab!=="query"&&tab!=="embeddings"&&tab!=="anonymization"&&<VisualizationExplain key={cacheGeneration.current+":"+tab} dataset={data.dataset} busy={mining} onUse={draft=>{setVisualizationDraft({generation:cacheGeneration.current,draft});setTab("llm");}} />}
      {source && (
        <Filters
          key={source.file.name}
          activities={source.activities}
          resources={source.resources}
          lifecycles={source.lifecycles}
          attributes={source.attributes}
          caseAttributes={source.caseAttributes}
          active={filtersActive}
          mining={mining}
          onApply={applyFilters}
        />
      )}
      {tab === "log" && (
        <main className="layout-log">
          <EventLogView data={data} />
        </main>
      )}
      {tab === "model" && (
        <main className="layout-log">
          <ProcessModel
            onOpenPlayout={loadFile}
            key={cacheGeneration.current}
            data={data}
            treeLayout={treeLayout}
            petriLayout={petriLayout}
            inductivePetriLayout={inductivePetriLayout}
            heuristicPetriLayout={heuristicPetriLayout}
            powlPetriLayout={powlPetriLayout}
            canExportPnml={sourceAvailable}
            mining={mining}
            onUseModel={(file,sourceId,activityKey)=>{setDiscoveryModelDraft({generation:cacheGeneration.current,file,sourceId,activityKey});setTab("conformance");}}
          />
        </main>
      )}
      {tab === "dotted" && (
        <main className="layout-log">
          <DottedChartView data={data} />
          <PerformanceSpectrumView data={data} />
          <Batches data={data} />
        </main>
      )}
      {tab === "timing" && <TimingDiscovery key={cacheGeneration.current} data={data} available={sourceAvailable} mining={mining}/>}
      {tab === "behavior" && <BehaviorAnalysis key={cacheGeneration.current} data={data} available={sourceAvailable} mining={mining}/>}
      {tab === "conformance" && (
        <main className="layout-log">
          <Conformance
            onOpenSimulation={loadFile}
            data={data}
            onRequestAlignment={requestAlignment}
            onRequestDecisions={source ? requestDecisions : undefined}
            onCheckModel={requestModelCheck}
            initialModel={discoveryModelDraft?.generation===cacheGeneration.current?discoveryModelDraft:null}
            onInitialModelConsumed={file=>setDiscoveryModelDraft(current=>current?.file===file?null:current)}
            onRequestDfg={requestDfg}
            onRequestEditDistance={requestEditDistance}
            onRequestProcessTree={requestProcessTree}
            onRequestDecomposed={requestDecomposed}
            onRequestPetriAnti={requestPetriAnti}
            onRequestPetriMulti={requestPetriMulti}
            onRequestReduction={requestReduction}
            onRequestEvaluation={requestEvaluation}
            onRequestPetriAnalysis={requestPetriAnalysis}
            onRequestEmd={requestEmd}
            onRequestConceptDrift={requestConceptDriftCached}
          />
        </main>
      )}
      {tab === "resources" && (
        <main className="layout-log">
          <Resources key={cacheGeneration.current} data={data} available={sourceAvailable} mining={mining} />
        </main>
      )}
      {tab === "embeddings" && (
        <main className="layout-log">
          <EmbeddingSearch key={cacheGeneration.current} data={data} available={sourceAvailable} mining={mining} />
        </main>
      )}
      {tab === "llm" && <main className="layout-log"><AskModel key={cacheGeneration.current+":"+(visualizationDraft?.generation===cacheGeneration.current?visualizationDraft.draft.id:"")} data={data} mining={mining} available={sourceAvailable} onOpenFiltered={loadFile} visualization={visualizationDraft?.generation===cacheGeneration.current?visualizationDraft.draft:null} /></main>}
      {tab === "query" && <main className="layout-log"><QueryWorkspace key={cacheGeneration.current} data={data} sourceId={source?.source_id} sourceOptions={source ? {miningInputShape:source.shape,miningSelectors:source.selectors} : undefined} available={sourceAvailable} mining={mining} onOpenFiltered={(file,options)=>loadFile(file,undefined,undefined,options)} /></main>}
      {tab === "anonymization" && <main className="layout-log"><AnonymizationWorkspace key={cacheGeneration.current} data={data} available={sourceAvailable} mining={mining} onOpenReleased={loadFile} /></main>}
      {tab === "clustering" && <main className="layout-log"><ClusteringWorkspace key={cacheGeneration.current} data={data} available={sourceAvailable} mining={mining} onOpenCluster={loadFile} /></main>}
      {tab === "transformation" && <main className="layout-log"><TransformationHub key={cacheGeneration.current} data={data} available={sourceAvailable} file={source?.file??null} sourceFormat={source?.format} mining={mining} onOpenTransformed={loadFile} onUseModel={(file,sourceId,activityKey,inferLegacyBoundaries)=>{setDiscoveryModelDraft({generation:cacheGeneration.current,file,sourceId,activityKey,inferLegacyBoundaries});setTab("conformance");}} /></main>}
      {tab === "dfg" && (
        <main className="layout">
          <DfgDiscovery key={cacheGeneration.current} data={data} available={sourceAvailable} mining={mining}/>
          <ProcessMap
            layout={layout}
            threshold={threshold}
            maxEdgeCount={maxEdge}
            onThresholdChange={setThreshold}
            selection={selection}
            caseDuration={data.performance?.case_duration}
            caseArrival={data.performance?.case_arrival}
          />
          <VariantList data={data} selected={selectedVariant} onSelect={setSelectedVariant} />
          <ActivityTable data={data} />
          <DfgVariants data={data} />
          <DfgDataframe />
          <CorrelationMining data={data} />
          <EventuallyFollows data={data} />
        </main>
      )}
      <footer className="footer reveal">
        <span>
          {data.generator ?? "minera"} · {data.dataset} ·{" "}
          {numberFormat.format(data.trace_count)} cases
        </span>
      </footer>
      <div className={dragging ? "dropveil active" : "dropveil"}>
        <div className="dropveil-card">Drop an event source, Minera export or OCEL file</div>
      </div>
    </>
  );
}
