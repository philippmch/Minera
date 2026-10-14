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
  isMineraExport,
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
  mineCsv,
  requestOcelDiscovery,
  type FilterOptions,
  type CsvOptions,
  type MinerOptions,
  type OcelDiscoveryOptions,
} from "./api";
import {
  delimiterForFile,
  hasActiveFilters,
  hasActiveMinerOptions,
  initialTab,
  isJsonExport,
  isOcelJsonDocument,
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
  const [source, setSource] = useState<{
    file: File;
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

  useEffect(() => {
    document.fonts.ready.then(() => setFontsReady(true));
  }, []);

  // Returns a model's alignments: embedded (CLI exports), cached, or fetched
  // from the backend (server mines defer it). Stable except across data loads.
  const requestAlignment = useCallback(
    async (minerId: string, options: AlignmentOptions = {}): Promise<Alignments> => {
      const effective = { ...DEFAULT_ALIGNMENT_OPTIONS, ...options };
      const cacheKey = `${minerId}|${JSON.stringify(effective)}`;
      const embedded = data?.conformance?.[minerId]?.alignments;
      const isDefault =
        effective.variant === DEFAULT_ALIGNMENT_OPTIONS.variant &&
        effective.cost_profile === DEFAULT_ALIGNMENT_OPTIONS.cost_profile &&
        Object.keys(effective).length === 2;
      if (embedded && isDefault) return embedded;
      const cached = alignCache.current[cacheKey];
      if (cached) return cached;
      const generation = cacheGeneration.current;
      const result = await alignModel(minerId, options);
      if (generation === cacheGeneration.current) alignCache.current = { ...alignCache.current, [cacheKey]: result };
      return result;
    },
    [data],
  );

  // Decision mining for a model, fetched on demand and cached like alignments.
  const requestDecisions = useCallback(async (minerId: string, options: DecisionMiningOptions = {}): Promise<Decisions> => {
    const cacheKey = `${minerId}|${JSON.stringify(options)}`;
    const cached = decideCache.current[cacheKey];
    if (cached) return cached;
    const generation = cacheGeneration.current;
    const result = await decideModel(minerId, options);
    if (generation === cacheGeneration.current) decideCache.current = { ...decideCache.current, [cacheKey]: result };
    return result;
  }, []);

  const requestDfg = useCallback(async (options: DfgAlignmentOptions = {}) => {
    const key = JSON.stringify(options);
    if (dfgCache.current[key]) return dfgCache.current[key];
    const generation = cacheGeneration.current;
    const result = await alignDfg(options);
    if (generation === cacheGeneration.current) dfgCache.current = { ...dfgCache.current, [key]: result };
    return result;
  }, []);
  const requestEditDistance = useCallback(async (reference: File, options: EditDistanceOptions = {}) => {
    const key = `${reference.name}|${reference.size}|${reference.lastModified}|${JSON.stringify(options)}`;
    if (editCache.current[key]) return editCache.current[key];
    const generation = cacheGeneration.current;
    const result = await alignEditDistance(reference, options);
    if (generation === cacheGeneration.current) editCache.current = { ...editCache.current, [key]: result };
    return result;
  }, []);
  const requestEvaluation = useCallback(async (miner: string, options: EvaluationOptions = {}) => {
    const key = `${miner}|${evaluationQuery(options)}`;
    const cached = evaluationCache.current[key];
    if (cached) return cached;
    const generation = cacheGeneration.current;
    const result = await evaluateModel(miner, options);
    if (generation === cacheGeneration.current) evaluationCache.current = { ...evaluationCache.current, [key]: result };
    return result;
  }, []);
  const requestPetriAnalysis = useCallback(async (options: PetriAnalysisOptions = {}): Promise<PetriAnalysisResult> => {
    const key = petriAnalysisQuery(options);
    const cached = petriAnalysisCache.current[key];
    if (cached) return cached;
    const generation = cacheGeneration.current;
    const result = await analyzePetriModel(options);
    if (generation === cacheGeneration.current) petriAnalysisCache.current = { ...petriAnalysisCache.current, [key]: result };
    return result;
  }, []);
  const requestEmd = useCallback(async (reference: File, options: EmdOptions = {}) => {
    const key = `${reference.name}|${reference.size}|${reference.lastModified}|${emdQuery(options)}`;
    const cached = emdCache.current[key];
    if (cached) return cached;
    const generation = cacheGeneration.current;
    const result = await evaluateEmd(reference, options);
    if (generation === cacheGeneration.current) emdCache.current = { ...emdCache.current, [key]: result };
    return result;
  }, []);
  const requestConceptDriftCached = useCallback(async (options: ConceptDriftOptions = {}): Promise<ConceptDriftResult> => {
    const key = conceptDriftQuery(options);
    const cached = conceptDriftCache.current[key];
    if (cached) return cached;
    const generation = cacheGeneration.current;
    const result = await requestConceptDrift(options);
    if (generation !== cacheGeneration.current) throw new Error("analysis changed while concept-drift request was pending");
    conceptDriftCache.current = { ...conceptDriftCache.current, [key]: result };
    return result;
  }, []);
  const requestProcessTree = useCallback(async (options: ProcessTreeAlignmentOptions = {}) => {
    const key = processTreeAlignmentQuery(options);
    if (processTreeCache.current[key]) return processTreeCache.current[key];
    const generation = cacheGeneration.current;
    const result = await alignProcessTree(options);
    if (generation === cacheGeneration.current) processTreeCache.current = { ...processTreeCache.current, [key]: result };
    return result;
  }, []);
  const requestDecomposed = useCallback(async (miner: string, options: DecomposedAlignmentOptions = {}) => {
    const key = `${miner}|${decomposedAlignmentQuery(options)}`;
    if (decomposedCache.current[key]) return decomposedCache.current[key];
    const generation = cacheGeneration.current;
    const result = await alignDecomposed(miner, options); if (generation === cacheGeneration.current) decomposedCache.current = { ...decomposedCache.current, [key]: result }; return result;
  }, []);
  const requestPetriAnti = useCallback(async (miner: string, options: PetriAntiAlignmentOptions = {}) => {
    const key = `${miner}|${petriAntiAlignmentQuery(options)}`;
    if (petriAntiCache.current[key]) return petriAntiCache.current[key];
    const generation = cacheGeneration.current;
    const result = await alignPetriAnti(miner, options); if (generation === cacheGeneration.current) petriAntiCache.current = { ...petriAntiCache.current, [key]: result }; return result;
  }, []);
  const requestPetriMulti = useCallback(async (miner: string, options: PetriMultiAlignmentOptions = {}) => {
    const key = `${miner}|${petriMultiAlignmentQuery(options)}`;
    if (petriMultiCache.current[key]) return petriMultiCache.current[key];
    const generation = cacheGeneration.current;
    const result = await alignPetriMulti(miner, options); if (generation === cacheGeneration.current) petriMultiCache.current = { ...petriMultiCache.current, [key]: result }; return result;
  }, []);
  const requestReduction = useCallback(async (traceIndex: number, options: ProcessTreeReductionOptions = {}) => {
    const key = `${traceIndex}|${processTreeReductionQuery(options)}`;
    if (reductionCache.current[key]) return reductionCache.current[key];
    const generation = cacheGeneration.current;
    const result = await reduceProcessTree(traceIndex, options); if (generation === cacheGeneration.current) reductionCache.current = { ...reductionCache.current, [key]: result }; return result;
  }, []);

  // Checks an uploaded PNML or BPMN model against the mined log. A new upload
  // replaces the server's cached "imported" net, so its stale alignments and
  // decisions are dropped.
  const requestModelCheck = useCallback(async (file: File, sourceId?: string, activityKey?: string, inferLegacyBoundaries?: boolean) => {
    const generation = cacheGeneration.current;
    const result = await checkModel(file, {sourceId,activityKey,pnmlInferLegacyBoundaries:inferLegacyBoundaries});
    if (generation !== cacheGeneration.current)
      throw new Error("The active analysis changed during model checking; choose the model again.");
    cacheGeneration.current += 1;
    alignCache.current = Object.fromEntries(
      Object.entries(alignCache.current).filter(([key]) => !key.startsWith("imported|")),
    );
    decideCache.current = Object.fromEntries(
      Object.entries(decideCache.current).filter(([key]) => !key.startsWith("imported|")),
    );
    decomposedCache.current = Object.fromEntries(Object.entries(decomposedCache.current).filter(([key]) => !key.startsWith("imported|")));
    petriAntiCache.current = Object.fromEntries(Object.entries(petriAntiCache.current).filter(([key]) => !key.startsWith("imported|")));
    petriMultiCache.current = Object.fromEntries(Object.entries(petriMultiCache.current).filter(([key]) => !key.startsWith("imported|")));
    evaluationCache.current = Object.fromEntries(Object.entries(evaluationCache.current).filter(([key]) => !key.startsWith("imported|")));
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
    ocelRequest.current?.abort();
    const controller = new AbortController();
    ocelRequest.current = controller;
    setMining(true);
    requestOcelDiscovery(ocelFile, options, controller.signal)
      .then((result) => {
        if (ocelRequest.current !== controller || controller.signal.aborted) return;
        cacheGeneration.current += 1;
        setOcelResult(result);
        setError(null);
      })
      .catch((cause: Error) => {
        if (ocelRequest.current === controller && !controller.signal.aborted) setError(`Could not discover ${options.variant}: ${cause.message}`);
      })
      .finally(() => { if (ocelRequest.current === controller) setMining(false); });
  }, [ocelFile]);

  const loadFile = (file: File, xesActivityAttribute?: string, xesOptions?: CsvOptions) => {
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
    ocelRequest.current?.abort();
    ocelRequest.current = null;
    setMining(true);
    (async () => {
      // Minera JSON exports open directly; object-centric JSON is discovered
      // through the local backend, and other files are mined as event logs.
      if (isJsonExport(file.name)) {
        const parsed: unknown = JSON.parse(await file.text());
        if (isMineraExport(parsed)) {
          setSource(null);
          openData(parsed);
        } else {
          if (!isOcelJsonDocument(parsed)) {
            throw new Error("not a supported JSON export");
          }
          const result = await requestOcelDiscovery(file, { variant: "ocdfg" });
          cacheGeneration.current += 1;
          setData(null);
          setOcelResult(result);
          setOcelFile(file);
          setOcelDataset(file.name);
          setSource(null);
          setFiltersActive(false);
          setError(null);
        }
      } else {
        const delimiter = delimiterForFile(file.name);
        const result = await mineCsv(file, { ...xesOptions, delimiter, xesActivityAttribute });
        setSource({
          file,
          delimiter,
          xesActivityAttribute,
          xesOptions,
          activities: result.activities.map((a) => a.name),
          resources: result.resources?.resources.map((r) => r.name) ?? [],
          lifecycles: logLifecycles(result),
          attributes: result.event_attributes ?? [],
          caseAttributes: result.case_attributes ?? [],
        });
        setFiltersActive(false);
        openData(result);
      }
    })()
      .catch((cause: Error) => {
        setError(`Could not load ${file.name}: ${cause.message}`);
      })
      .finally(() => setMining(false));
  };

  const loadSample = () => {
    fetch(`${import.meta.env.BASE_URL}sample-data.json`)
      .then((response) => response.json())
      .then((sample) => {
        if (!isMineraExport(sample)) throw new Error("invalid export");
        setSource(null);
        openData(sample);
      })
      .catch(() => setError("Could not load the bundled sample dataset."));
  };

  const loadOcelSample = () => {
    setMining(true);
    fetch(`${import.meta.env.BASE_URL}sample-ocel.json`)
      .then(response => { if (!response.ok) throw new Error("sample not found");return response.text(); })
      .then(text => loadFile(new File([text],"sample-ocel.json",{type:"application/json"})))
      .catch(() => {setError("Could not load the object-centric sample.");setMining(false);});
  };

  const loadResourceSample = () => {
    setMining(true);
    fetch(`${import.meta.env.BASE_URL}sample-resource-log.csv`)
      .then(response => { if (!response.ok) throw new Error("sample not found"); return response.text(); })
      .then(text => loadFile(new File([text], "sample-resource-log.csv", {type:"text/csv"})))
      .catch(() => { setError("Could not load the resource process sample."); setMining(false); });
  };

  // Re-mines the original upload with the given filters and miner settings,
  // keeping the active tab. Always mines from source.file so nothing compounds.
  const applyFilters = (filters: FilterOptions & MinerOptions) => {
    if (!source) return;
    setFiltersActive(hasActiveFilters(filters) || hasActiveMinerOptions(filters));
    setMining(true);
    mineCsv(source.file, { ...source.xesOptions, delimiter: source.delimiter, ...filters,
      xesActivityAttribute: source.xesActivityAttribute })
      .then((result) => {
        cacheGeneration.current += 1;
        setData(result);
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
      .catch((cause: Error) =>
        setError(`Could not filter ${source.file.name}: ${cause.message}`),
      )
      .finally(() => setMining(false));
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

  if (showWorkflowInputs) return <><TopBar dataset={data?.dataset??ocelDataset??'Workflow inputs'} datasetKind="independent workflow discovery" onOpenFile={loadFile} onHome={()=>setShowWorkflowInputs(false)}/><main className="layout-log workflow-input-workspace"><button className="btn-ghost" onClick={()=>setShowWorkflowInputs(false)}>Return to current workspace</button><WorkflowInputWorkspace file={source?.file??null}/></main></>;
  if (showPetriAlignment) return <><TopBar dataset={data?.dataset??ocelDataset??'Alignment inputs'} datasetKind="Petri alignment" onOpenFile={loadFile} onHome={()=>setShowPetriAlignment(false)}/><main className="layout-log"><button className="btn-ghost" onClick={()=>setShowPetriAlignment(false)}>Return to current workspace</button><PetriAlignmentWorkspace file={source?.file??null}/></main></>;
  if (showTokenReplay) return <><TopBar dataset={data?.dataset??ocelDataset??'Replay inputs'} datasetKind="token replay" onOpenFile={loadFile} onHome={()=>setShowTokenReplay(false)}/><main className="layout-log"><button className="btn-ghost" onClick={()=>setShowTokenReplay(false)}>Return to current workspace</button><TokenReplayWorkspace file={source?.file??null}/></main></>;
  if (showOcelIo) return <><TopBar dataset={ocelDataset??data?.dataset??'OCEL file exchange'} datasetKind="file exchange" onOpenFile={loadFile} onHome={()=>setShowOcelIo(false)}/><main className="layout-log"><button className="btn-ghost" onClick={()=>setShowOcelIo(false)}>Return to current workspace</button><OcelIoWorkspace file={ocelFile} busy={mining} onOpen={loadFile}/></main></>;
  if (showAcquisition) return <><TopBar dataset={data?.dataset??ocelDataset??'Activity sources'} datasetKind="source acquisition" onOpenFile={loadFile} onHome={()=>setShowAcquisition(false)}/><main className="layout-log"><button className="btn-ghost" onClick={()=>setShowAcquisition(false)}>Return to current workspace</button><SourceAcquisitionWorkspace busy={mining} onOpen={loadFile}/></main></>;

  if (showModelExchange) return <><TopBar dataset={data?.dataset??ocelDataset??'Model files'} datasetKind="model exchange" onOpenFile={loadFile} onHome={()=>setShowModelExchange(false)}/><main className="layout-log"><button className="btn-ghost" onClick={()=>setShowModelExchange(false)}>Return to current workspace</button><ModelExchangeWorkspace available={source!==null} busy={mining} onUseModel={(file,sourceId,activityKey)=>{setDiscoveryModelDraft({generation:cacheGeneration.current,file,sourceId,activityKey,inferLegacyBoundaries:false});setShowModelExchange(false);setTab("conformance");}}/></main></>;
  if (showProcessCube) return <><TopBar dataset={data?.dataset??ocelDataset??'Process cubes'} datasetKind="process cube" onOpenFile={loadFile} onHome={()=>setShowProcessCube(false)}/><main className="layout-log"><button className="btn-ghost" onClick={()=>setShowProcessCube(false)}>Return to current workspace</button><ProcessCubeWorkspace busy={mining}/></main></>;
  if (showTreeGeneration) return <><TopBar dataset="Generated models" datasetKind="tree generation" onOpenFile={loadFile} onHome={()=>setShowTreeGeneration(false)}/><main className="layout-log"><button className="btn-ghost" onClick={()=>setShowTreeGeneration(false)}>Return to current workspace</button><TreeGenerationWorkspace onOpenLog={loadFile} sourceAvailable={source!==null} onUseModel={async file=>{const generation=cacheGeneration.current;const schema=await requestLogQuerySchema();if(generation!==cacheGeneration.current)throw new Error("The active log changed; check this model again.");setDiscoveryModelDraft({generation,file,sourceId:schema.source_id,inferLegacyBoundaries:false});setShowTreeGeneration(false);setTab("conformance");}}/></main></>;
  if (showStreaming) return <><TopBar dataset={data?.dataset??ocelDataset??'Event streams'} datasetKind="streaming replay" onOpenFile={loadFile} onHome={()=>setShowStreaming(false)}/><main className="layout-log"><button className="btn-ghost" onClick={()=>setShowStreaming(false)}>Return to current workspace</button><StreamingWorkspace file={source?.file??null} busy={mining} data={data??undefined} onOpen={loadFile}/></main></>;
  if (showRelatedLogs) return <><TopBar dataset={data?.dataset??ocelDataset??'Related logs'} datasetKind="log relationships" onOpenFile={loadFile} onHome={()=>setShowRelatedLogs(false)}/><main className="layout-log"><button className="btn-ghost" onClick={()=>setShowRelatedLogs(false)}>Return to current workspace</button><RelatedLogsWorkspace file={source?.file??null} available={source!==null} mining={mining} dataset={data?.dataset} onOpen={loadFile}/></main></>;

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
          onHome={() => {
            ocelRequest.current?.abort();
            ocelRequest.current = null;
            setMining(false);
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
          <div className="dropveil-card">Drop a Minera JSON export</div>
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
          onImportOcel={()=>setShowOcelIo(true)}
          error={error}
          mining={mining}
        />
        <div className={dragging ? "dropveil active" : "dropveil"}>
          <div className="dropveil-card">Drop a Minera JSON export</div>
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
        onHome={() => {
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
            canExportPnml={source !== null}
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
      {tab === "timing" && <TimingDiscovery key={cacheGeneration.current} data={data} available={source!==null} mining={mining}/>}
      {tab === "behavior" && <BehaviorAnalysis key={cacheGeneration.current} data={data} available={source!==null} mining={mining}/>}
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
          <Resources key={cacheGeneration.current} data={data} available={source !== null} mining={mining} />
        </main>
      )}
      {tab === "embeddings" && (
        <main className="layout-log">
          <EmbeddingSearch key={cacheGeneration.current} data={data} available={source !== null} mining={mining} />
        </main>
      )}
      {tab === "llm" && <main className="layout-log"><AskModel key={cacheGeneration.current+":"+(visualizationDraft?.generation===cacheGeneration.current?visualizationDraft.draft.id:"")} data={data} mining={mining} available={source!==null} onOpenFiltered={loadFile} visualization={visualizationDraft?.generation===cacheGeneration.current?visualizationDraft.draft:null} /></main>}
      {tab === "query" && <main className="layout-log"><QueryWorkspace key={cacheGeneration.current} data={data} available={source!==null} mining={mining} onOpenFiltered={loadFile} /></main>}
      {tab === "anonymization" && <main className="layout-log"><AnonymizationWorkspace key={cacheGeneration.current} data={data} available={source!==null} mining={mining} onOpenReleased={loadFile} /></main>}
      {tab === "clustering" && <main className="layout-log"><ClusteringWorkspace key={cacheGeneration.current} data={data} available={source!==null} mining={mining} onOpenCluster={loadFile} /></main>}
      {tab === "transformation" && <main className="layout-log"><TransformationHub key={cacheGeneration.current} data={data} available={source!==null} file={source?.file??null} mining={mining} onOpenTransformed={loadFile} onUseModel={(file,sourceId,activityKey,inferLegacyBoundaries)=>{setDiscoveryModelDraft({generation:cacheGeneration.current,file,sourceId,activityKey,inferLegacyBoundaries});setTab("conformance");}} /></main>}
      {tab === "dfg" && (
        <main className="layout">
          <DfgDiscovery key={cacheGeneration.current} data={data} available={source!==null} mining={mining}/>
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
        <div className="dropveil-card">Drop a Minera JSON export</div>
      </div>
    </>
  );
}
