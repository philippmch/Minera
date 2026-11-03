import { Component, type ErrorInfo, type ReactNode } from "react";

interface ErrorBoundaryState {
  error: Error | null;
}

/**
 * Catches a render error anywhere below it, so one broken view shows a
 * recoverable message instead of unmounting the whole app to a blank page.
 */
export class ErrorBoundary extends Component<{ children: ReactNode; onReset?: () => void }, ErrorBoundaryState> {
  state: ErrorBoundaryState = { error: null };

  static getDerivedStateFromError(error: unknown): ErrorBoundaryState {
    return { error: error instanceof Error ? error : new Error(String(error)) };
  }

  componentDidCatch(error: Error, info: ErrorInfo): void {
    console.error("Minera view failed to render", error, info.componentStack);
  }

  private reset = () => {
    this.props.onReset?.();
    this.setState({ error: null });
  };

  render(): ReactNode {
    const { error } = this.state;
    if (!error) return this.props.children;
    return (
      <div className="app-crash" role="alert">
        <h1>Something went wrong in this view</h1>
        <p>Minera hit an unexpected error while drawing the page. Your files were not changed.</p>
        <pre>{error.message}</pre>
        <div className="app-crash-actions">
          <button className="btn-open" onClick={this.reset}>Try again</button>
          <button className="btn-ghost" onClick={() => { window.location.hash = ""; window.location.reload(); }}>Reload Minera</button>
        </div>
      </div>
    );
  }
}
