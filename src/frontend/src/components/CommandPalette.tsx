import { useEffect, useMemo, useRef, useState } from "react";
import { matchesQuery, toolGroup, TOOLS, VIEW_GROUPS, type ViewSpec } from "../tools";

export interface PaletteCommand {
  id: string;
  title: string;
  section: string;
  detail?: string;
  keywords?: string;
  run: () => void;
}

/** Commands for the palette: log views first, then every tool. */
export function paletteCommands({ views, onSelectView, onOpenTool, extra = [] }: { views: readonly ViewSpec[]; onSelectView: (view: ViewSpec["id"]) => void; onOpenTool: (tool: (typeof TOOLS)[number]["id"]) => void; extra?: PaletteCommand[] }): PaletteCommand[] {
  const viewCommands = views.map((view) => ({
    id: `view:${view.id}`,
    title: view.title,
    section: `Log · ${VIEW_GROUPS.find((group) => group.id === view.group)?.title ?? ""}`,
    detail: view.summary,
    run: () => onSelectView(view.id),
  }));
  const toolCommands = TOOLS.map((tool) => ({
    id: `tool:${tool.id}`,
    title: tool.title,
    section: toolGroup(tool.group).title,
    detail: tool.summary,
    keywords: tool.keywords,
    run: () => onOpenTool(tool.id),
  }));
  return [...extra, ...viewCommands, ...toolCommands];
}

export function filterCommands(commands: readonly PaletteCommand[], query: string): PaletteCommand[] {
  return commands.filter((command) => matchesQuery(query, command.title, command.section, command.detail, command.keywords));
}

export function CommandPalette({ commands, onClose }: { commands: readonly PaletteCommand[]; onClose: () => void }) {
  const [query, setQuery] = useState("");
  const [active, setActive] = useState(0);
  const input = useRef<HTMLInputElement | null>(null);
  const list = useRef<HTMLUListElement | null>(null);
  const results = useMemo(() => filterCommands(commands, query), [commands, query]);
  useEffect(() => { input.current?.focus(); }, []);
  useEffect(() => { setActive(0); }, [query]);
  useEffect(() => {
    list.current?.querySelector<HTMLElement>(`[data-index="${active}"]`)?.scrollIntoView({ block: "nearest" });
  }, [active]);
  const run = (command: PaletteCommand | undefined) => {
    if (!command) return;
    onClose();
    command.run();
  };
  return (
    <div className="palette-veil" onMouseDown={(event) => { if (event.target === event.currentTarget) onClose(); }}>
      <div className="palette" role="dialog" aria-modal="true" aria-label="Search views and tools">
        <input
          ref={input}
          className="palette-input"
          placeholder="Search views and tools…"
          aria-label="Search views and tools"
          value={query}
          onChange={(event) => setQuery(event.target.value)}
          onKeyDown={(event) => {
            if (event.key === "ArrowDown") { event.preventDefault(); setActive((index) => Math.min(index + 1, results.length - 1)); }
            else if (event.key === "ArrowUp") { event.preventDefault(); setActive((index) => Math.max(index - 1, 0)); }
            else if (event.key === "Enter") { event.preventDefault(); run(results[active]); }
            else if (event.key === "Escape") { event.preventDefault(); onClose(); }
          }}
        />
        <ul className="palette-list" ref={list} role="listbox">
          {results.map((command, index) => (
            <li key={command.id} role="option" aria-selected={index === active}>
              <button
                data-index={index}
                className={index === active ? "palette-item active" : "palette-item"}
                onMouseMove={() => setActive(index)}
                onClick={() => run(command)}
              >
                <span className="palette-item-title">{command.title}</span>
                <span className="palette-item-section">{command.section}</span>
                {command.detail && <span className="palette-item-detail">{command.detail}</span>}
              </button>
            </li>
          ))}
          {!results.length && <li className="palette-empty">Nothing matches “{query}”.</li>}
        </ul>
        <div className="palette-hint"><kbd>↑</kbd><kbd>↓</kbd> to move · <kbd>Enter</kbd> to open · <kbd>Esc</kbd> to close</div>
      </div>
    </div>
  );
}
