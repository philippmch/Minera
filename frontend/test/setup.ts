// The layout modules measure label widths through a 2D canvas context at import
// time (`document.createElement("canvas").getContext("2d")`). Node has no DOM,
// so provide a minimal deterministic stub: width grows with text length, which
// is all the layout math needs. This must run before any layout module loads,
// so it lives in a setup file rather than inside individual tests.
const ctx = {
  font: "",
  measureText: (text: string) => ({ width: text.length * 7 }),
};

(globalThis as unknown as { document: unknown }).document = {
  createElement: () => ({ getContext: () => ctx }),
};
