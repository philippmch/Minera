import { useEffect, useState, type ReactNode } from "react";
import { readStored, removeStored, writeStored } from "../storage";

/** File types the main "Open log" entry points accept. */
export const LOG_FILE_ACCEPT = ".csv,.tsv,.xes,.xes.gz,.gz,.json,text/csv,application/xml,application/gzip";

export function BrandMark({ className }: { className?: string }) {
  return (
    <svg className={className} viewBox="0 0 24 24" aria-hidden="true">
      <path d="M12 2 22 12 12 22 2 12Z" />
      <path className="brand-mark-core" d="M12 8 16 12 12 16 8 12Z" />
    </svg>
  );
}

/** A button-styled label around a hidden file input. */
export function FileButton({ className, disabled, accept = LOG_FILE_ACCEPT, onFile, children }: { className?: string; disabled?: boolean; accept?: string; onFile: (file: File) => void; children: ReactNode }) {
  return (
    <label className={disabled ? `${className ?? ""} disabled` : className}>
      {children}
      <input
        type="file"
        accept={accept}
        disabled={disabled}
        onChange={(event) => {
          const file = event.target.files?.[0];
          if (file) onFile(file);
          event.target.value = "";
        }}
      />
    </label>
  );
}

export function PageHeader({ eyebrow, title, summary, actions }: { eyebrow?: string; title: string; summary?: string; actions?: ReactNode }) {
  return (
    <header className="page-header">
      <div className="page-heading">
        {eyebrow && <span className="page-eyebrow">{eyebrow}</span>}
        <h1 className="page-title">{title}</h1>
        {summary && <p className="page-summary">{summary}</p>}
      </div>
      {actions && <div className="page-actions">{actions}</div>}
    </header>
  );
}

/** A collapsed section for settings and secondary results below a page's main content. */
export function Advanced({ title, hint, className, defaultOpen, children }: { title: string; hint?: string; className?: string; defaultOpen?: boolean; children: ReactNode }) {
  return (
    <details className={className ? `advanced ${className}` : "advanced"} open={defaultOpen || undefined}>
      <summary>
        <span className="advanced-title">{title}</span>
        {hint && <span className="advanced-hint">{hint}</span>}
      </summary>
      <div className="advanced-body">{children}</div>
    </details>
  );
}

export type ThemeChoice = "system" | "light" | "dark";
const THEME_KEY = "minera-theme";
const THEME_ORDER: ThemeChoice[] = ["system", "light", "dark"];

function storedTheme(): ThemeChoice {
  const value = readStored(THEME_KEY);
  return value === "light" || value === "dark" ? value : "system";
}

/** Cycles system → light → dark and remembers the choice in this browser. */
export function ThemeToggle() {
  const [theme, setTheme] = useState<ThemeChoice>(storedTheme);
  useEffect(() => {
    const root = document.documentElement;
    if (theme === "system") delete root.dataset.theme; else root.dataset.theme = theme;
    if (theme === "system") removeStored(THEME_KEY); else writeStored(THEME_KEY, theme);
  }, [theme]);
  const next = THEME_ORDER[(THEME_ORDER.indexOf(theme) + 1) % THEME_ORDER.length];
  const label = THEME_LABELS[theme];
  // The accessible name starts with the visible text so voice control ("click Auto") works.
  return (
    <button className="theme-toggle" onClick={() => setTheme(next)} title={`${label} theme. Switch to ${THEME_LABELS[next]}.`} aria-label={`${label} theme. Switch to ${THEME_LABELS[next]}.`}>
      {label}
    </button>
  );
}

const THEME_LABELS: Record<ThemeChoice, string> = { system: "Auto", light: "Light", dark: "Dark" };

/** Whether the viewer is on an Apple platform, where shortcuts use ⌘ rather than Ctrl. */
export function isApplePlatform(nav: { platform?: string; userAgent?: string; userAgentData?: { platform?: string } } | undefined = typeof navigator === "undefined" ? undefined : navigator as never): boolean {
  if (!nav) return false;
  const platform = nav.userAgentData?.platform || nav.platform || nav.userAgent || "";
  return /mac|iphone|ipad|ipod/i.test(platform);
}

/** The palette shortcut as the viewer's platform writes it. */
export function paletteShortcutLabel(apple = isApplePlatform()): string {
  return apple ? "⌘K" : "Ctrl K";
}

/** The palette shortcut hint shown on the search buttons. */
export function PaletteShortcut() {
  const label = paletteShortcutLabel();
  return <kbd aria-label={label === "⌘K" ? "Command K" : "Control K"}>{label}</kbd>;
}
