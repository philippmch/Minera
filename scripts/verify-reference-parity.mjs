#!/usr/bin/env node

import { execFileSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
let manifestPath = resolve(root, "docs", "reference-2.7.22-coverage.json");
const validStatuses = new Set(["complete", "partial", "missing", "adapter", "integration"]);

function fail(message) {
  process.stderr.write(`parity verification failed: ${message}\n`);
  process.exitCode = 1;
}

function key(entry) {
  return `${entry.package}\0${entry.variants.join("\0")}`;
}

if (process.argv.length !== 3 && process.argv.length !== 5) {
  throw new Error("usage: verify-reference-parity.mjs CHECKOUT [--manifest FILE]");
}

const checkout = resolve(process.argv[2]);
if (process.argv.length === 5) {
  if (process.argv[3] !== "--manifest") throw new Error(`unknown argument: ${process.argv[3]}`);
  manifestPath = resolve(process.argv[4]);
}
const manifest = JSON.parse(readFileSync(manifestPath, "utf8"));
const catalog = JSON.parse(execFileSync(
  process.execPath,
  [resolve(root, "scripts", "audit-reference-catalog.mjs"), checkout],
  { encoding: "utf8" },
));

if (manifest.target?.tag !== catalog.tag) fail(`target tag is ${manifest.target?.tag}, catalog is ${catalog.tag}`);
if (manifest.target?.commit !== catalog.commit) fail(`target commit is ${manifest.target?.commit}, catalog is ${catalog.commit}`);

const catalogEntries = new Map(catalog.algorithm_packages.map((entry) => [
  entry.package,
  { package: entry.package, variants: entry.declared_variants },
]));
const manifestEntries = new Map();
const artifactRequired = new Set(manifest.artifact_required_packages ?? []);
if (!Array.isArray(manifest.artifact_required_packages ?? [])) {
  fail("artifact_required_packages must be an array");
}
for (const entry of manifest.packages ?? []) {
  if (!entry || typeof entry.package !== "string" || !Array.isArray(entry.variants)) {
    fail("manifest contains a malformed package entry");
    continue;
  }
  if (manifestEntries.has(entry.package)) fail(`duplicate package ${entry.package}`);
  manifestEntries.set(entry.package, entry);
  if (!validStatuses.has(entry.status)) fail(`${entry.package} has invalid status ${entry.status}`);
  if (typeof entry.evidence !== "string" || entry.evidence.length === 0) {
    fail(`${entry.package} has no evidence path`);
  } else if (!existsSync(resolve(root, entry.evidence))) {
    fail(`${entry.package} evidence does not exist: ${entry.evidence}`);
  }
  if (artifactRequired.has(entry.package) && entry.status !== "complete") {
    fail(`${entry.package} requires artifacts but is not complete`);
  }
  if (artifactRequired.has(entry.package) && entry.artifacts === undefined) {
    fail(`${entry.package} requires an artifact list`);
  }
  if (entry.status === "complete" && entry.artifacts !== undefined) {
    if (!Array.isArray(entry.artifacts) || entry.artifacts.length === 0) {
      fail(`${entry.package} has an invalid artifact list`);
    } else {
      const uniqueArtifacts = new Set();
      for (const artifact of entry.artifacts) {
        if (typeof artifact !== "string" || artifact.length === 0) {
          fail(`${entry.package} has a malformed artifact path`);
          continue;
        }
        if (uniqueArtifacts.has(artifact)) fail(`${entry.package} repeats artifact ${artifact}`);
        uniqueArtifacts.add(artifact);
        if (!existsSync(resolve(root, artifact))) {
          fail(`${entry.package} artifact does not exist: ${artifact}`);
        }
      }
    }
  }
}
for (const packageName of artifactRequired) {
  if (!manifestEntries.has(packageName)) fail(`artifact requirement names unknown package ${packageName}`);
}

for (const [packageName, expected] of catalogEntries) {
  const actual = manifestEntries.get(packageName);
  if (!actual) {
    fail(`unmapped package ${packageName}`);
    continue;
  }
  if (key(actual) !== key(expected)) {
    fail(`${packageName} variants differ: expected [${expected.variants}], found [${actual.variants}]`);
  }
}
for (const packageName of manifestEntries.keys()) {
  if (!catalogEntries.has(packageName)) fail(`stale package ${packageName}`);
}

if (!process.exitCode) {
  const counts = {};
  for (const entry of manifestEntries.values()) counts[entry.status] = (counts[entry.status] ?? 0) + 1;
  process.stdout.write(
    `verified ${manifestEntries.size} packages: ` +
    Object.entries(counts).sort().map(([status, count]) => `${count} ${status}`).join(", ") +
    "\n",
  );
}
