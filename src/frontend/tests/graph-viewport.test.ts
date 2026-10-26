import { describe, expect, it } from "vitest";
import { clientToViewBox, panViewBox, viewBoxScale, zoomViewBoxAt } from "../src/graph-viewport";

// Screen position of a view-box point under xMidYMid meet.
function toScreen(view: {x: number; y: number; w: number; h: number}, rect: {left: number; top: number; width: number; height: number}, x: number, y: number) {
  const scale = Math.max(view.w / rect.width, view.h / rect.height);
  const offsetX = (rect.width - view.w / scale) / 2, offsetY = (rect.height - view.h / scale) / 2;
  return {x: rect.left + offsetX + (x - view.x) / scale, y: rect.top + offsetY + (y - view.y) / scale};
}

const cases = {
  tall: {view: {x: 0, y: 0, w: 200, h: 1000}, rect: {left: 10, top: 20, width: 1000, height: 500}},
  wide: {view: {x: -50, y: 0, w: 2000, h: 100}, rect: {left: 0, top: 0, width: 600, height: 600}},
  square: {view: {x: 5, y: 5, w: 400, h: 400}, rect: {left: 0, top: 0, width: 800, height: 800}},
};

describe("graph viewport under preserveAspectRatio meet", () => {
  for (const [name, {view, rect}] of Object.entries(cases)) {
    it(`moves a ${name} diagram with the pointer`, () => {
      const before = toScreen(view, rect, view.x + view.w / 2, view.y + view.h / 2);
      const panned = panViewBox(view, rect, 100, -40);
      const after = toScreen(panned, rect, view.x + view.w / 2, view.y + view.h / 2);
      expect(after.x - before.x).toBeCloseTo(100, 9);
      expect(after.y - before.y).toBeCloseTo(-40, 9);
    });

    it(`keeps the point under the cursor fixed while zooming a ${name} diagram`, () => {
      const cursor = {x: rect.left + rect.width * 0.3, y: rect.top + rect.height * 0.7};
      const anchor = clientToViewBox(view, rect, cursor.x, cursor.y);
      for (const newW of [view.w * 0.5, view.w * 1.7]) {
        const zoomed = zoomViewBoxAt(view, rect, cursor.x, cursor.y, newW);
        expect(zoomed.w).toBeCloseTo(newW, 9);
        expect(zoomed.h / zoomed.w).toBeCloseTo(view.h / view.w, 9);
        const screen = toScreen(zoomed, rect, anchor.x, anchor.y);
        expect(screen.x).toBeCloseTo(cursor.x, 6);
        expect(screen.y).toBeCloseTo(cursor.y, 6);
      }
    });
  }

  it("uses the tighter axis for the scale and survives a collapsed panel", () => {
    expect(viewBoxScale(cases.tall.view, cases.tall.rect)).toBe(2);
    expect(panViewBox(cases.tall.view, cases.tall.rect, 100, 0).x).toBe(-200);
    const collapsed = {left: 0, top: 0, width: 0, height: 0};
    expect(panViewBox(cases.tall.view, collapsed, 10, 10)).toEqual({...cases.tall.view, x: -10, y: -10});
    expect(zoomViewBoxAt(cases.tall.view, collapsed, 0, 0, 100)).toEqual({x: 0, y: 0, w: 100, h: 500});
  });
});
