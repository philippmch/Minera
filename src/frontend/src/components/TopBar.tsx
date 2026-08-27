interface TopBarProps {
  dataset: string;
  generator?: string;
  datasetKind?: string;
  onOpenFile: (file: File) => void;
  onHome: () => void;
}

export function TopBar({ dataset, generator, datasetKind = "event log", onOpenFile, onHome }: TopBarProps) {
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
      <label className="btn-open">
        Open log
        <input
          type="file"
          accept=".csv,.tsv,.json,text/csv"
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
