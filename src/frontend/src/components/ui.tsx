import { useEffect, useState, type ReactNode } from "react";

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
  try {
    const value = typeof localStorage === "undefined" ? null : localStorage.getItem(THEME_KEY);
    return value === "light" || value === "dark" ? value : "system";
  } catch {
    return "system";
  }
}

/** Cycles system → light → dark and remembers the choice in this browser. */
export function ThemeToggle() {
  const [theme, setTheme] = useState<ThemeChoice>(storedTheme);
  useEffect(() => {
    const root = document.documentElement;
    if (theme === "system") delete root.dataset.theme; else root.dataset.theme = theme;
    try { if (theme === "system") localStorage.removeItem(THEME_KEY); else localStorage.setItem(THEME_KEY, theme); } catch { /* storage unavailable */ }
  }, [theme]);
  const next = THEME_ORDER[(THEME_ORDER.indexOf(theme) + 1) % THEME_ORDER.length];
  return (
    <button className="theme-toggle" onClick={() => setTheme(next)} title={`Theme: ${theme}. Switch to ${next}.`} aria-label={`Theme: ${theme}. Switch to ${next}.`}>
      {theme === "system" ? "Auto" : theme === "light" ? "Light" : "Dark"}
    </button>
  );
}
