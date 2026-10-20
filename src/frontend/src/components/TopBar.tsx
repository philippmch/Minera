interface TopBarProps {
  dataset: string;
  generator?: string;
  datasetKind?: string;
  onOpenFile: (file: File) => void;
  onHome: () => void;
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
  onOriginalFeatures?: () => void;
  onOriginalLabels?: () => void;
  onOriginalLogQuery?: () => void;
  onPromptChat?: () => void;
  onActiveSource?: () => void;
  onSavedStructures?: () => void;
}

export function TopBar({ dataset, generator, datasetKind = "event log", onOpenFile, onHome, onImportOcel, onRelateLogs, onStreaming, onModelExchange, onProcessCube, onTreeGeneration, onAcquisition, onTokenReplay, onPetriAlignment, onWorkflowInputs, onGraphAlignment, onInductiveInputs, onTreeAlignment, onPetriAntiMulti, onModelConversion, onMonteCarlo, onPlayout, onOriginalDecision, onOriginalDeclare, onOriginalLogSkeleton, onOriginalFootprints, onOriginalGeneticMatrix, onOriginalEmbeddingFilter, onOriginalEmd, onOriginalEditDistance, onOriginalBose, onOriginalDiscovery, onOriginalPetriAnalysis, onOriginalRootCause, onOriginalAnonymization, onOriginalQuality, onOriginalClustering, onOriginalOrganizational, onOriginalTemporal, onOriginalTiming, onOriginalTimeAnalysis, onOriginalOcPlayout, onOriginalFeatures, onOriginalLabels, onOriginalLogQuery, onPromptChat, onActiveSource, onSavedStructures }: TopBarProps) {
  return (
    <header className="topbar reveal">
      <button className="brand" onClick={onHome} title="Back to start">
        <svg className="brand-mark" viewBox="0 0 24 24" aria-hidden="true">
          <path d="M12 2 22 12 12 22 2 12Z" />
          <path className="brand-mark-core" d="M12 8 16 12 12 16 8 12Z" />
        </svg>
        <span className="brand-name">Minera</span>
        <span className="brand-tagline">process intelligence</span>
      </button>
      <div className="dataset">
        <span className="dataset-label">{datasetKind}</span>
        <span className="dataset-name">{dataset}</span>
        {generator && <span className="dataset-generator">· {generator}</span>}
      </div>
      {onImportOcel && <button className="btn-ghost" onClick={onImportOcel}>Import OCEL data</button>}
      {onRelateLogs && <button className="btn-ghost" onClick={onRelateLogs}>Relate logs</button>}
      {onStreaming && <button className="btn-ghost" onClick={onStreaming}>Event streams</button>}
      {onModelExchange && <button className="btn-ghost" onClick={onModelExchange}>Model files</button>}
      {onProcessCube && <button className="btn-ghost" onClick={onProcessCube}>Process cubes</button>}
      {onTreeGeneration && <button className="btn-ghost" onClick={onTreeGeneration}>Generate trees</button>}
      {onAcquisition && <button className="btn-ghost" onClick={onAcquisition}>Acquire source data</button>}
      {onTokenReplay && <button className="btn-ghost" onClick={onTokenReplay}>Token replay</button>}
      {onPetriAlignment && <button className="btn-ghost" onClick={onPetriAlignment}>Petri alignment</button>}
      {onWorkflowInputs && <button className="btn-ghost" onClick={onWorkflowInputs}>Workflow inputs</button>}
      {onGraphAlignment && <button className="btn-ghost" onClick={onGraphAlignment}>Graph alignment</button>}
      {onInductiveInputs && <button className="btn-ghost" onClick={onInductiveInputs}>Inductive inputs</button>}
      {onTreeAlignment && <button className="btn-ghost" onClick={onTreeAlignment}>Tree alignment</button>}
      {onPetriAntiMulti && <button className="btn-ghost" onClick={onPetriAntiMulti}>Anti/multi alignment</button>}
      {onModelConversion && <button className="btn-ghost" onClick={onModelConversion}>Convert models</button>}
      {onMonteCarlo && <button className="btn-ghost" onClick={onMonteCarlo}>Original-model Monte Carlo</button>}
      {onPlayout && <button className="btn-ghost" onClick={onPlayout}>Original-model playout</button>}
      {onOriginalDecision && <button className="btn-ghost" onClick={onOriginalDecision}>Original-model decisions</button>}
      {onOriginalDeclare && <button className="btn-ghost" onClick={onOriginalDeclare}>Original Declare</button>}
      {onOriginalLogSkeleton && <button className="btn-ghost" onClick={onOriginalLogSkeleton}>Original LogSkeleton</button>}
      {onOriginalFootprints && <button className="btn-ghost" onClick={onOriginalFootprints}>Original Footprints</button>}
      {onOriginalGeneticMatrix && <button className="btn-ghost" onClick={onOriginalGeneticMatrix}>Original genetic matrix</button>}
      {onOriginalEmbeddingFilter && <button className="btn-ghost" onClick={onOriginalEmbeddingFilter}>Original embedding / filter</button>}
      {onOriginalEmd && <button className="btn-ghost" onClick={onOriginalEmd}>Original weighted-language EMD</button>}
      {onOriginalEditDistance && <button className="btn-ghost" onClick={onOriginalEditDistance}>Original edit distance</button>}
      {onOriginalBose && <button className="btn-ghost" onClick={onOriginalBose}>Original BOSE workbench</button>}
      {onOriginalDiscovery && <button className="btn-ghost" onClick={onOriginalDiscovery}>Original discovery workbench</button>}
      {onOriginalPetriAnalysis && <button className="btn-ghost" onClick={onOriginalPetriAnalysis}>Original Petri analysis</button>}
      {onOriginalRootCause && <button className="btn-ghost" onClick={onOriginalRootCause}>Original root cause</button>}
      {onOriginalAnonymization && <button className="btn-ghost" onClick={onOriginalAnonymization}>Original anonymization</button>}
      {onOriginalClustering && <button className="btn-ghost" onClick={onOriginalClustering}>Original clustering</button>}
      {onOriginalOrganizational && <button className="btn-ghost" onClick={onOriginalOrganizational}>Original organizational</button>}
      {onOriginalTemporal && <button className="btn-ghost" onClick={onOriginalTemporal}>Original temporal</button>}
      {onOriginalTiming && <button className="btn-ghost" onClick={onOriginalTiming}>Original timing</button>}
      {onOriginalTimeAnalysis && <button className="btn-ghost" onClick={onOriginalTimeAnalysis}>Original time analysis</button>}
      {onOriginalQuality && <button className="btn-ghost" onClick={onOriginalQuality}>Original model quality</button>}
      {onOriginalOcPlayout && <button className="btn-ghost" onClick={onOriginalOcPlayout}>Original object-centric playout</button>}
      {onOriginalFeatures && <button className="btn-ghost" onClick={onOriginalFeatures}>Original log features</button>}
      {onOriginalLabels && <button className="btn-ghost" onClick={onOriginalLabels}>Original label splitting</button>}
      {onOriginalLogQuery && <button className="btn-ghost" onClick={onOriginalLogQuery}>Original native SQL and query</button>}
      {onPromptChat && <button className="btn-ghost" onClick={onPromptChat}>Plain model prompt</button>}
      {onSavedStructures && <button className="btn-ghost" onClick={onSavedStructures}>Saved structures</button>}
      {onActiveSource && <button className="btn-ghost" onClick={onActiveSource}>Source inputs</button>}
      <label className="btn-open">
        Open log
        <input
          type="file"
          accept=".csv,.tsv,.xes,.xes.gz,.gz,.json,text/csv,application/xml,application/gzip"
          onChange={(event) => {
            const file = event.target.files?.[0];
            if (file) onOpenFile(file);
            event.target.value = "";
          }}
        />
      </label>
    </header>
  );
}
