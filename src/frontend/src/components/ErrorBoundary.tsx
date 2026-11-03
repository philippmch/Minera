import { Component, type ErrorInfo, type ReactNode } from "react";

interface ErrorBoundaryState {
  error: Error | null;
}

/**
 * Catches a render error anywhere below it and shows a recoverable message
 * instead of a blank page.
 *
 * - scope "view" (inside App, around the open view or tool): App and its open
 *   data stay mounted, so "Try again" draws the view again with the same data,
 *   and the sidebar still leads to every other view. A change of resetKey
 *   (App passes the route) clears the error, so choosing another view draws
 *   it normally.
 * - scope "app" (the top-level fallback in main.tsx): App itself failed and
 *   React has already unmounted it, so its data is gone; the actions say so.
 */
export class ErrorBoundary extends Component<{ children: ReactNode; onReset?: () => void; scope?: "view" | "app"; resetKey?: string }, ErrorBoundaryState> {
  state: ErrorBoundaryState = { error: null };

  static getDerivedStateFromError(error: unknown): ErrorBoundaryState {
    return { error: error instanceof Error ? error : new Error(String(error)) };
  }

  componentDidCatch(error: Error, info: ErrorInfo): void {
    console.error(this.props.scope === "view" ? "Minera view failed to render" : "Minera failed to render", error, info.componentStack);
  }

  componentDidUpdate(previous: { resetKey?: string }): void {
    if (this.state.error && previous.resetKey !== this.props.resetKey) this.setState({ error: null });
  }

  private reset = () => {
    this.props.onReset?.();
    this.setState({ error: null });
  };

  render(): ReactNode {
    const { error } = this.state;
    if (!error) return this.props.children;
    if (this.props.scope === "view") {
      return (
        <div className="app-crash app-crash-view" role="alert">
          <h2>Something went wrong in this view</h2>
          <p>Minera hit an unexpected error while drawing this view. The open data is kept: try again, or choose another view or tool from the sidebar.</p>
          <pre>{error.message}</pre>
          <div className="app-crash-actions">
            <button className="btn-open" onClick={this.reset}>Try again</button>
          </div>
        </div>
      );
    }
    return (
      <div className="app-crash" role="alert">
        <h1>Something went wrong</h1>
        <p>Minera hit an unexpected error while drawing the page, and the open data in this tab was lost. Your files were not changed.</p>
        <pre>{error.message}</pre>
        <div className="app-crash-actions">
          <button className="btn-open" onClick={this.reset}>Back to the start page</button>
          <button className="btn-ghost" onClick={() => { window.location.hash = ""; window.location.reload(); }}>Reload Minera</button>
        </div>
      </div>
    );
  }
}
