import { defineConfig } from "vitest/config";

// Unit tests for the pure data→geometry layer (layout algorithms and helpers).
// They run under Node; the layout modules touch a canvas for text measurement,
// which test/setup.ts stubs deterministically.
export default defineConfig({
  test: {
    environment: "node",
    setupFiles: ["./test/setup.ts"],
    include: ["tests/**/*.test.ts"],
  },
});
