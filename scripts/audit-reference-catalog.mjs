#!/usr/bin/env node

/**
 * Build a reproducible source catalogue for a pinned reference checkout.
 *
 * The catalogue is derived exclusively from Git commit objects. It never
 * imports, executes, or reads package files from the checkout working tree.
 * Names and paths are collected as structural evidence; they are not an API
 * compatibility layer.
 */

import { execFileSync } from "node:child_process";
import { writeFileSync } from "node:fs";
import { resolve } from "node:path";

export const DEFAULT_TAG = "2.7.22";
export const DEFAULT_COMMIT = "0715c31f3f1d85924107d6aa5329806db69df899";

process.stdout.on("error", (error) => {
  if (error.code === "EPIPE") process.exit(0);
  throw error;
});

function gitValue(checkout, ...args) {
  try {
    return execFileSync("git", ["-C", checkout, ...args], {
      encoding: "utf8",
      stdio: ["ignore", "pipe", "pipe"],
    }).trim();
  } catch (error) {
    const detail = error.stderr?.toString().trim();
    throw new Error(
      `git ${args.join(" ")} failed${detail ? `: ${detail}` : ""}`,
    );
  }
}

function gitTree(checkout, commit) {
  let listing;
  try {
    listing = execFileSync(
      "git",
      ["-C", checkout, "ls-tree", "-r", "-z", "--name-only", commit],
      { encoding: "utf8", stdio: ["ignore", "pipe", "pipe"] },
    );
  } catch (error) {
    const detail = error.stderr?.toString().trim();
    throw new Error(
      `could not read the pinned Git tree${detail ? `: ${detail}` : ""}`,
    );
  }
  return listing.split("\0").filter(Boolean);
}

function gitShow(checkout, commit, path) {
  return execFileSync("git", ["-C", checkout, "show", `${commit}:${path}`], {
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
}

function isPython(path) {
  return path.endsWith(".py");
}

function isInit(path) {
  return path.endsWith("/__init__.py");
}

function relativePath(path, libraryRoot) {
  const prefix = `${libraryRoot}/`;
  if (!path.startsWith(prefix)) {
    throw new Error(`tree path is outside the package root: ${path}`);
  }
  return path.slice(prefix.length);
}

function moduleName(path, libraryRoot) {
  return relativePath(path, libraryRoot)
    .replace(/\.py$/, "")
    .split("/")
    .join(".");
}

function findLibraryRoot(tree) {
  const algoRoots = new Set(
    tree
      .filter((path) => path.endsWith("/algo/__init__.py"))
      .map((path) => path.slice(0, -"/algo/__init__.py".length)),
  );
  const objectRoots = new Set(
    tree
      .filter((path) => path.endsWith("/objects/__init__.py"))
      .map((path) => path.slice(0, -"/objects/__init__.py".length)),
  );
  const matches = [...algoRoots].filter((root) => objectRoots.has(root));
  if (matches.length !== 1) {
    throw new Error(
      `expected exactly one package root containing algo and objects, found ${matches.length}`,
    );
  }
  return matches[0];
}

function indentOf(line) {
  return line.match(/^[ \t]*/u)?.[0].length ?? 0;
}

function enumClasses(source) {
  const lines = source.split(/\r?\n/u);
  const classes = new Map();
  for (let index = 0; index < lines.length; ++index) {
    const declaration = lines[index].match(
      /^([ \t]*)class\s+([A-Za-z_]\w*)\s*\(([^)]*\bEnum\b[^)]*)\)\s*:/u,
    );
    if (!declaration) continue;

    const classIndent = declaration[1].length;
    const members = [];
    for (let body = index + 1; body < lines.length; ++body) {
      const line = lines[body];
      if (line.trim() !== "" && indentOf(line) <= classIndent) break;
      const member = line.match(
        /^[ \t]+([A-Za-z_]\w*)\s*(?::[^=]+)?\s*=/u,
      );
      if (member && !members.includes(member[1])) members.push(member[1]);
    }
    classes.set(declaration[2], members);
  }
  return classes;
}

function enumMemberExpressions(source, className) {
  const lines = source.split(/\r?\n/u);
  for (let index = 0; index < lines.length; ++index) {
    const declaration = lines[index].match(
      /^([ \t]*)class\s+([A-Za-z_]\w*)\s*\(([^)]*\bEnum\b[^)]*)\)\s*:/u,
    );
    if (!declaration || declaration[2] !== className) continue;
    const classIndent = declaration[1].length;
    const members = [];
    for (let body = index + 1; body < lines.length; ++body) {
      const line = lines[body];
      if (line.trim() !== "" && indentOf(line) <= classIndent) break;
      const member = line.match(
        /^[ \t]+([A-Za-z_]\w*)\s*(?::[^=]+)?\s*=\s*(.*?)\s*(?:#.*)?$/u,
      );
      if (member && !members.some((entry) => entry.name === member[1])) {
        members.push({ name: member[1], expression: member[2] });
      }
    }
    return members;
  }
  return [];
}

function dynamicVariantEntries(source) {
  const entries = [];
  for (const match of source.matchAll(
    /^\s*Variants\.([A-Za-z_]\w*)\s*=\s*(.*?)\s*(?:#.*)?$/gmu,
  )) {
    if (!entries.some((entry) => entry.name === match[1])) {
      entries.push({ name: match[1], expression: match[2] });
    }
  }
  return entries;
}

function literalAvailableEntries(source) {
  const entries = [];
  for (const declaration of source.matchAll(
    /^\s*(AVAILABLE_[A-Za-z0-9_]+)\s*=\s*\[((?:.|\r?\n)*?)\]/gmsu,
  )) {
    for (const literal of declaration[2].matchAll(
      /(["'])(.*?)\1/gsu,
    )) {
      if (!entries.some((entry) => entry.name === literal[2])) {
        entries.push({ name: literal[2], source: declaration[1] });
      }
    }
  }
  return entries;
}

function parseImports(source) {
  const lines = source.split(/\r?\n/u);
  const imports = [];
  for (let index = 0; index < lines.length; ++index) {
    const match = lines[index].match(
      /^\s*from\s+([.A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*)\s+import\s+(.+)$/u,
    );
    if (!match) continue;

    let imported = match[2];
    let balance = (imported.match(/\(/gu) ?? []).length -
      (imported.match(/\)/gu) ?? []).length;
    while (balance > 0 && index + 1 < lines.length) {
      imported += ` ${lines[++index]}`;
      balance += (lines[index].match(/\(/gu) ?? []).length;
      balance -= (lines[index].match(/\)/gu) ?? []).length;
    }
    imported = imported.replace(/#.*/u, "").replace(/[()]/gu, "");
    for (const item of imported.split(",")) {
      const symbol = item.trim().match(
        /^([A-Za-z_]\w*)(?:\s+as\s+([A-Za-z_]\w*))?$/u,
      );
      if (symbol) imports.push({ module: match[1], name: symbol[1], alias: symbol[2] });
    }
  }
  return imports;
}

function importBindings(imports, algorithmModule, libraryRoot, treeSet) {
  const bindings = new Map();
  for (const imported of imports) {
    const module = imported.module.startsWith(".")
      ? resolveImportModule(imported.module, algorithmModule)
      : imported.module;
    const sourceModule = module.startsWith(`${libraryRoot}.`)
      ? module.slice(libraryRoot.length + 1)
      : module;
    const directPath = sourcePathForModule(`${sourceModule}.${imported.name}`, libraryRoot, treeSet);
    const packagePath = sourcePathForModule(sourceModule, libraryRoot, treeSet);
    const targetModule = directPath
      ? directPath.endsWith("/__init__.py")
        ? `${sourceModule}.${imported.name}`
        : moduleName(directPath, libraryRoot)
      : packagePath
        ? sourceModule
        : undefined;
    bindings.set(imported.alias ?? imported.name, {
      module: targetModule,
      isPackage: directPath?.endsWith("/__init__.py") ?? false,
      sourceModule,
      sourceName: imported.name,
    });
  }
  return bindings;
}

function directVariantImports(imports, algorithmModule, libraryRoot, treeSet) {
  const members = [];
  const bindings = importBindings(imports, algorithmModule, libraryRoot, treeSet);
  for (const imported of imports) {
    const module = imported.module.startsWith(".")
      ? resolveImportModule(imported.module, algorithmModule)
      : imported.module;
    if (!module.endsWith(".variants")) continue;
    const localName = imported.alias ?? imported.name;
    const binding = bindings.get(localName);
    if (!binding?.module || !members.some((entry) => entry.name === localName)) {
      members.push({ name: localName, module: binding?.module });
    }
  }
  return members;
}

function resolveImportModule(imported, algorithmModule) {
  if (!imported.startsWith(".")) return imported;
  const dots = imported.match(/^\.+/u)[0].length;
  const suffix = imported.slice(dots);
  const base = algorithmModule.split(".").slice(0, -1 - (dots - 1));
  return [...base, ...(suffix ? suffix.split(".") : [])].join(".");
}

function sourcePathForModule(module, libraryRoot, treeSet) {
  const packagePrefix = `${libraryRoot}.`;
  const relativeModule = module.startsWith(packagePrefix)
    ? module.slice(packagePrefix.length)
    : module;
  const relative = relativeModule.split(".").join("/");
  const candidates = [
    `${libraryRoot}/${relative}.py`,
    `${libraryRoot}/${relative}/__init__.py`,
  ];
  return candidates.find((candidate) => treeSet.has(candidate));
}

function expressionModules(expression, imports, libraryRoot, treeSet) {
  const modules = [];
  for (const token of expression.matchAll(
    /\b([A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*)\b/gu,
  )) {
    const parts = token[1].split(".");
    const imported = imports.get(parts[0]);
    if (!imported?.module) continue;
    let target = imported.module;
    if (imported.isPackage && parts.length > 1) {
      const memberPath = sourcePathForModule(
        `${imported.module}.${parts[1]}`,
        libraryRoot,
        treeSet,
      );
      if (memberPath && !memberPath.endsWith("/__init__.py")) {
        target = moduleName(memberPath, libraryRoot);
      }
    }
    if (!modules.includes(target)) {
      modules.push(target);
    }
  }
  return modules;
}

function selectorBinding(selector, modules, kind, className = null, symbol = null) {
  return { selector, modules, kind, class: className, symbol };
}

function selectorBindings(checkout, commit, algorithmPath, algorithmSource, libraryRoot, treeSet, sourceCache) {
  const algorithmModule = moduleName(algorithmPath, libraryRoot);
  const imports = parseImports(algorithmSource);
  const importMap = importBindings(imports, algorithmModule, libraryRoot, treeSet);
  const bindings = [];
  const add = (binding) => {
    if (!bindings.some((entry) => entry.selector === binding.selector)) bindings.push(binding);
  };

  for (const member of enumMemberExpressions(algorithmSource, "Variants")) {
    const modules = expressionModules(
      member.expression,
      importMap,
      libraryRoot,
      treeSet,
    );
    add(selectorBinding(
      member.name,
      modules.length > 0 ? modules : [algorithmModule],
      "enum",
      "Variants",
      member.name,
    ));
  }
  for (const member of dynamicVariantEntries(algorithmSource)) {
    const modules = expressionModules(
      member.expression,
      importMap,
      libraryRoot,
      treeSet,
    );
    add(selectorBinding(
      member.name,
      modules.length > 0 ? modules : [algorithmModule],
      "dynamic",
      "Variants",
      member.name,
    ));
  }

  for (const imported of imports) {
    const selectorLike = imported.name === "Variants" ||
      imported.alias === "Variants" ||
      /variant/i.test(imported.name) ||
      /variant/i.test(imported.alias ?? "");
    if (!selectorLike) continue;
    const importedModule = resolveImportModule(imported.module, algorithmModule);
    const importedPath = sourcePathForModule(importedModule, libraryRoot, treeSet);
    if (!importedPath) continue;
    let importedSource = sourceCache.get(importedPath);
    if (importedSource === undefined) {
      importedSource = gitShow(checkout, commit, importedPath);
      sourceCache.set(importedPath, importedSource);
    }
    const members = enumClasses(importedSource).get(imported.name) ?? [];
    for (const name of members) {
      add(selectorBinding(
        name,
        [importedPath.endsWith("/__init__.py") ? algorithmModule : moduleName(importedPath, libraryRoot)],
        "imported-enum",
        imported.name,
        name,
      ));
    }
  }

  if (bindings.length > 0) return bindings;

  const available = literalAvailableEntries(algorithmSource);
  const packagePrefix = algorithmModule.slice(0, -".algorithm".length);
  for (const entry of available) {
    const path = sourcePathForModule(`${packagePrefix}.variants.${entry.name}`, libraryRoot, treeSet);
    add(selectorBinding(
      entry.name,
      [path ? moduleName(path, libraryRoot) : algorithmModule],
      "available",
      null,
      entry.source,
    ));
  }
  if (bindings.length > 0) return bindings;

  if (/\bvariant\s*=\s*None\b/u.test(algorithmSource)) {
    const direct = directVariantImports(imports, algorithmModule, libraryRoot, treeSet);
    for (const entry of direct) {
      add(selectorBinding(
        entry.name,
        [entry.module ?? algorithmModule],
        "variant-import",
        null,
        entry.name,
      ));
    }
    if (bindings.length > 0) return bindings;
  }
  return [selectorBinding("default", [algorithmModule], "default")];
}

function catalogue(checkout, expectedCommit = DEFAULT_COMMIT) {
  const actualCommit = gitValue(checkout, "rev-parse", "HEAD");
  const expectedResolved = gitValue(
    checkout,
    "rev-parse",
    "--verify",
    `${expectedCommit}^{commit}`,
  );
  if (actualCommit !== expectedResolved) {
    throw new Error(
      `expected pinned commit ${expectedResolved}, found ${actualCommit}`,
    );
  }

  const tree = gitTree(checkout, actualCommit);
  const treeSet = new Set(tree);
  const libraryRoot = findLibraryRoot(tree);
  const algoPrefix = `${libraryRoot}/algo/`;
  const algoFiles = tree.filter(
    (path) => path.startsWith(algoPrefix) && isPython(path),
  );
  const nonInitAlgoFiles = algoFiles.filter((path) => !isInit(path));
  const algorithmFiles = nonInitAlgoFiles
    .filter((path) => path.endsWith("/algorithm.py"))
    .sort();
  const sourceCache = new Map();
  const algorithms = algorithmFiles.map((path) => {
    const source = gitShow(checkout, actualCommit, path);
    sourceCache.set(path, source);
    const bindings = selectorBindings(
      checkout,
      actualCommit,
      path,
      source,
      libraryRoot,
      treeSet,
      sourceCache,
    );
    return {
      package: relativePath(path, libraryRoot)
        .slice(0, -"/algorithm.py".length)
        .split("/")
        .join("."),
      declared_variants: bindings.map((entry) => entry.selector),
      selector_bindings: bindings,
    };
  });

  const modules = nonInitAlgoFiles
    .map((path) => {
      const relative = relativePath(path, libraryRoot);
      const module = moduleName(path, libraryRoot);
      const kind = relative.endsWith("/algorithm.py")
        ? "dispatcher"
        : relative.split("/").includes("variants")
          ? "variant"
          : "support";
      return { module, path: relative, kind };
    })
    .sort((left, right) => left.module.localeCompare(right.module));
  const variantModules = modules
    .filter((entry) => entry.kind === "variant")
    .map((entry) => entry.module);
  const domains = [...new Set(
    algoFiles
      .map((path) => relativePath(path, libraryRoot).split("/")[1])
      .filter((domain) => domain && domain !== "__init__.py"),
  )].sort();
  const countPython = (section) => {
    const prefix = `${libraryRoot}/${section}/`;
    return tree.filter((path) => path.startsWith(prefix) && isPython(path)).length;
  };

  return {
    tag: gitValue(checkout, "describe", "--tags", "--exact-match", actualCommit),
    commit: actualCommit,
    package_root: libraryRoot,
    domains,
    algorithm_packages: algorithms,
    dispatcher_modules: algorithms.map((entry) => `${entry.package}.algorithm`),
    variant_modules: variantModules,
    modules,
    counts: {
      domains: domains.length,
      algorithm_packages: algorithms.length,
      dispatcher_modules: algorithms.length,
      variant_modules: variantModules.length,
      support_modules: modules.filter((entry) => entry.kind === "support").length,
      non_init_algo_modules: modules.length,
      algo_python_files: countPython("algo"),
      object_python_files: countPython("objects"),
      statistics_python_files: countPython("statistics"),
      visualization_python_files: countPython("visualization"),
      streaming_python_files: countPython("streaming"),
      util_python_files: countPython("util"),
    },
  };
}

function parseArguments(argv) {
  if (argv.length === 0) {
    throw new Error(
      "usage: audit-reference-catalog.mjs CHECKOUT [--expected-tag TAG] [--expected-commit COMMIT] [-o FILE]",
    );
  }
  const options = {
    checkout: resolve(argv[0]),
    expectedTag: DEFAULT_TAG,
    expectedCommit: DEFAULT_COMMIT,
  };
  for (let index = 1; index < argv.length; ++index) {
    const argument = argv[index];
    if (argument === "--expected-tag") {
      options.expectedTag = argv[++index];
    } else if (argument === "--expected-commit") {
      options.expectedCommit = argv[++index];
    } else if (argument === "-o" || argument === "--output") {
      options.output = resolve(argv[++index]);
    } else {
      throw new Error(`unknown argument: ${argument}`);
    }
  }
  return options;
}

const options = parseArguments(process.argv.slice(2));
const result = catalogue(options.checkout, options.expectedCommit);
if (result.tag !== options.expectedTag) {
  throw new Error(`expected reference tag ${options.expectedTag}, found ${result.tag}`);
}

const rendered = `${JSON.stringify(result, null, 2)}\n`;
if (options.output) writeFileSync(options.output, rendered, "utf8");
else process.stdout.write(rendered);
