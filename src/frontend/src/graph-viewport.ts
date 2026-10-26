import { type ViewBox } from "./layout";

/** The on-screen box of a graph's SVG element. */
export interface ViewportRect {
  left: number;
  top: number;
  width: number;
  height: number;
}

// Graph SVGs keep the default preserveAspectRatio ("xMidYMid meet"): the view
// box is scaled uniformly by the tighter axis and centred, leaving unused bands
// on the other axis. Pointer math must go through that fit, not through the
// element width alone, or a tall diagram in a wide panel drags far too slowly.
function fit(view: ViewBox, rect: ViewportRect) {
  const scale = Math.max(view.w / rect.width, view.h / rect.height);
  return {
    scale,
    offsetX: (rect.width - view.w / scale) / 2,
    offsetY: (rect.height - view.h / scale) / 2,
  };
}

function usable(view: ViewBox, rect: ViewportRect) {
  return rect.width > 0 && rect.height > 0 && view.w > 0 && view.h > 0;
}

/** View-box units per screen pixel. */
export function viewBoxScale(view: ViewBox, rect: ViewportRect): number {
  return usable(view, rect) ? fit(view, rect).scale : 1;
}

/** The view-box point drawn under a client (screen) position. */
export function clientToViewBox(
  view: ViewBox, rect: ViewportRect, clientX: number, clientY: number,
): { x: number; y: number } {
  if (!usable(view, rect)) return { x: view.x, y: view.y };
  const { scale, offsetX, offsetY } = fit(view, rect);
  return {
    x: view.x + (clientX - rect.left - offsetX) * scale,
    y: view.y + (clientY - rect.top - offsetY) * scale,
  };
}

/** Moves the drawing with a pointer drag of (dx, dy) screen pixels. */
export function panViewBox(view: ViewBox, rect: ViewportRect, dx: number, dy: number): ViewBox {
  const scale = viewBoxScale(view, rect);
  return { ...view, x: view.x - dx * scale, y: view.y - dy * scale };
}

/**
 * Resizes the view box to width `newW` (keeping its aspect ratio) so that the
 * point under the cursor stays under the cursor.
 */
export function zoomViewBoxAt(
  view: ViewBox, rect: ViewportRect, clientX: number, clientY: number, newW: number,
): ViewBox {
  const newH = view.h * (newW / view.w);
  if (!usable(view, rect)) return { ...view, w: newW, h: newH };
  const anchor = clientToViewBox(view, rect, clientX, clientY);
  const next = { x: 0, y: 0, w: newW, h: newH };
  const { scale, offsetX, offsetY } = fit(next, rect);
  next.x = anchor.x - (clientX - rect.left - offsetX) * scale;
  next.y = anchor.y - (clientY - rect.top - offsetY) * scale;
  return next;
}
