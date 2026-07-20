#!/usr/bin/env node

/**
 * Build a reproducible source catalogue for a pinned PM4Py checkout.
 *
 * This script inventories names and paths only. It never imports or executes
 * PM4Py, so optional Python dependencies are unnecessary. The resulting JSON
 * is evidence for Minera's parity checklist, not a compatibility layer.
 */

import { execFileSync } from "node:child_process";
import { readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { resolve, relative, sep } from "node:path";

process.stdout.on("error", (error) => {
  if (error.code === "EPIPE") process.exit(0);
  throw error;
});

function gitValue(checkout, ...args) {
  return execFileSync("git", ["-C", checkout, ...args], {
    encoding: "utf8",
  }).trim();
}

function filesBelow(root) {
  const files = [];
  for (const entry of readdirSync(root, { withFileTypes: true })) {
    const path = resolve(root, entry.name);
    if (entry.isDirectory()) files.push(...filesBelow(path));
    else files.push(path);
  }
  return files;
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

function relativeModule(pm4pyRoot, path) {
  return relative(pm4pyRoot, path)
    .replace(/\.py$/, "")
    .split(sep)
    .join(".");
}

function pythonFileCount(root) {
  return filesBelow(root).filter((path) => path.endsWith(".py")).length;
}

function catalogue(checkout) {
  const pm4pyRoot = resolve(checkout, "pm4py");
  const algoRoot = resolve(pm4pyRoot, "algo");
  if (!statSync(algoRoot).isDirectory()) {
    throw new Error(`${checkout} is not a PM4Py checkout`);
  }

  const algoFiles = filesBelow(algoRoot).filter((path) => path.endsWith(".py"));
  const algorithmFiles = algoFiles
    .filter((path) => path.endsWith(`${sep}algorithm.py`))
    .sort();
  const algorithms = algorithmFiles.map((path) => ({
    package: relativeModule(pm4pyRoot, resolve(path, "..")),
    declared_variants: enumVariants(path),
  }));
  const variantModules = algoFiles
    .filter(
      (path) =>
        path.includes(`${sep}variants${sep}`) &&
        !path.endsWith(`${sep}__init__.py`),
    )
    .map((path) => relativeModule(pm4pyRoot, path))
    .sort();
  const domains = readdirSync(algoRoot)
    .filter((name) => statSync(resolve(algoRoot, name)).isDirectory())
    .sort();

  return {
    tag: gitValue(checkout, "describe", "--tags", "--exact-match"),
    commit: gitValue(checkout, "rev-parse", "HEAD"),
    domains,
    algorithm_packages: algorithms,
    variant_modules: variantModules,
    counts: {
      domains: domains.length,
      algorithm_packages: algorithms.length,
      variant_modules: variantModules.length,
      algo_python_files: algoFiles.length,
      object_python_files: pythonFileCount(resolve(pm4pyRoot, "objects")),
      statistics_python_files: pythonFileCount(
        resolve(pm4pyRoot, "statistics"),
      ),
      visualization_python_files: pythonFileCount(
        resolve(pm4pyRoot, "visualization"),
      ),
    },
  };
}

function parseArguments(argv) {
  if (argv.length === 0) {
    throw new Error(
      "usage: audit-pm4py-catalog.mjs CHECKOUT [--expected-tag TAG] [-o FILE]",
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
  throw new Error(`expected PM4Py ${options.expectedTag}, found ${result.tag}`);
}

const rendered = `${JSON.stringify(result, null, 2)}\n`;
if (options.output) writeFileSync(options.output, rendered, "utf8");
else process.stdout.write(rendered);
