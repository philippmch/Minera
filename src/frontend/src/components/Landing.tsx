interface LandingProps {
  onOpenFile: (file: File) => void;
  onLoadSample: () => void;
  error: string | null;
  mining: boolean;
}

export function Landing({ onOpenFile, onLoadSample, error, mining }: LandingProps) {
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
          Local process mining for CSV/XES event logs and object-centric JSON.
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

        <button className="landing-sample reveal" onClick={onLoadSample}>
          Explore the sample log instead →
        </button>

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
