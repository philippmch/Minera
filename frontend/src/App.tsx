import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
  type Alignments,
  type AlignmentOptions,
  type DfgAlignmentOptions,
  type EditDistanceOptions,
  DEFAULT_ALIGNMENT_OPTIONS,
  type CaseAttributeStat,
  type Decisions,
  type EventAttributeStat,
  type MineraExport,
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
import { buildDottedChart } from "./dotted-chart";
import { ActivityTable } from "./components/ActivityTable";
import { Landing } from "./components/Landing";
import { EventLogView } from "./components/EventLogView";
import { EventuallyFollows } from "./components/EventuallyFollows";
import { DfgVariants } from "./components/DfgVariants";
import { CorrelationMining } from "./components/CorrelationMining";
import { Filters } from "./components/Filters";
import {
  alignModel,
  alignDfg,
  alignEditDistance,
  checkModel,
  decideModel,
  mineCsv,
  type FilterOptions,
  type MinerOptions,
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
  const [error, setError] = useState<string | null>(null);
  const [fontsReady, setFontsReady] = useState(false);
  const [threshold, setThreshold] = useState(1);
  const [selectedVariant, setSelectedVariant] = useState<number | null>(null);
  const [dragging, setDragging] = useState(false);
  const [tab, setTab] = useState<Tab>("log");
  const [mining, setMining] = useState(false);
  // The CSV upload behind the current view, kept so filters can re-mine it.
  // null when the data came from a pre-mined JSON export (not re-minable).
  const [source, setSource] = useState<{
    file: File;
    delimiter?: string;
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
      const result = await alignModel(minerId, options);
      alignCache.current = { ...alignCache.current, [cacheKey]: result };
      return result;
    },
    [data],
  );

  // Decision mining for a model, fetched on demand and cached like alignments.
  const requestDecisions = useCallback(async (minerId: string): Promise<Decisions> => {
    const cached = decideCache.current[minerId];
    if (cached) return cached;
    const result = await decideModel(minerId);
    decideCache.current = { ...decideCache.current, [minerId]: result };
    return result;
  }, []);

  const requestDfg = useCallback(async (options: DfgAlignmentOptions = {}) => {
    const key = JSON.stringify(options);
    if (dfgCache.current[key]) return dfgCache.current[key];
    const result = await alignDfg(options);
    dfgCache.current = { ...dfgCache.current, [key]: result };
    return result;
  }, []);
  const requestEditDistance = useCallback(async (reference: File, options: EditDistanceOptions = {}) => {
    const key = `${reference.name}|${reference.size}|${reference.lastModified}|${JSON.stringify(options)}`;
    if (editCache.current[key]) return editCache.current[key];
    const result = await alignEditDistance(reference, options);
    editCache.current = { ...editCache.current, [key]: result };
    return result;
  }, []);

  // Checks an uploaded PNML or BPMN model against the mined log. A new upload
  // replaces the server's cached "imported" net, so its stale alignments and
  // decisions are dropped.
  const requestModelCheck = useCallback(async (file: File) => {
    const result = await checkModel(file);
    alignCache.current = Object.fromEntries(
      Object.entries(alignCache.current).filter(([key]) => !key.startsWith("imported|")),
    );
    const { imported: _droppedDecisions, ...restDecisions } = decideCache.current;
    decideCache.current = restDecisions;
    return result;
  }, []);

  const openData = (parsed: MineraExport) => {
    setData(parsed);
    alignCache.current = {};
    decideCache.current = {};
    dfgCache.current = {};
    editCache.current = {};
    setSelectedVariant(null);
    setThreshold(1);
    setError(null);
    setTab(initialTab(parsed));
  };

  const loadFile = (file: File) => {
    setMining(true);
    (async () => {
      // JSON files are pre-mined exports and open directly; anything else
      // is a raw CSV event log, uploaded to the local backend for mining.
      if (isJsonExport(file.name)) {
        const parsed: unknown = JSON.parse(await file.text());
        if (!isMineraExport(parsed)) throw new Error("not a Minera export");
        setSource(null);
        openData(parsed);
      } else {
        const delimiter = delimiterForFile(file.name);
        const result = await mineCsv(file, { delimiter });
        setSource({
          file,
          delimiter,
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

  // Re-mines the original upload with the given filters and miner settings,
  // keeping the active tab. Always mines from source.file so nothing compounds.
  const applyFilters = (filters: FilterOptions & MinerOptions) => {
    if (!source) return;
    setFiltersActive(hasActiveFilters(filters) || hasActiveMinerOptions(filters));
    setMining(true);
    mineCsv(source.file, { delimiter: source.delimiter, ...filters })
      .then((result) => {
        setData(result);
        // The re-mine replaced the server session, so cached on-demand
        // results belong to the previous analysis.
        alignCache.current = {};
        decideCache.current = {};
        dfgCache.current = {};
        editCache.current = {};
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

  if (!data || !layout) {
    return (
      <>
        <Landing
          onOpenFile={loadFile}
          onLoadSample={loadSample}
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
        onHome={() => {
          setData(null);
          setError(null);
          setSource(null);
          setFiltersActive(false);
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
        {data.resources && data.resources.resources.length > 0 && (
          <button
            className={tab === "resources" ? "tab active" : "tab"}
            onClick={() => setTab("resources")}
          >
            Resources
          </button>
        )}
      </nav>
      <Kpis data={data} />
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
            data={data}
            treeLayout={treeLayout}
            petriLayout={petriLayout}
            inductivePetriLayout={inductivePetriLayout}
            heuristicPetriLayout={heuristicPetriLayout}
            powlPetriLayout={powlPetriLayout}
            canExportPnml={source !== null}
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
      {tab === "conformance" && (
        <main className="layout-log">
          <Conformance
            data={data}
            onRequestAlignment={requestAlignment}
            onRequestDecisions={source ? requestDecisions : undefined}
            onCheckModel={requestModelCheck}
            onRequestDfg={requestDfg}
            onRequestEditDistance={requestEditDistance}
          />
        </main>
      )}
      {tab === "resources" && (
        <main className="layout-log">
          <Resources data={data} />
        </main>
      )}
      {tab === "dfg" && (
        <main className="layout">
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
