import {afterEach, describe, expect, it, vi} from "vitest";
import {MAX_UPLOAD_BYTES, mineActiveDataset, uploadTooLargeMessage} from "../src/api";

afterEach(() => vi.unstubAllGlobals());

describe("upload size limit", () => {
  it("matches the 128 MiB server limit and suggests gzip", () => {
    expect(MAX_UPLOAD_BYTES).toBe(128 * 1024 * 1024);
    expect(uploadTooLargeMessage()).toContain("128 MiB");
    expect(uploadTooLargeMessage()).toContain(".xes.gz");
    expect(uploadTooLargeMessage(200 * 1024 * 1024)).toContain("(200.0 MiB)");
  });

  it("refuses an oversized file before uploading it", async () => {
    const fetch = vi.fn();
    vi.stubGlobal("fetch", fetch);
    const file = new File(["x"], "big.xes");
    Object.defineProperty(file, "size", {value: MAX_UPLOAD_BYTES + 1});
    await expect(mineActiveDataset(file)).rejects.toThrow("128 MiB limit");
    expect(fetch).not.toHaveBeenCalled();
  });

  it("names the limit when the server answers 413", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response("", {status: 413})));
    await expect(mineActiveDataset(new File(["x"], "log.xes"))).rejects.toThrow("128 MiB limit");
  });
});
