import { useEffect, useMemo, useRef, useState } from "react";
import { EVERYDAY_TOOLS, REFERENCE_TOOLS, groupTools, searchTools, type ToolGroup, type ToolId, type ToolSpec } from "../tools";
import { BrandMark, LOG_FILE_ACCEPT, PaletteShortcut, ThemeToggle } from "./ui";

interface LandingProps {
  onOpenFile: (file: File) => void;
  onLoadSample: () => void;
  onLoadOcelSample: () => void;
  onLoadResourceSample: () => void;
  onOpenTool?: (tool: ToolId) => void;
  onOpenPalette?: () => void;
  error: string | null;
  mining: boolean;
  /** Stops the load in progress; shown as a Cancel button while mining. */
  onCancel?: () => void;
}

const SAMPLES = [
  { key: "sample", title: "Order-to-cash log", detail: "320 cases · 13 activities", text: "Explore the sample log" },
  { key: "ocel", title: "Orders and items", detail: "Object-centric", text: "Explore object-centric orders and items" },
  { key: "resource", title: "Resource handovers", detail: "Small log with resources", text: "Mine a sample with resource relationships" },
] as const;

export function Landing({ onOpenFile, onLoadSample, onLoadOcelSample, onLoadResourceSample, onOpenTool, onOpenPalette, error, mining, onCancel }: LandingProps) {
  const [query, setQuery] = useState("");
  const groups = useMemo(() => groupTools(searchTools(query, EVERYDAY_TOOLS)), [query]);
  const reference = useMemo(() => groupTools(searchTools(query, REFERENCE_TOOLS)), [query]);
  const searching = query.trim().length > 0;
  const loaders = { sample: onLoadSample, ocel: onLoadOcelSample, resource: onLoadResourceSample };
  // The Cancel button (and the disabled pickers) leave focus on the body when a
  // load stops on this page; the drop zone takes it back.
  const dropInput = useRef<HTMLInputElement>(null);
  const wasMining = useRef(mining);
  useEffect(() => {
    if (wasMining.current && !mining && (!document.activeElement || document.activeElement === document.body)) dropInput.current?.focus();
    wasMining.current = mining;
  }, [mining]);
  return (
    <div className="landing">
      <header className="landing-bar reveal">
        <span className="brand">
          <BrandMark className="brand-mark" />
          <span className="brand-name">Minera</span>
          <span className="brand-tagline">process intelligence</span>
        </span>
        <span className="landing-bar-actions">
          {onOpenPalette && (
            <button className="sidebar-search landing-search" onClick={onOpenPalette}>
              <span>Search tools</span>
              <PaletteShortcut />
            </button>
          )}
          <ThemeToggle />
        </span>
      </header>

      <section className="landing-hero">
        <div className="landing-copy reveal">
          <h1 className="landing-title">Mine your processes locally.</h1>
          <p className="landing-lede">
            Open a CSV or XES event log, an OCEL file or a Minera export. The native core
            discovers models, checks conformance and analyses performance on your machine.
          </p>
        </div>

        <label className={mining ? "landing-drop mining reveal" : "landing-drop reveal"}>
          <input
            ref={dropInput}
            type="file"
            accept={LOG_FILE_ACCEPT}
            disabled={mining}
            onChange={(event) => {
              const file = event.target.files?.[0];
              if (file) onOpenFile(file);
              event.target.value = "";
            }}
          />
          <BrandMark className="landing-drop-mark" />
          <span className="landing-drop-title">
            {mining ? "Mining event log…" : "Drop an event log here"}
          </span>
          <span className="landing-drop-sub">
            or click to browse · .csv, .xes, .xes.gz, OCEL or Minera JSON
          </span>
        </label>

        {mining && onCancel
          ? <div className="landing-error landing-status" role="status">Mining the event log… <button className="btn-ghost landing-cancel" onClick={onCancel}>Cancel</button></div>
          : error && <div className="error-banner landing-error" role="alert">{error}</div>}

        <div className="landing-samples reveal" aria-label="Sample data">
          <span className="section-label">Or start from a sample</span>
          <div className="landing-sample-grid">
            {SAMPLES.map((sample) => (
              <button key={sample.key} className="landing-sample" disabled={mining} onClick={loaders[sample.key]}>
                <span className="landing-sample-title">{sample.title}</span>
                <span className="landing-sample-detail">{sample.detail}</span>
                <span className="landing-sample-action">{sample.text} →</span>
              </button>
            ))}
          </div>
        </div>
      </section>

      {onOpenTool && (
        <section className="landing-tools reveal" aria-labelledby="landing-tools-title">
          <div className="landing-tools-head">
            <div>
              <h2 id="landing-tools-title">Tools that work without an open log</h2>
              <p>Each tool takes its own files, so you can align a log against a model, convert a model or simulate one directly.</p>
            </div>
            <input
              className="landing-tools-filter"
              type="search"
              placeholder="Filter tools"
              aria-label="Filter tools"
              value={query}
              onChange={(event) => setQuery(event.target.value)}
            />
          </div>
          <ToolGroups groups={groups} onOpenTool={onOpenTool} />
          {reference.length > 0 && (
            <details className="landing-reference" key={searching ? "search" : "browse"} open={searching || undefined}>
              <summary>
                <span className="landing-reference-title">Reference API workbenches</span>
                <span className="landing-reference-hint">One workbench per reference call, with every option exposed as JSON. Meant for parity checks and exact reproductions.</span>
              </summary>
              <ToolGroups groups={reference} onOpenTool={onOpenTool} />
            </details>
          )}
          {!groups.length && !reference.length && <p className="landing-tools-empty">No tool matches “{query}”.</p>}
        </section>
      )}

      <footer className="landing-note">
        CSV logs need <code>case_id</code>, <code>activity</code> and optionally <code>timestamp</code>,{" "}
        <code>resource</code> and <code>lifecycle</code> columns. JSON exports from <code>minera_export</code> open here too.
      </footer>
    </div>
  );
}

function ToolGroups({ groups, onOpenTool }: { groups: { group: ToolGroup; tools: ToolSpec[] }[]; onOpenTool: (tool: ToolId) => void }) {
  return (
    <>
      {groups.map(({ group, tools }) => (
        <div className="tool-group" key={group.id}>
          <div className="tool-group-head">
            <h3>{group.title}</h3>
            <span>{group.blurb}</span>
          </div>
          <div className="tool-grid">
            {tools.map((tool) => (
              <button key={tool.id} className="tool-card" onClick={() => onOpenTool(tool.id)}>
                <span className="tool-card-title">{tool.title}</span>
                <span className="tool-card-summary">{tool.summary}</span>
              </button>
            ))}
          </div>
        </div>
      ))}
    </>
  );
}
