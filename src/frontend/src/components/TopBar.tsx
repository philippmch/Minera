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
  onActiveSource?: () => void;
}

export function TopBar({ dataset, generator, datasetKind = "event log", onOpenFile, onHome, onImportOcel, onRelateLogs, onStreaming, onModelExchange, onProcessCube, onTreeGeneration, onAcquisition, onTokenReplay, onPetriAlignment, onWorkflowInputs, onGraphAlignment, onInductiveInputs, onTreeAlignment, onPetriAntiMulti, onModelConversion, onMonteCarlo, onPlayout, onOriginalDecision, onActiveSource }: TopBarProps) {
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
