interface LandingProps {
  onOpenFile: (file: File) => void;
  onLoadSample: () => void;
  onLoadOcelSample: () => void;
  onLoadResourceSample: () => void;
  error: string | null;
  mining: boolean;
  onImportOcel?: () => void;
  onRelateLogs?: () => void;
  onStreaming?: () => void;
  onModelExchange?: () => void;
  onProcessCube?: () => void;
  onTreeGeneration?: () => void;
  onAcquisition?: () => void;
  onTokenReplay?: () => void;
  onPetriAlignment?: () => void;
  onWorkflowInputs?: () => void;
  onGraphAlignment?: () => void;
  onInductiveInputs?: () => void;
  onTreeAlignment?: () => void;
  onPetriAntiMulti?: () => void;
  onModelConversion?: () => void;
  onMonteCarlo?: () => void;
  onPlayout?: () => void;
  onOriginalDecision?: () => void;
  onOriginalDeclare?: () => void;
  onOriginalLogSkeleton?: () => void;
  onOriginalFootprints?: () => void;
  onOriginalGeneticMatrix?: () => void;
  onOriginalEmbeddingFilter?: () => void;
  onOriginalEmd?: () => void;
  onOriginalEditDistance?: () => void;
  onOriginalBose?: () => void;
  onOriginalDiscovery?: () => void;
  onOriginalPetriAnalysis?: () => void;
  onOriginalRootCause?: () => void;
  onOriginalAnonymization?: () => void;
  onOriginalQuality?: () => void;
  onOriginalClustering?: () => void;
  onOriginalOrganizational?: () => void;
  onOriginalTemporal?: () => void;
  onOriginalTiming?: () => void;
  onOriginalTimeAnalysis?: () => void;
  onOriginalOcPlayout?: () => void;
  onActiveSource?: () => void;
}

export function Landing({ onOpenFile, onLoadSample, onLoadOcelSample, onLoadResourceSample, error, mining, onImportOcel, onRelateLogs, onStreaming, onModelExchange, onProcessCube, onTreeGeneration, onAcquisition, onTokenReplay, onPetriAlignment, onWorkflowInputs, onGraphAlignment, onInductiveInputs, onTreeAlignment, onPetriAntiMulti, onModelConversion, onMonteCarlo, onPlayout, onOriginalDecision, onOriginalDeclare, onOriginalLogSkeleton, onOriginalFootprints, onOriginalGeneticMatrix, onOriginalEmbeddingFilter, onOriginalEmd, onOriginalEditDistance, onOriginalBose, onOriginalDiscovery, onOriginalPetriAnalysis, onOriginalRootCause, onOriginalAnonymization, onOriginalQuality, onOriginalClustering, onOriginalOrganizational, onOriginalTemporal, onOriginalTiming, onOriginalTimeAnalysis, onOriginalOcPlayout, onActiveSource }: LandingProps) {
  return (
    <div className="landing">
      <div className="landing-inner">
        <svg className="landing-mark reveal" viewBox="0 0 24 24" aria-hidden="true">
          <path d="M12 2 22 12 12 22 2 12Z" />
          <path className="landing-mark-core" d="M12 8 16 12 12 16 8 12Z" />
        </svg>
        <h1 className="landing-title reveal">Minera</h1>
        <p className="landing-tagline reveal">process intelligence</p>
        <p className="landing-lede reveal">
          Local process mining for CSV/XES event logs, native EventTable JSON and object-centric JSON.
        </p>

        <label className={mining ? "landing-drop mining" : "landing-drop"}>
          <input
            type="file"
            accept=".csv,.tsv,.xes,.xes.gz,.gz,.json,text/csv,application/xml,application/gzip"
            disabled={mining}
            onChange={(event) => {
              const file = event.target.files?.[0];
              if (file) onOpenFile(file);
              event.target.value = "";
            }}
          />
          <span className="landing-drop-title">
            {mining ? "Mining event log…" : "Drop your event log here"}
          </span>
          <span className="landing-drop-sub">
            or click to browse — .csv (case_id, activity, timestamp), .xes, or .xes.gz
          </span>
        </label>

        {error && <div className="error-banner landing-error">{error}</div>}

        <button className="landing-sample reveal" disabled={mining} onClick={onLoadSample}>
          Explore the sample log instead →
        </button>
        <button className="landing-sample reveal" disabled={mining} onClick={onLoadOcelSample}>
          Explore object-centric orders and items
        </button>
        <button className="landing-sample reveal" disabled={mining} onClick={onLoadResourceSample}>
          Mine a sample with resource relationships
        </button>
        {onImportOcel && <button className="landing-sample reveal" disabled={mining} onClick={onImportOcel}>Import OCEL JSON, XML, CSV or SQLite</button>}
        {onRelateLogs && <button className="landing-sample reveal" disabled={mining} onClick={onRelateLogs}>Relate logs and events</button>}
        {onStreaming && <button className="landing-sample reveal" disabled={mining} onClick={onStreaming}>Replay event streams</button>}
        {onModelExchange && <button className="landing-sample reveal" disabled={mining} onClick={onModelExchange}>Inspect and convert model files</button>}
        {onProcessCube && <button className="landing-sample reveal" disabled={mining} onClick={onProcessCube}>Explore process cubes</button>}
        {onTreeGeneration && <button className="landing-sample reveal" disabled={mining} onClick={onTreeGeneration}>Generate process trees and logs</button>}
        {onAcquisition && <button className="landing-sample reveal" disabled={mining} onClick={onAcquisition}>Acquire source data</button>}
        {onTokenReplay && <button className="landing-sample reveal" disabled={mining} onClick={onTokenReplay}>Replay a log against a PNML model</button>}
        {onPetriAlignment && <button className="landing-sample reveal" disabled={mining} onClick={onPetriAlignment}>Align a log against a PNML model</button>}
        {onWorkflowInputs && <button className="landing-sample reveal" disabled={mining} onClick={onWorkflowInputs}>Discover a workflow from original inputs</button>}
        {onGraphAlignment && <button className="landing-sample reveal" disabled={mining} onClick={onGraphAlignment}>Align original traces against a DFG or PNML model</button>}
        {onInductiveInputs && <button className="landing-sample reveal" disabled={mining} onClick={onInductiveInputs}>Discover an Inductive workflow from original inputs</button>}
        {onTreeAlignment && <button className="landing-sample reveal" disabled={mining} onClick={onTreeAlignment}>Align original traces against a process-tree model</button>}
        {onPetriAntiMulti && <button className="landing-sample reveal" disabled={mining} onClick={onPetriAntiMulti}>Compare original traces with an independent PNML model</button>}
        {onModelConversion && <button className="landing-sample reveal" disabled={mining} onClick={onModelConversion}>Convert or reduce an original process model</button>}
        {onMonteCarlo && <button className="landing-sample reveal" disabled={mining} onClick={onMonteCarlo}>Simulate an original PNML model with Monte Carlo</button>}
        {onPlayout && <button className="landing-sample reveal" disabled={mining} onClick={onPlayout}>Generate a log from an original Petri, DFG or tree model</button>}
        {onOriginalDecision && <button className="landing-sample reveal" disabled={mining} onClick={onOriginalDecision}>Learn decisions from an original PNML model and event source</button>}
        {onOriginalDeclare && <button className="landing-sample reveal" disabled={mining} onClick={onOriginalDeclare}>Check or play out an original Declare model</button>}
        {onOriginalLogSkeleton && <button className="landing-sample reveal" disabled={mining} onClick={onOriginalLogSkeleton}>Check an original LogSkeleton model or diagnose supplied results</button>}
        {onOriginalFootprints && <button className="landing-sample reveal" disabled={mining} onClick={onOriginalFootprints}>Compare original Footprints or use independent diagnostics and projections</button>}
        {onOriginalGeneticMatrix && <button className="landing-sample reveal" disabled={mining} onClick={onOriginalGeneticMatrix}>Convert an original genetic causal matrix to a Petri net</button>}
        {onOriginalEmbeddingFilter && <button className="landing-sample reveal" disabled={mining} onClick={onOriginalEmbeddingFilter}>Filter or embed original logs and literal text</button>}
        {onOriginalEmd && <button className="landing-sample reveal" disabled={mining} onClick={onOriginalEmd}>Compare original weighted stochastic languages</button>}
        {onOriginalEditDistance && <button className="landing-sample reveal" disabled={mining} onClick={onOriginalEditDistance}>Align or project independent original event inputs</button>}
        {onOriginalBose && <button className="landing-sample reveal" disabled={mining} onClick={onOriginalBose}>Inspect original BOSE drift, helpers and caller RNG</button>}
        {onOriginalDiscovery && <button className="landing-sample reveal" disabled={mining} onClick={onOriginalDiscovery}>Inspect original ILP and genetic discovery</button>}
        {onOriginalPetriAnalysis && <button className="landing-sample reveal" disabled={mining} onClick={onOriginalPetriAnalysis}>Inspect original Petri analysis from a complete net</button>}
        {onOriginalRootCause && <button className="landing-sample reveal" disabled={mining} onClick={onOriginalRootCause}>Inspect original root cause from independent cohorts</button>}
        {onOriginalAnonymization && <button className="landing-sample reveal" disabled={mining} onClick={onOriginalAnonymization}>Anonymize independent original logs and released queries</button>}
        {onOriginalClustering && <button className="landing-sample reveal" disabled={mining} onClick={onOriginalClustering}>Cluster independent original logs and inspect full stored results</button>}
        {onOriginalOrganizational && <button className="landing-sample reveal" disabled={mining} onClick={onOriginalOrganizational}>Inspect original social networks, roles, profiles and local diagnostics before mining</button>}
        {onOriginalTemporal && <button className="landing-sample reveal" disabled={mining} onClick={onOriginalTemporal}>Inspect complete original temporal sources, profiles and native helpers before mining</button>}
        {onOriginalTiming && <button className="landing-sample reveal" disabled={mining} onClick={onOriginalTiming}>Inspect original performance spectra and batches with independent inputs</button>}
        {onOriginalTimeAnalysis && <button className="landing-sample reveal" disabled={mining} onClick={onOriginalTimeAnalysis}>Inspect passed, service, cycle and case times with an independent EventLog</button>}
        {onOriginalQuality && <button className="landing-sample reveal" disabled={mining} onClick={onOriginalQuality}>Evaluate independent original sources and models</button>}
        {onOriginalOcPlayout && <button className="landing-sample reveal" disabled={mining} onClick={onOriginalOcPlayout}>Original object-centric playout</button>}
        {onActiveSource && <button className="landing-sample reveal" disabled={mining} onClick={onActiveSource}>Open and inspect a native mining source</button>}

        <div className="landing-steps reveal">
          <div className="landing-step">
            <span className="landing-step-num">01</span>
            <span>
              Export your process data as a CSV event log — one row per event,
              with case id, activity, and timestamp columns
            </span>
          </div>
          <div className="landing-step">
            <span className="landing-step-num">02</span>
            <span>
              Open it here: the native Minera core mines it on your machine
            </span>
          </div>
          <div className="landing-step">
            <span className="landing-step-num">03</span>
            <span>
              Inspect the imported log, then open the directly-follows graph
            </span>
          </div>
        </div>

        <p className="landing-note reveal">
          JSON exports from the <code>minera_export</code> CLI and standard OCEL
          files open here too.
        </p>
      </div>
    </div>
  );
}
