import { useMemo, useState } from "react";
import type { Tab } from "../app-logic";
import { EVERYDAY_TOOLS, REFERENCE_TOOLS, VIEW_GROUPS, groupTools, searchTools, type ToolId, type ViewSpec } from "../tools";
import { BrandMark, FileButton, ThemeToggle } from "./ui";

export interface SidebarDataset {
  kind: string;
  name: string;
  detail?: string;
}

interface SidebarProps {
  dataset: SidebarDataset | null;
  views?: readonly ViewSpec[];
  activeView?: Tab | null;
  onSelectView?: (view: Tab) => void;
  /** A single workspace entry for data without log views, such as an OCEL log. */
  workspace?: { title: string; active: boolean; onSelect: () => void };
  activeTool: ToolId | null;
  onOpenTool: (tool: ToolId) => void;
  onHome: () => void;
  onOpenFile: (file: File) => void;
  onOpenPalette: () => void;
  mining?: boolean;
  open?: boolean;
  onClose?: () => void;
}

export function Sidebar({ dataset, views, activeView, onSelectView, workspace, activeTool, onOpenTool, onHome, onOpenFile, onOpenPalette, mining = false, open = false, onClose }: SidebarProps) {
  const [query, setQuery] = useState("");
  const groups = useMemo(() => groupTools(searchTools(query, EVERYDAY_TOOLS)), [query]);
  const reference = useMemo(() => groupTools(searchTools(query, REFERENCE_TOOLS)), [query]);
  const searching = query.trim().length > 0;
  return (
    <aside className={open ? "sidebar open" : "sidebar"} aria-label="Navigation">
      <div className="sidebar-head">
        <button className="brand" onClick={onHome} title="Back to start">
          <BrandMark className="brand-mark" />
          <span className="brand-name">Minera</span>
        </button>
        {onClose && <button className="sidebar-close" onClick={onClose} aria-label="Close navigation">×</button>}
      </div>

      <section className="sidebar-dataset" aria-label="Open data">
        <span className="sidebar-label">{dataset ? dataset.kind : "No log open"}</span>
        {dataset && <span className="sidebar-dataset-name" title={dataset.name}>{dataset.name}</span>}
        {dataset?.detail && <span className="sidebar-dataset-detail">{dataset.detail}</span>}
        <FileButton className="btn-open sidebar-open" disabled={mining} onFile={onOpenFile}>
          {mining ? "Loading…" : dataset ? "Open another log" : "Open log"}
        </FileButton>
      </section>

      <button className="sidebar-search" onClick={onOpenPalette}>
        <span>Search views and tools</span>
        <kbd>⌘K</kbd>
      </button>

      <div className="sidebar-scroll">
        {workspace && (
          <nav className="sidebar-nav" aria-label="Workspace">
            <button className={workspace.active ? "sidebar-item active" : "sidebar-item"} aria-current={workspace.active ? "page" : undefined} onClick={workspace.onSelect}>
              {workspace.title}
            </button>
          </nav>
        )}
        {views && views.length > 0 && (
          <nav className="sidebar-nav" aria-label="Log views">
            {VIEW_GROUPS.map((group) => {
              const items = views.filter((view) => view.group === group.id);
              if (!items.length) return null;
              return (
                <div className="sidebar-group" key={group.id}>
                  <span className="sidebar-label">{group.title}</span>
                  {items.map((view) => (
                    <button
                      key={view.id}
                      className={activeTool === null && activeView === view.id ? "sidebar-item active" : "sidebar-item"}
                      aria-current={activeTool === null && activeView === view.id ? "page" : undefined}
                      onClick={() => onSelectView?.(view.id)}
                    >
                      {view.title}
                    </button>
                  ))}
                </div>
              );
            })}
          </nav>
        )}

        <nav className="sidebar-nav sidebar-tools" aria-label="Tools">
          <div className="sidebar-tools-head">
            <span className="sidebar-label">Tools</span>
            <input
              className="sidebar-filter"
              type="search"
              placeholder="Filter tools"
              aria-label="Filter tools"
              value={query}
              onChange={(event) => setQuery(event.target.value)}
            />
          </div>
          {groups.map(({ group, tools }) => (
            <details
              className="sidebar-tool-group"
              key={group.id + (searching ? ":search" : "")}
              open={searching || tools.some((tool) => tool.id === activeTool) || undefined}
            >
              <summary>{group.title}<span className="sidebar-count">{tools.length}</span></summary>
              {tools.map((tool) => (
                <button
                  key={tool.id}
                  className={activeTool === tool.id ? "sidebar-item active" : "sidebar-item"}
                  aria-current={activeTool === tool.id ? "page" : undefined}
                  title={tool.summary}
                  onClick={() => onOpenTool(tool.id)}
                >
                  {tool.title}
                </button>
              ))}
            </details>
          ))}
          {reference.length > 0 && (
            <details
              className="sidebar-tool-group sidebar-reference"
              key={searching ? "reference:search" : "reference"}
              open={searching || REFERENCE_TOOLS.some((tool) => tool.id === activeTool) || undefined}
            >
              <summary>Reference API<span className="sidebar-count">{reference.reduce((sum, entry) => sum + entry.tools.length, 0)}</span></summary>
              {reference.map(({ group, tools }) => (
                <div className="sidebar-reference-group" key={group.id}>
                  <span className="sidebar-label">{group.title}</span>
                  {tools.map((tool) => (
                    <button
                      key={tool.id}
                      className={activeTool === tool.id ? "sidebar-item active" : "sidebar-item"}
                      aria-current={activeTool === tool.id ? "page" : undefined}
                      title={tool.summary}
                      onClick={() => onOpenTool(tool.id)}
                    >
                      {tool.title}
                    </button>
                  ))}
                </div>
              ))}
            </details>
          )}
          {!groups.length && !reference.length && <p className="sidebar-empty">No tool matches “{query}”.</p>}
        </nav>
      </div>
      <div className="sidebar-foot">
        <span className="sidebar-label">Theme</span>
        <ThemeToggle />
      </div>
    </aside>
  );
}
