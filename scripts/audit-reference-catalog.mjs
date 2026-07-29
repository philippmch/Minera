#!/usr/bin/env node

/**
 * Build a reproducible source catalogue for a pinned reference checkout.
 *
 * This script inventories names and paths only. It never imports or executes
 * the reference package, so optional Python dependencies are unnecessary. The
 * resulting JSON is evidence for Minera's parity checklist, not a compatibility
 * layer.
 */

import { execFileSync } from "node:child_process";
import { readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { resolve, relative, sep } from "node:path";

process.stdout.on("error", (error) => {
  if (error.code === "EPIPE") process.exit(0);
  throw error;
});

function gitValue(referenceRoot, ...args) {
  return execFileSync("git", ["-C", referenceRoot, ...args], {
    encoding: "utf8",
  }).trim();
}

function ignoredDirectory(name) {
  return name === "__pycache__" || name.startsWith(".");
}

function filesBelow(root) {
  const files = [];
  for (const entry of readdirSync(root, { withFileTypes: true })) {
    const path = resolve(root, entry.name);
    if (entry.isDirectory()) {
      if (!ignoredDirectory(entry.name)) files.push(...filesBelow(path));
    }
    else files.push(path);
  }
  return files;
}

function isDirectory(path) {
  try {
    return statSync(path).isDirectory();
  } catch {
    return false;
  }
}

function findLibraryRoot(referenceRoot) {
  const candidates = [referenceRoot];
  for (const entry of readdirSync(referenceRoot, { withFileTypes: true })) {
    if (entry.isDirectory() && !ignoredDirectory(entry.name))
      candidates.push(resolve(referenceRoot, entry.name));
  }
  const matches = candidates.filter(
    (candidate) =>
      isDirectory(resolve(candidate, "algo")) &&
      isDirectory(resolve(candidate, "objects")),
  );
  if (matches.length !== 1) {
    throw new Error(
      `could not locate exactly one reference package below ${referenceRoot}`,
    );
  }
  return matches[0];
}

function enumVariants(algorithmFile) {
  const source = readFileSync(algorithmFile, "utf8");
  const classMatch = source.match(
    /^class Variants(?:\([^)]*\))?:\r?\n((?:[ \t]+.*(?:\r?\n|$))*)/m,
  );
  if (!classMatch) return ["default"];

  const variants = [];
  for (const match of classMatch[1].matchAll(
    /^[ \t]+([A-Z][A-Z0-9_]*)\s*(?::[^=]+)?=/gm,
  )) {
    variants.push(match[1]);
  }
  return variants.length === 0 ? ["default"] : variants;
}

function relativeModule(libraryRoot, path) {
  return relative(libraryRoot, path)
    .replace(/\.py$/, "")
    .split(sep)
    .join(".");
}

function pythonFileCount(root) {
  return filesBelow(root).filter((path) => path.endsWith(".py")).length;
}

function catalogue(referenceRoot) {
  const libraryRoot = findLibraryRoot(referenceRoot);
  const algoRoot = resolve(libraryRoot, "algo");

  const algoFiles = filesBelow(algoRoot).filter((path) => path.endsWith(".py"));
  const algorithmFiles = algoFiles
    .filter((path) => path.endsWith(`${sep}algorithm.py`))
    .sort();
  const algorithms = algorithmFiles.map((path) => ({
    package: relativeModule(libraryRoot, resolve(path, "..")),
    declared_variants: enumVariants(path),
  }));
  const variantModules = algoFiles
    .filter(
      (path) =>
        path.includes(`${sep}variants${sep}`) &&
        !path.endsWith(`${sep}__init__.py`),
    )
    .map((path) => relativeModule(libraryRoot, path))
    .sort();
  const domains = readdirSync(algoRoot)
    .filter(
      (name) =>
        !ignoredDirectory(name) &&
        isDirectory(resolve(algoRoot, name)),
    )
    .sort();

  return {
    tag: gitValue(referenceRoot, "describe", "--tags", "--exact-match"),
    commit: gitValue(referenceRoot, "rev-parse", "HEAD"),
    domains,
    algorithm_packages: algorithms,
    variant_modules: variantModules,
    counts: {
      domains: domains.length,
      algorithm_packages: algorithms.length,
      variant_modules: variantModules.length,
      algo_python_files: algoFiles.length,
      object_python_files: pythonFileCount(resolve(libraryRoot, "objects")),
      statistics_python_files: pythonFileCount(
        resolve(libraryRoot, "statistics"),
      ),
      visualization_python_files: pythonFileCount(
        resolve(libraryRoot, "visualization"),
      ),
    },
  };
}

function parseArguments(argv) {
  if (argv.length === 0) {
    throw new Error(
      "usage: audit-reference-catalog.mjs CHECKOUT [--expected-tag TAG] [-o FILE]",
    );
  }
  const options = { checkout: resolve(argv[0]), expectedTag: "2.7.22" };
  for (let index = 1; index < argv.length; ++index) {
    const argument = argv[index];
    if (argument === "--expected-tag") {
      options.expectedTag = argv[++index];
    } else if (argument === "-o" || argument === "--output") {
      options.output = resolve(argv[++index]);
    } else {
      throw new Error(`unknown argument: ${argument}`);
    }
  }
  return options;
}

const options = parseArguments(process.argv.slice(2));
const result = catalogue(options.checkout);
if (result.tag !== options.expectedTag) {
  throw new Error(`expected reference tag ${options.expectedTag}, found ${result.tag}`);
}

const rendered = `${JSON.stringify(result, null, 2)}\n`;
if (options.output) writeFileSync(options.output, rendered, "utf8");
else process.stdout.write(rendered);
