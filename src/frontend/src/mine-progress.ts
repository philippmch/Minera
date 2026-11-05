import { useEffect, useState } from "react";
import { backendFetch } from "./backend-fetch";

/**
 * Progress of the backend's running mine (GET /api/mine-progress), polled
 * while this tab mines. The backend answers from its own record, never the
 * mining slot, so a poll is answered at once while the mine runs.
 */
export interface MineProgress {
  /** Whether the backend is mining at all. */
  running: boolean;
  /** Whether the running mine is the request this tab asked about. */
  requestMatches?: boolean;
  stage?: string;
  /** 1-based index of the current stage. */
  stageIndex?: number;
  stagesDone?: number;
  /** null when the backend does not know its stage count up front. */
  stagesTotal?: number | null;
  elapsedSeconds?: number;
  stageElapsedSeconds?: number;
  /** The running mine was canceled and is stopping. */
  canceling?: boolean;
}

/** How often the mining overlay asks for progress. */
export const MINE_PROGRESS_INTERVAL_MS = 1000;

const finite = (value: unknown) => typeof value === "number" && Number.isFinite(value) && value >= 0 ? value : undefined;

/** The MINE_PROGRESS answer as a MineProgress, or null for anything else. */
export function parseMineProgress(value: unknown): MineProgress | null {
  if (value === null || typeof value !== "object") return null;
  const body = value as Record<string, unknown>;
  if (body.family !== "MINE_PROGRESS" || typeof body.running !== "boolean") return null;
  if (!body.running) return {running: false};
  const total = body.stages_total === null ? null : finite(body.stages_total);
  return {
    running: true,
    ...(typeof body.request_matches === "boolean" ? {requestMatches: body.request_matches} : {}),
    ...(typeof body.stage === "string" ? {stage: body.stage} : {}),
    ...(finite(body.stage_index) !== undefined ? {stageIndex: finite(body.stage_index)} : {}),
    ...(finite(body.stages_done) !== undefined ? {stagesDone: finite(body.stages_done)} : {}),
    ...(total !== undefined ? {stagesTotal: total} : {}),
    ...(finite(body.elapsed_seconds) !== undefined ? {elapsedSeconds: finite(body.elapsed_seconds)} : {}),
    ...(finite(body.stage_elapsed_seconds) !== undefined ? {stageElapsedSeconds: finite(body.stage_elapsed_seconds)} : {}),
    ...(typeof body.canceling === "boolean" ? {canceling: body.canceling} : {}),
  };
}

/** One poll; null when the backend cannot say (an older backend, a network failure). */
export async function fetchMineProgress(requestId: string, signal?: AbortSignal): Promise<MineProgress | null> {
  try {
    const response = await backendFetch(`/api/mine-progress?request_id=${encodeURIComponent(requestId)}`, {signal, cache: "no-store"});
    if (!response.ok) return null;
    return parseMineProgress(await response.json());
  } catch {
    return null;
  }
}

/** Elapsed time as m:ss, or h:mm:ss from an hour on. */
export function formatElapsed(seconds: number): string {
  const whole = Math.max(0, Math.floor(seconds));
  const h = Math.floor(whole / 3600), m = Math.floor(whole / 60) % 60, s = whole % 60;
  const ss = String(s).padStart(2, "0");
  return h > 0 ? `${h}:${String(m).padStart(2, "0")}:${ss}` : `${m}:${ss}`;
}

/**
 * The overlay's line for a poll of this tab's mine, e.g.
 * "Inductive net: reachability · stage 13 of 26 · 1:05 elapsed"; null while
 * there is nothing of this mine to show (not started yet, another client's
 * mine, or no answer).
 */
export function mineProgressText(progress: MineProgress | null): string | null {
  if (!progress?.running || progress.requestMatches === false || !progress.stage) return null;
  const parts = [progress.stage];
  if (progress.stageIndex !== undefined)
    parts.push(progress.stagesTotal ? `stage ${progress.stageIndex} of ${progress.stagesTotal}` : `stage ${progress.stageIndex}`);
  if (progress.elapsedSeconds !== undefined) parts.push(`${formatElapsed(progress.elapsedSeconds)} elapsed`);
  return parts.join(" · ");
}

/**
 * Polls the progress of mine `requestId` every `intervalMs` (one request at a
 * time) and hands each answer to `onProgress`. Returns the stop function,
 * which also aborts a poll in flight.
 */
export function watchMineProgress(requestId: string, onProgress: (progress: MineProgress | null) => void,
                                  intervalMs = MINE_PROGRESS_INTERVAL_MS): () => void {
  const controller = new AbortController();
  let timer: ReturnType<typeof setTimeout> | undefined;
  const poll = async () => {
    const progress = await fetchMineProgress(requestId, controller.signal);
    if (controller.signal.aborted) return;
    onProgress(progress);
    timer = setTimeout(() => void poll(), intervalMs);
  };
  timer = setTimeout(() => void poll(), 0);
  return () => {controller.abort(); clearTimeout(timer);};
}

/** The latest progress of mine `requestId` while it is set; null otherwise. */
export function useMineProgress(requestId: string | null): MineProgress | null {
  const [progress, setProgress] = useState<MineProgress | null>(null);
  useEffect(() => {
    setProgress(null);
    if (!requestId) return undefined;
    return watchMineProgress(requestId, setProgress);
  }, [requestId]);
  return progress;
}
