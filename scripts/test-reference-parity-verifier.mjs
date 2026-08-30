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

  const artifactManifest = JSON.parse(readFileSync(manifestPath, "utf8"));
  const complete = artifactManifest.packages.find((entry) => entry.status === "complete");
  complete.artifacts = ["src/backend/does-not-exist.cpp"];
  const missingArtifact = resolve(directory, "missing-artifact.json");
  writeFileSync(missingArtifact, JSON.stringify(artifactManifest), "utf8");
  const artifactResult = spawnSync(
    process.execPath,
    [verifier, checkout, "--manifest", missingArtifact],
    { encoding: "utf8" },
  );
  if (artifactResult.status === 0 || !artifactResult.stderr.includes("artifact does not exist")) {
    throw new Error(`missing artifact was not rejected: ${artifactResult.stderr}`);
  }

  const requiredManifest = JSON.parse(readFileSync(manifestPath, "utf8"));
  const requiredPackage = requiredManifest.artifact_required_packages[0];
  delete requiredManifest.packages.find((entry) => entry.package === requiredPackage).artifacts;
  const absentArtifacts = resolve(directory, "absent-artifacts.json");
  writeFileSync(absentArtifacts, JSON.stringify(requiredManifest), "utf8");
  const absentResult = spawnSync(
    process.execPath,
    [verifier, checkout, "--manifest", absentArtifacts],
    { encoding: "utf8" },
  );
  if (absentResult.status === 0 || !absentResult.stderr.includes("requires an artifact list")) {
    throw new Error(`required artifact list was not enforced: ${absentResult.stderr}`);
  }
} finally {
  rmSync(directory, { recursive: true, force: true });
}

process.stdout.write("parity verifier self-test passed\n");
