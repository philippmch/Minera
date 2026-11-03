import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import App from "./App";
import { ErrorBoundary } from "./components/ErrorBoundary";
import { installScrollableFocus } from "./scrollable-focus";
import "./styles.css";
import "./shell.css";

installScrollableFocus();

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <ErrorBoundary onReset={() => { window.location.hash = ""; }}>
      <App />
    </ErrorBoundary>
  </StrictMode>,
);
