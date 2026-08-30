#!/usr/bin/env node

import { execFileSync, spawnSync } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

if (process.argv.length !== 3) {
  throw new Error("usage: test-reference-parity-verifier.mjs CHECKOUT");
}

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const verifier = resolve(root, "scripts", "verify-reference-parity.mjs");
const manifestPath = resolve(root, "docs", "reference-2.7.22-coverage.json");
const checkout = resolve(process.argv[2]);

execFileSync(process.execPath, [verifier, checkout], { stdio: "pipe" });

const directory = mkdtempSync(resolve(tmpdir(), "minera-parity-"));
try {
  const manifest = JSON.parse(readFileSync(manifestPath, "utf8"));
  manifest.packages[0].variants.push("DRIFT_PROBE");
  const changed = resolve(directory, "changed.json");
  writeFileSync(changed, JSON.stringify(manifest), "utf8");
  const result = spawnSync(
    process.execPath,
    [verifier, checkout, "--manifest", changed],
    { encoding: "utf8" },
  );
  if (result.status === 0 || !result.stderr.includes("variants differ")) {
    throw new Error(`variant drift was not rejected: ${result.stderr}`);
  }
} finally {
  rmSync(directory, { recursive: true, force: true });
}

process.stdout.write("parity verifier self-test passed\n");
