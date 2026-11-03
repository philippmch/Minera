import {OriginalLogQueryWorkspace} from "./components/OriginalLogQueryWorkspace";
import {OriginalLabelSplittingWorkspace} from "./components/OriginalLabelSplittingWorkspace";
import {OriginalFeaturesWorkspace} from "./components/OriginalFeaturesWorkspace";
import {OwnedExactFilterResult,exactFilterRun,type ExactFilterRun} from "./exact-filter-controls";
import {sourceImportOptions} from "./source-import-options";
import {ExactFilterInspection} from "./components/OriginalLtlFilter";
import { useCallback, useEffect, useMemo, useRef, useState, type ReactNode } from "react";
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
import { Sidebar, type SidebarDataset } from "./components/Sidebar";
import { CommandPalette, paletteCommands } from "./components/CommandPalette";
import { Advanced, PageHeader } from "./components/ui";
import { toolById, toolGroup, viewById, VIEWS, VIEW_GROUPS, type ToolId } from "./tools";
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
import {OriginalOrganizationalWorkspace} from "./components/OriginalOrganizationalWorkspace";
import {OriginalTemporalWorkspace} from "./components/OriginalTemporalWorkspace";
import {OriginalTimingWorkspace} from "./components/OriginalTimingWorkspace";
import {OriginalTimeAnalysisWorkspace} from "./components/OriginalTimeAnalysisWorkspace";
import {SavedStructuralWorkspace} from "./components/SavedStructuralWorkspace";
import { OriginalEditDistanceWorkspace } from "./components/OriginalEditDistanceWorkspace";
import { OriginalObjectCentricPlayoutWorkspace } from "./components/OriginalObjectCentricPlayoutWorkspace";
import {ActiveMiningSource} from "./components/ActiveMiningSource";
import {acknowledgeMiningRequest, activeDatasetAnalysis, activeMiningSourceText, assertMiningSourceBinding, cancelMiningRequest, classifyMiningJson, isActiveSourceMining, miningSourceDescriptor, miningSourceHash, miningSourceOptions, MiningSourceRequestGate, suggestMiningSourceForm, type ActiveMiningSourceDescriptor, type ActiveSourceMining} from "./active-mining-source";
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
  LiveSourceHandback,
  type LiveSourceRollback,
  delimiterForFile,
  hasActiveFilters,
  hasActiveMinerOptions,
  displayedTab,
  initialTab,
  isJsonExport,
  logLifecycles,
  maxEdgeCount,
  type Tab,
} from "./app-logic";

export default function App() {
  const [exactInspection,setExactInspection] = useState<ExactFilterRun|null>(null);
  const [ownedExactInspection,setOwnedExactInspection] = useState(false);
  const [previousExactSuccess,setPreviousExactSuccess] = useState<ExactFilterRun|null>(null);
  const [data, setData] = useState<MineraExport | null>(null);
  const [ocelResult, setOcelResult] = useState<OcelDiscoveryResult | null>(null);
  const [tool, setTool] = useState<ToolId | null>(null);
  const [paletteOpen, setPaletteOpen] = useState(false);
  const [sidebarOpen, setSidebarOpen] = useState(false);
  const [exportOpen, setExportOpen] = useState(false);
  const [pendingSourceFile, setPendingSourceFile] = useState<File | null>(null);
  const [sourceOutcome, setSourceOutcome] = useState<ActiveSourceMining | null>(null);
  const [liveSourceId, setLiveSourceId] = useState<string | null>(null);
  const liveSource = useRef<string | null>(null), sourceGate = useRef(new MiningSourceRequestGate());
  // previousSource: the live backend source this load replaced, handed back if the load fails or is cancelled.
  const sourceRequest = useRef<{controller: AbortController; request_id: string; posted: boolean; previousSource: string | null} | null>(null);
  const liveSourceHandback = useRef(new LiveSourceHandback());
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
  // Counts completed fresh loads (not filter re-mines), so the filter panel starts over only for a new source.
  const [sourceLoadSerial, setSourceLoadSerial] = useState(0);
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
  // Counts imported-model checks. A check replaces only the backend's "imported" net, so it must not invalidate
  // requests in flight for other models (which cacheGeneration would); it scopes every result cache key instead,
  // so a result for the replaced imported net can never be read back.
  const importedModelGeneration = useRef(0);
  const resultCacheScope = () => `${liveSource.current ?? 'offline'}#${importedModelGeneration.current}`;
  const sourceAvailable = source !== null && liveSourceId === source.source_id;
  const clearSourceCaches = () => {cacheGeneration.current += 1; alignCache.current = {}; decideCache.current = {}; dfgCache.current = {}; editCache.current = {}; processTreeCache.current = {}; decomposedCache.current = {}; petriAntiCache.current = {}; petriMultiCache.current = {}; reductionCache.current = {}; evaluationCache.current = {}; petriAnalysisCache.current = {}; emdCache.current = {}; conceptDriftCache.current = {};};
  // Settles once every cancel-mine sent so far has been handled. A new mining POST waits for it, so the backend
  // applies an older load's rollback before the newer load begins (begin_active_mining drops the older rollback).
  const cancelBarrier = useRef<Promise<void>>(Promise.resolve());
  const sendCancel = (requestId: string) => {const rollback = cancelMiningRequest(requestId); cancelBarrier.current = Promise.all([cancelBarrier.current, rollback]).then(() => undefined); return rollback;};
  const abortSourceLoad = () => {sourceGate.current.invalidate(); const previous = sourceRequest.current; sourceRequest.current = null; previous?.controller.abort(); const rollback: Promise<LiveSourceRollback> = previous?.posted ? sendCancel(previous.request_id) : Promise.resolve(undefined); ocelRequest.current?.abort(); ocelRequest.current = null; setMining(false); return {previous, rollback};};
  const handBackLiveSource = (previousSource: string | null | undefined, rollback: Promise<LiveSourceRollback>) => void liveSourceHandback.current.handback(previousSource, rollback, sourceId => {if (sourceRequest.current) return; liveSource.current = sourceId; setLiveSourceId(sourceId);});
  // A cancelled load leaves the previous analysis on screen; the backend restores its session, so it is live again.
  // That holds even once the response was written: the backend keeps the previous session until this page
  // acknowledges the applied response (acknowledgeMiningRequest), sends a request on the new source, or its
  // rollback window (60 s) ends.
  const cancelSourceLoad = () => {const {previous, rollback} = abortSourceLoad(); handBackLiveSource(previous?.previousSource, rollback);};
  // A failed load: roll the backend back in case it published the session before the response failed to arrive,
  // or the response arrived but could not be applied (it was never acknowledged).
  const failSourceLoad = (pending: {request_id: string; posted: boolean; previousSource: string | null} | null) => {if (pending) handBackLiveSource(pending.previousSource, pending.posted ? sendCancel(pending.request_id) : Promise.resolve(undefined));};
  const beginSourceLoad = () => {setOwnedExactInspection(false); liveSourceHandback.current.supersede(); const previousSource = sourceRequest.current ? sourceRequest.current.previousSource : liveSource.current; abortSourceLoad(); clearSourceCaches(); liveSource.current = null; setLiveSourceId(null); const ticket = sourceGate.current.issue(), controller = new AbortController(); sourceRequest.current = {controller, request_id: ticket.request_id, posted: false, previousSource}; setMining(true); setError(null); return {ticket, controller, cancelsSettled: cancelBarrier.current};};
  useEffect(() => () => {sourceGate.current.invalidate(); const pending = sourceRequest.current; pending?.controller.abort(); if (pending?.posted) void cancelMiningRequest(pending.request_id); ocelRequest.current?.abort();}, []);
  // A run on the imported model is stale once another model has been imported, even though its source is unchanged.
  const importedModelGuard = (miner: string | undefined) => {const generation = importedModelGeneration.current; return () => {if (miner === "imported" && generation !== importedModelGeneration.current) throw new Error("The imported model changed while this request was pending; run it again.");};};

  useEffect(() => {
    document.fonts.ready.then(() => setFontsReady(true));
  }, []);

  // Returns a model's alignments: embedded (CLI exports), cached, or fetched
  // from the backend (server mines defer it). Stable except across data loads.
  const requestAlignment = useCallback(
    async (minerId: string, options: AlignmentOptions = {}): Promise<Alignments> => {
      const effective = { ...DEFAULT_ALIGNMENT_OPTIONS, ...options };
      const cacheKey = `${resultCacheScope()}|${minerId}|${JSON.stringify(effective)}`;
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
      const sameModel = importedModelGuard(minerId);
      const result = await alignModel(minerId, options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current); sameModel();
      alignCache.current = { ...alignCache.current, [cacheKey]: result };
      return result;
    },
    [data],
  );

  // Decision mining for a model, fetched on demand and cached like alignments.
  const requestDecisions = useCallback(async (minerId: string, options: DecisionMiningOptions = {}): Promise<Decisions> => {
    const cacheKey = `${resultCacheScope()}|${minerId}|${JSON.stringify(options)}`;
    const cached = decideCache.current[cacheKey];
    if (cached) return cached;
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const sameModel = importedModelGuard(minerId); const result = await decideModel(minerId, options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current); sameModel();
    decideCache.current = { ...decideCache.current, [cacheKey]: result };
    return result;
  }, []);

  const requestDfg = useCallback(async (options: DfgAlignmentOptions = {}) => {
    const key = resultCacheScope() + '|' + JSON.stringify(options);
    if (dfgCache.current[key]) return dfgCache.current[key];
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const result = await alignDfg(options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    dfgCache.current = { ...dfgCache.current, [key]: result };
    return result;
  }, []);
  const requestEditDistance = useCallback(async (reference: File, options: EditDistanceOptions = {}) => {
    const key = `${resultCacheScope()}|${reference.name}|${reference.size}|${reference.lastModified}|${JSON.stringify(options)}`;
    if (editCache.current[key]) return editCache.current[key];
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const result = await alignEditDistance(reference, options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    editCache.current = { ...editCache.current, [key]: result };
    return result;
  }, []);
  const requestEvaluation = useCallback(async (miner: string, options: EvaluationOptions = {}) => {
    const key = `${resultCacheScope()}|${miner}|${evaluationQuery(options)}`;
    const cached = evaluationCache.current[key];
    if (cached) return cached;
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const sameModel = importedModelGuard(miner); const result = await evaluateModel(miner, options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current); sameModel();
    evaluationCache.current = { ...evaluationCache.current, [key]: result };
    return result;
  }, []);
  const requestPetriAnalysis = useCallback(async (options: PetriAnalysisOptions = {}): Promise<PetriAnalysisResult> => {
    const key = resultCacheScope() + '|' + petriAnalysisQuery(options);
    const cached = petriAnalysisCache.current[key];
    if (cached) return cached;
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const sameModel = importedModelGuard(options.miner); const result = await analyzePetriModel(options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current); sameModel();
    petriAnalysisCache.current = { ...petriAnalysisCache.current, [key]: result };
    return result;
  }, []);
  const requestEmd = useCallback(async (reference: File, options: EmdOptions = {}) => {
    const key = `${resultCacheScope()}|${reference.name}|${reference.size}|${reference.lastModified}|${emdQuery(options)}`;
    const cached = emdCache.current[key];
    if (cached) return cached;
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const result = await evaluateEmd(reference, options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    emdCache.current = { ...emdCache.current, [key]: result };
    return result;
  }, []);
  const requestConceptDriftCached = useCallback(async (options: ConceptDriftOptions = {}): Promise<ConceptDriftResult> => {
    const key = resultCacheScope() + '|' + conceptDriftQuery(options);
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
    const key = resultCacheScope() + '|' + processTreeAlignmentQuery(options);
    if (processTreeCache.current[key]) return processTreeCache.current[key];
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const result = await alignProcessTree(options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    processTreeCache.current = { ...processTreeCache.current, [key]: result };
    return result;
  }, []);
  const requestDecomposed = useCallback(async (miner: string, options: DecomposedAlignmentOptions = {}) => {
    const key = `${resultCacheScope()}|${miner}|${decomposedAlignmentQuery(options)}`;
    if (decomposedCache.current[key]) return decomposedCache.current[key];
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const sameModel = importedModelGuard(miner); const result = await alignDecomposed(miner, options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current); sameModel(); decomposedCache.current = { ...decomposedCache.current, [key]: result }; return result;
  }, []);
  const requestPetriAnti = useCallback(async (miner: string, options: PetriAntiAlignmentOptions = {}) => {
    const key = `${resultCacheScope()}|${miner}|${petriAntiAlignmentQuery(options)}`;
    if (petriAntiCache.current[key]) return petriAntiCache.current[key];
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const sameModel = importedModelGuard(miner); const result = await alignPetriAnti(miner, options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current); sameModel(); petriAntiCache.current = { ...petriAntiCache.current, [key]: result }; return result;
  }, []);
  const requestPetriMulti = useCallback(async (miner: string, options: PetriMultiAlignmentOptions = {}) => {
    const key = `${resultCacheScope()}|${miner}|${petriMultiAlignmentQuery(options)}`;
    if (petriMultiCache.current[key]) return petriMultiCache.current[key];
    const generation = cacheGeneration.current, sourceId = liveSource.current;
    assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current);
    const sameModel = importedModelGuard(miner); const result = await alignPetriMulti(miner, options); assertMiningSourceBinding(sourceId, liveSource.current, generation, cacheGeneration.current); sameModel(); petriMultiCache.current = { ...petriMultiCache.current, [key]: result }; return result;
  }, []);
  const requestReduction = useCallback(async (traceIndex: number, options: ProcessTreeReductionOptions = {}) => {
    const key = `${resultCacheScope()}|${traceIndex}|${processTreeReductionQuery(options)}`;
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
    importedModelGeneration.current += 1;
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
    setTool(null);
    setPendingSourceFile(file);
    const {ticket, controller, cancelsSettled} = beginSourceLoad();
    const accepts = () => sourceRequest.current?.controller === controller && sourceGate.current.accepts(ticket, controller.signal);
    (async () => {
      // Minera JSON exports open directly; object-centric JSON is discovered
      // through the local backend, and other files are mined as event logs.
      let options: MineOptions = {...xesOptions, delimiter: delimiterForFile(file.name), xesActivityAttribute, ...miningOptions};
      if (isJsonExport(file.name) && options.miningInputFormat !== "table") {
        const classified = classifyMiningJson(activeMiningSourceText(await file.arrayBuffer()));
        if (!accepts()) return;
        if (classified.kind === "offline") {
          setExactInspection(null);
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
          setExactInspection(null);
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
        await cancelsSettled;
        if (!accepts()) return;
        sourceRequest.current!.posted = true;
        const result = await mineActiveDataset(file, options, controller.signal);
        if (!accepts()) return;
        setExactInspection(exactFilterRun(result) ?? null); setOwnedExactInspection(false); if(exactFilterRun(result))setPreviousExactSuccess(exactFilterRun(result)!);
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
        setSourceLoadSerial(serial => serial + 1);
        if (analysis) openData(analysis); else {setData(null); setOcelResult(null); setOcelFile(null); setOcelDataset(null); setTool("active-source"); setError(null);}
        // Applied: the backend may now release the previous session it kept for a cancel.
        void acknowledgeMiningRequest(ticket.request_id);
    })()
      .catch((cause: Error) => {
        if (accepts()) {if (cause instanceof OwnedExactFilterResult) {setExactInspection(cause.run);setOwnedExactInspection(true);clearSourceCaches();} else failSourceLoad(sourceRequest.current); setError(`Could not load ${file.name}: ${cause.message}`); if (/\.json$/i.test(file.name)) setTool("active-source");}
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
    // Only the import settings carry over; filters and miner settings come
    // entirely from this request, so Reset ({}) really clears them.
    const original = source, {ticket, controller, cancelsSettled} = beginSourceLoad(), options: MineOptions = {...sourceImportOptions(original.options), ...filters, miningRequestId: ticket.request_id};
    const accepts = () => sourceGate.current.accepts(ticket, controller.signal) && sourceRequest.current?.controller === controller;
    cancelsSettled
      .then(() => {
        if (!accepts()) return null;
        sourceRequest.current!.posted = true;
        return mineActiveDataset(original.file, options, controller.signal);
      })
      .then((result) => {
        if (result === null || !accepts()) return;
        setExactInspection(exactFilterRun(result) ?? null); setOwnedExactInspection(false); if(exactFilterRun(result))setPreviousExactSuccess(exactFilterRun(result)!);
        const descriptor = miningSourceDescriptor(original.file, original.sha256, options, result), analysis = activeDatasetAnalysis(result);
        liveSource.current = descriptor.source_id; setLiveSourceId(descriptor.source_id);
        setSource({...original, ...descriptor}); setSourceOutcome(isActiveSourceMining(result) ? result : null);
        setFiltersActive(hasActiveFilters(filters) || hasActiveMinerOptions(filters));
        clearSourceCaches(); setData(analysis); if (!analysis) setTool("active-source");
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
        void acknowledgeMiningRequest(ticket.request_id);
      })
      .catch((cause: Error) => {if (sourceGate.current.accepts(ticket, controller.signal)) {if (cause instanceof OwnedExactFilterResult) {setExactInspection(cause.run);setOwnedExactInspection(true);clearSourceCaches();} else if (sourceRequest.current?.controller === controller) failSourceLoad(sourceRequest.current); setError(`Could not filter ${original.file.name}: ${cause.message}`);}})
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

  const renderActiveSource = () => <main className="layout-log active-mining-source-workspace">{error&&<div className="error-banner" role="alert">{error}</div>}{exactInspection&&<details><summary>Latest complete exact native result and request</summary><ExactFilterInspection run={exactInspection}/></details>}<ActiveMiningSource source={source} initialFile={pendingSourceFile} data={data} outcome={sourceOutcome} available={sourceAvailable} mining={mining} onOpen={(file,options)=>{loadFile(file,undefined,undefined,options);setTool("active-source");}} onCancel={cancelSourceLoad}/>{source&&<QueryWorkspace key={cacheGeneration.current} sourceId={source.source_id} dataset={source.file.name} sourceOptions={{miningInputShape:source.shape,miningSelectors:source.selectors}} available={sourceAvailable} mining={mining} onOpenFiltered={(file,options)=>loadFile(file,undefined,undefined,options)}/>}</main>;
  const closeTool = () => setTool(null);
  const openTool = (id: ToolId) => { setTool(id); setSidebarOpen(false); setExportOpen(false); window.scrollTo?.(0, 0); };
  const selectView = (view: Tab) => { setTool(null); setOwnedExactInspection(false); setTab(view); setSidebarOpen(false); setExportOpen(false); window.scrollTo?.(0, 0); };
  const goHome = () => {
    setTool(null);
    setSidebarOpen(false);
    setExportOpen(false);
    setOwnedExactInspection(false);
    abortSourceLoad();
    liveSourceHandback.current.supersede();
    setExactInspection(null);
    liveSource.current = null; setLiveSourceId(null); setSourceOutcome(null); setPendingSourceFile(null);
    cacheGeneration.current += 1;
    setData(null);
    setOcelResult(null);
    setOcelFile(null);
    setOcelDataset(null);
    setError(null);
    setSource(null);
    setFiltersActive(false);
    conceptDriftCache.current = {};
  };

  // ⌘K / Ctrl+K opens the palette from anywhere.
  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "k") { event.preventDefault(); setPaletteOpen((open) => !open); }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);

  const views = data
    ? VIEWS.filter((view) => {
        if (view.id === "log") return Boolean(data.traces && data.traces.length > 0);
        if (view.id === "dotted") return hasDottedChart;
        if (view.id === "conformance") return Boolean(data.conformance);
        if (view.id === "resources") return Boolean(source || (data.resources && data.resources.resources.length > 0));
        return true;
      })
    : [];
  const shownTab = displayedTab(tab, views.map((item) => item.id));
  const datasetCard: SidebarDataset | null = data
    ? { kind: "Event log", name: data.dataset, detail: `${numberFormat.format(data.trace_count)} cases · ${numberFormat.format(data.event_count)} events` }
    : ocelResult
      ? { kind: "Object-centric log", name: ocelDataset ?? "object-centric log" }
      : source
        ? { kind: "Event source", name: source.file.name, detail: mining ? "Mining…" : "Not mined yet" }
        : null;
  const exportDataset = data?.dataset ?? ocelDataset ?? source?.file.name ?? "Independent workspace";
  const commands = paletteCommands({
    views,
    onSelectView: selectView,
    onOpenTool: openTool,
    extra: [
      { id: "action:home", title: "Go to start", section: "Navigate", detail: "Close the open data and return to the start page.", run: goHome },
      ...(data || ocelResult ? [] : [
        { id: "action:sample", title: "Open the sample log", section: "Samples", detail: "Order-to-cash sample with 320 cases.", run: loadSample },
        { id: "action:ocel-sample", title: "Open the object-centric sample", section: "Samples", detail: "Orders and items.", run: loadOcelSample },
      ]),
    ],
  });
  const palette = paletteOpen && <CommandPalette commands={commands} onClose={() => setPaletteOpen(false)} />;
  const dropveil = (
    <div className={dragging ? "dropveil active" : "dropveil"}>
      <div className="dropveil-card">Drop an event source, Minera export or OCEL file</div>
    </div>
  );
  const exportToggle = (
    <button className={exportOpen ? "btn-ghost active" : "btn-ghost"} aria-expanded={exportOpen} onClick={() => setExportOpen((open) => !open)}>
      Export or explain
    </button>
  );
  const shell = (header: ReactNode, content: ReactNode, workspace?: { title: string; active: boolean; onSelect: () => void }, kind: "tool" | "workspace" = "workspace") => (
    <div className="shell">
      <Sidebar
        dataset={datasetCard}
        views={views}
        activeView={shownTab}
        onSelectView={selectView}
        workspace={workspace}
        activeTool={tool}
        onOpenTool={openTool}
        onHome={goHome}
        onOpenFile={loadFile}
        onOpenPalette={() => setPaletteOpen(true)}
        mining={mining}
        open={sidebarOpen}
        onClose={() => setSidebarOpen(false)}
      />
      {sidebarOpen && <div className="sidebar-veil" onClick={() => setSidebarOpen(false)} />}
      <div className="shell-main" data-view={kind}>
        <div className="mobile-bar">
          <button className="mobile-menu" onClick={() => setSidebarOpen(true)} aria-label="Open navigation">☰</button>
          <span className="brand-name">Minera</span>
          <button className="mobile-menu" onClick={() => setPaletteOpen(true)} aria-label="Search views and tools">⌕</button>
        </div>
        {header}
        {content}
      </div>
      {palette}
      {dropveil}
    </div>
  );
  const ocelWorkspace = !data && ocelResult ? { title: "Object-centric workspace", active: tool === null, onSelect: closeTool } : undefined;

  if (tool) {
    const spec = toolById(tool);
    const content = (() => {
      switch (tool) {
        case "saved-structures": return <main className="layout-log"><SavedStructuralWorkspace/></main>;
        case "prompt-chat": return <main className="layout-log"><PromptChat mining={mining} dataset="Plain prompt"/></main>;
        case "log-query": return <main className="layout-log"><OriginalLogQueryWorkspace onOpenLog={(file,activityKey)=>{setTool(null);loadFile(file,activityKey);}}/></main>;
        case "labels": return <main className="layout-log"><OriginalLabelSplittingWorkspace/></main>;
        case "features": return <main className="layout-log"><OriginalFeaturesWorkspace/></main>;
        case "oc-playout": return <main className="layout-log original-oc-playout-workspace"><OriginalObjectCentricPlayoutWorkspace onOpenOcel={loadFile}/></main>;
        case "petri-analysis": return <main className="layout-log original-petri-analysis-workspace"><OriginalPetriAnalysisWorkspace standalone/></main>;
        case "anonymization": return <main className="layout-log original-anonymization-workspace"><OriginalAnonymizationWorkspace standalone/></main>;
        case "clustering": return <main className="layout-log original-clustering-workspace"><OriginalClusteringWorkspace standalone/></main>;
        case "organizational": return <main className="layout-log original-organizational-workspace"><OriginalOrganizationalWorkspace standalone/></main>;
        case "temporal": return <main className="layout-log original-temporal-workspace"><OriginalTemporalWorkspace standalone/></main>;
        case "timing": return <main className="layout-log original-timing-workspace"><OriginalTimingWorkspace standalone/></main>;
        case "time-analysis": return <main className="layout-log original-time-analysis-workspace"><OriginalTimeAnalysisWorkspace standalone/></main>;
        case "quality": return <main className="layout-log original-quality-workspace"><OriginalQualityWorkspace standalone/></main>;
        case "root-cause": return <main className="layout-log original-root-cause-workspace"><OriginalRootCauseWorkspace standalone/></main>;
        case "ilp-genetic": return <main className="layout-log original-discovery-workspace"><OriginalDiscoveryWorkspace standalone/></main>;
        case "bose": return <main className="layout-log original-bose-workspace"><OriginalBoseWorkspace standalone/></main>;
        case "edit-distance": return <main className="layout-log original-edit-distance-workspace"><OriginalEditDistanceWorkspace standalone/></main>;
        case "emd": return <main className="layout-log original-emd-workspace"><OriginalEmdWorkspace/></main>;
        case "embedding-filter": return <main className="layout-log original-embedding-filter-workspace"><OriginalEmbeddingFilterWorkspace file={source?.file??null} source={source} onOpenLog={loadFile}/></main>;
        case "genetic-matrix": return <main className="layout-log original-genetic-matrix-workspace"><OriginalGeneticMatrixWorkspace/></main>;
        case "footprints": return <main className="layout-log original-footprints-workspace"><OriginalFootprintsWorkspace file={source?.file??null} source={source} onOpenLog={loadFile}/></main>;
        case "log-skeleton": return <main className="layout-log original-log-skeleton-workspace"><OriginalLogSkeletonWorkspace file={source?.file??null} source={source} onOpenLog={loadFile}/></main>;
        case "declare": return <main className="layout-log original-declare-workspace"><OriginalDeclareWorkspace file={source?.file??null} source={source} onOpenLog={loadFile}/></main>;
        case "decisions": return <main className="layout-log original-decision-workspace"><OriginalDecisionWorkspace file={source?.file??null} source={source}/></main>;
        case "playout": return <main className="layout-log playout-workspace"><PlayoutWorkspace file={source?.file??null} source={source} onOpenLog={loadFile}/></main>;
        case "monte-carlo": return <main className="layout-log monte-carlo-workspace"><MonteCarloWorkspace file={source?.file??null} source={source} onOpenLog={loadFile}/></main>;
        case "model-conversion": return <main className="layout-log model-conversion-workspace"><ModelConversionWorkspace/></main>;
        case "anti-multi": return <main className="layout-log petri-anti-multi-workspace"><PetriAntiMultiWorkspace file={source?.file??null} source={source}/></main>;
        case "tree-alignment": return <main className="layout-log process-tree-alignment-workspace"><ProcessTreeAlignmentWorkspace file={source?.file??null} source={source}/></main>;
        case "inductive-inputs": return <main className="layout-log inductive-input-workspace"><InductiveInputWorkspace file={source?.file??null} source={source}/></main>;
        case "graph-alignment": return <main className="layout-log graph-alignment-workspace"><GraphAlignmentWorkspace file={source?.file??null} source={source}/></main>;
        case "workflow-inputs": return <main className="layout-log workflow-input-workspace"><WorkflowInputWorkspace file={source?.file??null} source={source}/></main>;
        case "petri-alignment": return <main className="layout-log"><PetriAlignmentWorkspace file={source?.file??null} source={source}/></main>;
        case "token-replay": return <main className="layout-log"><TokenReplayWorkspace file={source?.file??null} source={source}/></main>;
        case "ocel-io": return <main className="layout-log"><OcelIoWorkspace file={ocelFile} busy={mining} onOpen={loadFile}/></main>;
        case "acquisition": return <main className="layout-log"><SourceAcquisitionWorkspace busy={mining} onOpen={loadFile}/></main>;
        case "model-exchange": return <main className="layout-log"><ModelExchangeWorkspace available={sourceAvailable} busy={mining} onUseModel={(file,sourceId,activityKey)=>{setDiscoveryModelDraft({generation:cacheGeneration.current,file,sourceId,activityKey,inferLegacyBoundaries:false});setTool(null);setTab("conformance");}}/></main>;
        case "process-cube": return <main className="layout-log"><ProcessCubeWorkspace busy={mining}/></main>;
        case "tree-generation": return <main className="layout-log"><TreeGenerationWorkspace onOpenLog={loadFile} sourceAvailable={sourceAvailable} onUseModel={async file=>{const generation=cacheGeneration.current;const schema=await requestLogQuerySchema();if(generation!==cacheGeneration.current)throw new Error("The active log changed; check this model again.");setDiscoveryModelDraft({generation,file,sourceId:schema.source_id,inferLegacyBoundaries:false});setTool(null);setTab("conformance");}}/></main>;
        case "streaming": return <main className="layout-log"><StreamingWorkspace file={source?.file??null} sourceFormat={source?.format} busy={mining} data={data??undefined} onOpen={loadFile}/></main>;
        case "related-logs": return <main className="layout-log"><RelatedLogsWorkspace file={source?.file??null} sourceFormat={source?.format} available={sourceAvailable} mining={mining} dataset={data?.dataset??source?.file.name} onOpen={loadFile}/></main>;
        default: { const unknown: never = tool; throw new Error(`no view for tool ${String(unknown)}`); }
        case "active-source": return renderActiveSource();
      }
    })();
    return shell(
      <>
        <PageHeader eyebrow={toolGroup(spec.group).title} title={spec.title} summary={spec.summary} actions={exportToggle} />
        {exportOpen && <VisualizationExplain dataset={exportDataset} busy={mining} />}
      </>,
      content,
      ocelWorkspace,
      "tool",
    );
  }
  if (exactInspection && ownedExactInspection) return shell(
    <PageHeader eyebrow="Filter result" title="Exact native operation" summary="The complete native result and diagnostics of the last exact filter." actions={<button className="btn-ghost" onClick={() => setOwnedExactInspection(false)}>Back to the log</button>} />,
    <main className="layout-log"><ExactFilterInspection run={exactInspection}/>{previousExactSuccess&&previousExactSuccess!==exactInspection&&<details><summary>Earlier returned exact native result</summary><ExactFilterInspection run={previousExactSuccess}/></details>}</main>,
  );
  if (!data && source) {
    const spec = toolById("active-source");
    return shell(<PageHeader eyebrow={toolGroup(spec.group).title} title={spec.title} summary={spec.summary} />, renderActiveSource());
  }
  if (!data && ocelResult) {
    return shell(
      <>
        <PageHeader eyebrow="Object-centric log" title={ocelDataset ?? "Object-centric log"} summary="Discover object-centric models, explore objects and ask a model about the log." actions={exportToggle} />
        {error && <div className="error-banner">{error}</div>}
        {exportOpen && <VisualizationExplain key={cacheGeneration.current+":"+ocelResult.variant} dataset={ocelDataset??"object-centric log"} busy={mining} onUse={draft=>{setVisualizationDraft({generation:cacheGeneration.current,draft});if(ocelQuestions.current)ocelQuestions.current.open=true;ocelQuestions.current?.scrollIntoView?.({behavior:"smooth",block:"start"});}} />}
      </>,
      <main className="layout-log">
        <OcelWorkspaceHub
          result={ocelResult}
          file={ocelFile}
          busy={mining}
          onDiscover={changeOcelModel}
          onOpen={loadFile}
        />
        <details ref={ocelQuestions} className="ocel-model-question"><summary>Ask a model about this object-centric log</summary><PromptChat key={cacheGeneration.current+":"+(visualizationDraft?.generation===cacheGeneration.current?visualizationDraft.draft.id:"")} mining={mining} available={ocelFile!==null} ocelFile={ocelFile} dataset={ocelDataset??undefined} visualization={visualizationDraft?.generation===cacheGeneration.current?visualizationDraft.draft:null} /></details>
      </main>,
      ocelWorkspace,
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
          onOpenTool={openTool}
          onOpenPalette={() => setPaletteOpen(true)}
          error={error}
          mining={mining}
        />
        {palette}
        {dropveil}
      </>
    );
  }
  const maxEdge = maxEdgeCount(data);
  // The body, header and sidebar all follow the view actually shown: a re-mine
  // (applyFilters keeps the tab) can remove the view that was selected.
  const view = viewById(shownTab);
  const exportable = shownTab!=="llm"&&shownTab!=="query"&&shownTab!=="embeddings"&&shownTab!=="anonymization";
  return shell(
    <>
      <PageHeader
        eyebrow={VIEW_GROUPS.find((group) => group.id === view.group)?.title}
        title={view.title}
        summary={view.summary}
        actions={exportable ? exportToggle : undefined}
      />
      {error && <div className="error-banner">{error}</div>}
      {exportable && exportOpen && <VisualizationExplain key={cacheGeneration.current+":"+shownTab} dataset={data.dataset} busy={mining} onUse={draft=>{setVisualizationDraft({generation:cacheGeneration.current,draft});setTab("llm");setExportOpen(false);}} />}
      {(shownTab === "log" || shownTab === "dotted" || shownTab === "dfg") && <Kpis data={data} />}
      {exactInspection&&<details className="exact-result"><summary>Latest complete exact native result and request</summary><ExactFilterInspection run={exactInspection}/></details>}
      {source && (
        <Filters
          key={sourceLoadSerial}
          activities={source.activities}
          resources={source.resources}
          lifecycles={source.lifecycles}
          attributes={source.attributes}
          caseAttributes={source.caseAttributes}
          active={filtersActive}
          mining={mining}
          onApply={applyFilters}
          onExactEdit={cancelSourceLoad}
        />
      )}
    </>,
    <>
      {shownTab === "log" && (
        <main className="layout-log">
          <EventLogView data={data} available={sourceAvailable} />
        </main>
      )}
      {shownTab === "model" && (
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
            playoutSourceId={sourceAvailable && source ? source.source_id : undefined}
            mining={mining}
            onUseModel={(file,sourceId,activityKey)=>{setDiscoveryModelDraft({generation:cacheGeneration.current,file,sourceId,activityKey});setTab("conformance");}}
          />
        </main>
      )}
      {shownTab === "dotted" && (
        <main className="layout-log">
          <DottedChartView data={data} />
          <PerformanceSpectrumView data={data} />
          <Batches data={data} />
        </main>
      )}
      {shownTab === "timing" && <TimingDiscovery key={cacheGeneration.current} data={data} available={sourceAvailable} mining={mining}/>}
      {shownTab === "behavior" && <BehaviorAnalysis key={cacheGeneration.current} data={data} available={sourceAvailable} mining={mining}/>}
      {shownTab === "conformance" && (
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
            playoutSourceId={sourceAvailable && source ? source.source_id : undefined}
          />
        </main>
      )}
      {shownTab === "resources" && (
        <main className="layout-log">
          <Resources key={cacheGeneration.current} data={data} available={sourceAvailable} mining={mining} />
        </main>
      )}
      {shownTab === "embeddings" && (
        <main className="layout-log">
          <EmbeddingSearch key={cacheGeneration.current} data={data} available={sourceAvailable} mining={mining} />
        </main>
      )}
      {shownTab === "llm" && <main className="layout-log"><AskModel key={cacheGeneration.current+":"+(visualizationDraft?.generation===cacheGeneration.current?visualizationDraft.draft.id:"")} data={data} mining={mining} available={sourceAvailable} onOpenFiltered={loadFile} visualization={visualizationDraft?.generation===cacheGeneration.current?visualizationDraft.draft:null} /></main>}
      {shownTab === "query" && <main className="layout-log"><QueryWorkspace key={cacheGeneration.current} data={data} sourceId={source?.source_id} sourceOptions={source ? {miningInputShape:source.shape,miningSelectors:source.selectors} : undefined} available={sourceAvailable} mining={mining} onOpenFiltered={(file,options)=>loadFile(file,undefined,undefined,options)} /></main>}
      {shownTab === "anonymization" && <main className="layout-log"><AnonymizationWorkspace key={cacheGeneration.current} data={data} available={sourceAvailable} mining={mining} onOpenReleased={loadFile} /></main>}
      {shownTab === "clustering" && <main className="layout-log"><ClusteringWorkspace key={cacheGeneration.current} data={data} available={sourceAvailable} mining={mining} onOpenCluster={loadFile} /></main>}
      {shownTab === "transformation" && <main className="layout-log"><TransformationHub key={cacheGeneration.current} data={data} available={sourceAvailable} file={source?.file??null} sourceFormat={source?.format} mining={mining} onOpenTransformed={loadFile} onUseModel={(file,sourceId,activityKey,inferLegacyBoundaries)=>{setDiscoveryModelDraft({generation:cacheGeneration.current,file,sourceId,activityKey,inferLegacyBoundaries});setTab("conformance");}} /></main>}
      {shownTab === "dfg" && (
        <main className="layout">
          <Advanced className="dfg-discovery-section" title="Configure DFG discovery" hint="Statistical and causal DFG variants on the current source"><DfgDiscovery key={cacheGeneration.current} data={data} available={sourceAvailable} mining={mining}/></Advanced>
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
          <DfgDataframe key={cacheGeneration.current} available={sourceAvailable} mining={mining} />
          <CorrelationMining data={data} />
          <EventuallyFollows data={data} />
        </main>
      )}
    </>,
  );
}
