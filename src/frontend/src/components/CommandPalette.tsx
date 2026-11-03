import { useEffect, useId, useMemo, useRef, useState } from "react";
import { matchesQuery, toolGroup, TOOLS, VIEW_GROUPS, type ViewSpec } from "../tools";

export interface PaletteCommand {
  id: string;
  title: string;
  section: string;
  detail?: string;
  keywords?: string;
  run: () => void;
}

/**
 * Commands for the palette: extra commands, log views, every tool, then the
 * trailing commands. Commands that discard work (such as going to the start
 * page) belong in `trailing`, so an empty query never preselects them.
 */
export function paletteCommands({ views, onSelectView, onOpenTool, extra = [], trailing = [] }: { views: readonly ViewSpec[]; onSelectView: (view: ViewSpec["id"]) => void; onOpenTool: (tool: (typeof TOOLS)[number]["id"]) => void; extra?: PaletteCommand[]; trailing?: PaletteCommand[] }): PaletteCommand[] {
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
    section: tool.reference ? `Reference API · ${toolGroup(tool.group).title}` : toolGroup(tool.group).title,
    detail: tool.summary,
    keywords: tool.keywords,
    run: () => onOpenTool(tool.id),
  }));
  return [...extra, ...viewCommands, ...toolCommands, ...trailing];
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
  const baseId = useId();
  const optionId = (index: number) => `${baseId}-option-${index}`;
  // Focus the search on open and hand it back to the opener on close.
  useEffect(() => {
    const previous = document.activeElement instanceof HTMLElement ? document.activeElement : null;
    input.current?.focus();
    return () => { if (previous?.isConnected && previous !== document.body) previous.focus(); };
  }, []);
  useEffect(() => { setActive(0); }, [query]);
  useEffect(() => {
    list.current?.querySelector<HTMLElement>(`[data-index="${active}"]`)?.scrollIntoView?.({ block: "nearest" });
  }, [active]);
  const run = (command: PaletteCommand | undefined) => {
    if (!command) return;
    onClose();
    command.run();
  };
  const selected = results.length ? Math.min(Math.max(active, 0), results.length - 1) : -1;
  return (
    <div className="palette-veil" onMouseDown={(event) => { if (event.target === event.currentTarget) onClose(); }}>
      <div
        className="palette"
        role="dialog"
        aria-modal="true"
        aria-label="Search views and tools"
        // A click on the dialog's own text or padding keeps focus in the
        // search field, so the keys below still reach the dialog.
        onMouseDown={(event) => { if (event.target !== input.current) event.preventDefault(); }}
        onKeyDown={(event) => {
          // The search field keeps focus (a combobox), so Escape and Tab are handled for the whole dialog.
          if (event.key === "Escape") { event.preventDefault(); event.stopPropagation(); onClose(); }
          else if (event.key === "Tab") { event.preventDefault(); input.current?.focus(); }
        }}
      >
        <input
          ref={input}
          className="palette-input"
          placeholder="Search views and tools…"
          aria-label="Search views and tools"
          role="combobox"
          aria-expanded="true"
          aria-autocomplete="list"
          aria-controls={`${baseId}-list`}
          aria-activedescendant={selected >= 0 ? optionId(selected) : undefined}
          value={query}
          onChange={(event) => setQuery(event.target.value)}
          onKeyDown={(event) => {
            if (event.key === "ArrowDown") { event.preventDefault(); setActive((index) => Math.min(index + 1, Math.max(results.length - 1, 0))); }
            else if (event.key === "ArrowUp") { event.preventDefault(); setActive((index) => Math.max(index - 1, 0)); }
            else if (event.key === "Enter") { event.preventDefault(); run(results[selected]); }
          }}
        />
        <ul className="palette-list" ref={list} role="listbox" id={`${baseId}-list`} aria-label="Matching views and tools">
          {results.map((command, index) => (
            <li
              key={command.id}
              id={optionId(index)}
              role="option"
              aria-selected={index === selected}
              data-index={index}
              className={index === selected ? "palette-item active" : "palette-item"}
              onMouseMove={() => setActive(index)}
              onMouseDown={(event) => event.preventDefault()}
              onClick={() => run(command)}
            >
              <span className="palette-item-title">{command.title}</span>
              <span className="palette-item-section">{command.section}</span>
              {command.detail && <span className="palette-item-detail">{command.detail}</span>}
            </li>
          ))}
          {!results.length && <li className="palette-empty" role="presentation">Nothing matches “{query}”.</li>}
        </ul>
        <div className="palette-hint" aria-hidden="true"><kbd>↑</kbd><kbd>↓</kbd> to move · <kbd>Enter</kbd> to open · <kbd>Esc</kbd> to close</div>
      </div>
    </div>
  );
}
