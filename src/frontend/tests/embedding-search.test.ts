import { afterEach, describe, expect, it, vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { EmbeddingSearch } from "../src/components/EmbeddingSearch";
import { DEFAULT_EMBEDDING_SEARCH, LOCAL_EMBEDDING_MODEL, embeddingSearchOptions, embeddingSearchError } from "../src/embedding-search";
import { requestEmbeddings } from "../src/api";
import { type MineraExport } from "../src/types";

afterEach(() => vi.unstubAllGlobals());
describe("embedding search delivery", () => {
  it("uses inferred transformer width and the configured real model for search", () => {
    const options = embeddingSearchOptions({ ...DEFAULT_EMBEDDING_SEARCH, provider: "transformer", model: LOCAL_EMBEDDING_MODEL, dimensions: "0" }, " approve refund ");
    expect(options.embedding_provider).toBe("transformer");
    expect(options.embedding_dimensions).toBe(0);
    expect(options.embedding_model).toBe(LOCAL_EMBEDDING_MODEL);
    expect(options.embedding_target).toBe(" approve refund ");
    expect(options.embedding_top_k).toBe(10);
  });
  it("keeps vector generation independent of match controls and case IDs", () => {
    const options = embeddingSearchOptions({ ...DEFAULT_EMBEDDING_SEARCH, topK: "", eventId: "event:id" });
    expect(options.embedding_top_k).toBeUndefined();
    expect(options.embedding_target).toBeUndefined();
    expect(options.embedding_event_id_key).toBeUndefined();
  });
  it("forwards event text fields, explicit IDs and all-match ranking", () => {
    const options = embeddingSearchOptions({ ...DEFAULT_EMBEDDING_SEARCH, variant: "events_transformers", attribute: " description ", eventId: " event:id ", topK: "0" }, "shipping");
    expect(options.embedding_variant).toBe("events_transformers");
    expect(options.embedding_attribute_key).toBe("description");
    expect(options.embedding_event_id_key).toBe("event:id");
    expect(options.embedding_top_k).toBe(0);
  });
  it("rejects empty queries and settings outside native bounds before a request", () => {
    expect(() => embeddingSearchOptions(DEFAULT_EMBEDDING_SEARCH, " \n ")).toThrow(/Enter text/);
    for (const dimensions of ["", "0", "1.5", "4097"])
      expect(() => embeddingSearchOptions({ ...DEFAULT_EMBEDDING_SEARCH, dimensions })).toThrow(/Vector dimensions/);
    expect(() => embeddingSearchOptions({ ...DEFAULT_EMBEDDING_SEARCH, maxItems: "0" })).toThrow(/Item limit/);
    expect(() => embeddingSearchOptions({ ...DEFAULT_EMBEDDING_SEARCH, topK: "10001" }, "x")).toThrow(/Match count/);
  });
  it("disables requests from static exports instead of searching an unrelated cached log", () => {
    const data = { trace_count: 3, event_count: 9, activities: [] } as unknown as MineraExport;
    const markup = renderToStaticMarkup(createElement(EmbeddingSearch, { data, available: false, mining: false }));
    expect(markup).toContain("Open a CSV or XES log");
    expect(markup).toContain('<fieldset disabled=""');
    expect(markup).toContain("Local transformer");
    expect(markup).toContain("Build embeddings");
  });
  it("passes cancellation through to the native API and preserves provider failures", async () => {
    const fetch = vi.fn(async () => ({ ok: false, json: async () => ({ error: "transformer provider is not configured" }) }));
    vi.stubGlobal("fetch", fetch);
    const signal = new AbortController().signal;
    await expect(requestEmbeddings({ embedding_provider: "transformer", embedding_model: LOCAL_EMBEDDING_MODEL, embedding_dimensions: 0 }, signal)).rejects.toThrow(/not configured/);
    expect(fetch.mock.calls[0]).toEqual([expect.stringContaining("embedding_provider=transformer"), { method: "POST", signal }]);
  });
  it("explains local encoder rejections without replacing other backend errors", () => {
    expect(embeddingSearchError(new Error("embedding runtime: HTTP status 400"))).toContain("model name matches");
    expect(embeddingSearchError(new Error("embeddings: transformer provider requires a callback"))).toContain("running local encoder");
    expect(embeddingSearchError(new Error("no cached log"))).toBe("no cached log");
  });
});
