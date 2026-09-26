#!/usr/bin/env node

import { execFileSync } from "node:child_process";
import { lstatSync, readFileSync } from "node:fs";
import { dirname, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const repositoryRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const auditScript = resolve(repositoryRoot, "scripts", "audit-reference-catalog.mjs");
const defaultManifestPath = resolve(repositoryRoot, "docs", "reference-2.7.22-coverage.json");
const defaultTag = "2.7.22";
const defaultCommit = "0715c31f3f1d85924107d6aa5329806db69df899";
const statuses = new Set(["complete", "partial", "missing", "adapter", "integration"]);
const bindingKinds = new Set([
  "enum", "imported-enum", "dynamic", "available", "variant-import", "default",
]);
const sourceExtensions = new Set([
  "c", "cc", "cpp", "cxx", "h", "hh", "hpp", "hxx", "js", "mjs", "py", "rs", "ts", "tsx",
]);
const testExtensions = new Set([...sourceExtensions, "csv", "json", "txt", "xes", "xml", "yaml", "yml"]);

let projectRoot = repositoryRoot;
const artifactCache = new Map();
const textCache = new Map();
let testRegistryCache;

function parseArguments(argv) {
  if (argv.length < 1) {
    throw new Error(
      "usage: verify-reference-parity.mjs CHECKOUT [--manifest FILE] [--project-root DIR] " +
      "[--expected-tag TAG] [--expected-commit COMMIT] [--require-complete]",
    );
  }
  const options = {
    checkout: resolve(argv[0]),
    manifestPath: defaultManifestPath,
    projectRoot: repositoryRoot,
    expectedTag: defaultTag,
    expectedCommit: defaultCommit,
    requireComplete: false,
  };
  for (let index = 1; index < argv.length; ++index) {
    const argument = argv[index];
    const next = () => {
      if (index + 1 >= argv.length) throw new Error(`${argument} requires a value`);
      return argv[++index];
    };
    if (argument === "--manifest") options.manifestPath = resolve(next());
    else if (argument === "--project-root") options.projectRoot = resolve(next());
    else if (argument === "--expected-tag") options.expectedTag = next();
    else if (argument === "--expected-commit") options.expectedCommit = next();
    else if (argument === "--require-complete") options.requireComplete = true;
    else throw new Error(`unknown argument: ${argument}`);
  }
  return options;
}

function runAudit(options) {
  try {
    return JSON.parse(execFileSync(process.execPath, [
      auditScript,
      options.checkout,
      "--expected-tag", options.expectedTag,
      "--expected-commit", options.expectedCommit,
    ], { encoding: "utf8" }));
  } catch (error) {
    const detail = [error.stdout, error.stderr]
      .filter(Boolean)
      .map((value) => value.toString().trim())
      .filter(Boolean)
      .join(" ");
    throw new Error(`catalog audit failed${detail ? `: ${detail}` : ""}`);
  }
}

function fail(errors, message) {
  errors.push(message);
}

function sortedUnique(values) {
  return [...new Set(values)].sort();
}

function sameValues(left, right) {
  if (!Array.isArray(left) || !Array.isArray(right)) return false;
  const a = sortedUnique(left);
  const b = sortedUnique(right);
  return a.length === b.length && a.every((value, index) => value === b[index]);
}

function exactKeys(value, keys) {
  return value && typeof value === "object" && !Array.isArray(value) &&
    sameValues(Object.keys(value), keys);
}

function sameJson(left, right) {
  return JSON.stringify(left) === JSON.stringify(right);
}

function normalizeProjectPath(path) {
  return path.replaceAll("\\", "/").replace(/^\.\//u, "");
}

function trackedArtifact(path) {
  if (artifactCache.has(path)) return artifactCache.get(path);
  let result = { ok: false, reason: "malformed path" };
  if (typeof path === "string" && path.length > 0 && !/[\0\r\n]/u.test(path) &&
    !path.includes("\\") && !path.startsWith("/") && !/^[A-Za-z]:/u.test(path) &&
    !path.split("/").some((part) => part === "." || part === "..")) {
    const absolute = resolve(projectRoot, path);
    const relativePath = relative(projectRoot, absolute);
    if (relativePath !== "" && relativePath !== ".." &&
      !relativePath.startsWith(`..\\`) && !relativePath.startsWith("../")) {
      try {
        if (!lstatSync(absolute).isFile()) result = { ok: false, reason: "not a regular file" };
        else {
          try {
            const tracked = execFileSync(
              "git",
              ["-C", projectRoot, "ls-files", "--error-unmatch", "--", path],
              { encoding: "utf8", stdio: ["ignore", "pipe", "pipe"] },
            ).trim().split(/\r?\n/u).filter(Boolean);
            result = tracked.length === 1 && normalizeProjectPath(tracked[0]) === path
              ? { ok: true, path }
              : { ok: false, reason: "not tracked" };
          } catch {
            result = { ok: false, reason: "not tracked" };
          }
        }
      } catch {
        result = { ok: false, reason: "not a tracked regular file" };
      }
    } else result = { ok: false, reason: "outside project root" };
  }
  artifactCache.set(path, result);
  return result;
}

function readTrackedText(path) {
  if (textCache.has(path)) return textCache.get(path);
  const text = trackedArtifact(path).ok
    ? readFileSync(resolve(projectRoot, path), "utf8")
    : undefined;
  textCache.set(path, text);
  return text;
}

function extension(path) {
  return path.match(/\.([A-Za-z0-9]+)$/u)?.[1]?.toLowerCase();
}

function isImplementationPath(path) {
  const ext = extension(path);
  return Boolean(ext && sourceExtensions.has(ext)) && !/^docs\//iu.test(path) &&
    !/(^|\/)(tests?|fixtures?|testdata|golden|corpus)(\/|$)/iu.test(path) &&
    !/\.(test|spec)\.[A-Za-z0-9]+$/iu.test(path);
}

function isTestPath(path) {
  const ext = extension(path);
  return Boolean(ext && testExtensions.has(ext)) && !/^docs\//iu.test(path) &&
    (/(^|\/)(tests?|fixtures?|testdata|golden|corpus)(\/|$)/iu.test(path) ||
      /\.(test|spec)\.[A-Za-z0-9]+$/iu.test(path));
}

function maskRange(buffer, source, begin, end) {
  for (let index = begin; index < end; ++index) {
    if (source[index] !== "\n" && source[index] !== "\r") buffer[index] = " ";
  }
}

function maskCommentsAndLiterals(source) {
  const buffer = [...source];
  for (let index = 0; index < source.length;) {
    const raw = source.slice(index).match(/^(?:u8|u|U|L)?R"([^\s()\\]{0,16})\(/u);
    if (raw) {
      const marker = `)${raw[1]}"`;
      const found = source.indexOf(marker, index + raw[0].length);
      const end = found < 0 ? source.length : found + marker.length;
      maskRange(buffer, source, index, end);
      index = end;
      continue;
    }
    if (source.startsWith("//", index)) {
      const found = source.indexOf("\n", index + 2);
      const end = found < 0 ? source.length : found;
      maskRange(buffer, source, index, end);
      index = end;
      continue;
    }
    if (source.startsWith("/*", index)) {
      const found = source.indexOf("*/", index + 2);
      const end = found < 0 ? source.length : found + 2;
      maskRange(buffer, source, index, end);
      index = end;
      continue;
    }
    if (source[index] === "#") {
      const found = source.indexOf("\n", index + 1);
      const end = found < 0 ? source.length : found;
      maskRange(buffer, source, index, end);
      index = end;
      continue;
    }
    if (source[index] === "'" && /\d/u.test(source[index - 1] ?? "") &&
      /\d/u.test(source[index + 1] ?? "")) {
      ++index;
      continue;
    }
    if (source[index] === "\"" || source[index] === "'") {
      const quote = source[index];
      let end = index + 1;
      while (end < source.length) {
        if (source[end] === "\\") end += 2;
        else if (source[end++] === quote) break;
      }
      maskRange(buffer, source, index, end);
      index = end;
      continue;
    }
    ++index;
  }
  return buffer.join("");
}

function escapeRegExp(value) {
  return value.replace(/[.*+?^${}()|[\]\\]/gu, "\\$&");
}

function symbolParts(symbol) {
  if (typeof symbol !== "string" ||
    !/^[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*$/u.test(symbol)) return undefined;
  return symbol.split("::");
}

function codeContainsSymbol(source, symbol) {
  const parts = symbolParts(symbol);
  if (!parts || typeof source !== "string") return false;
  const code = maskCommentsAndLiterals(source);
  const qualified = parts.map(escapeRegExp).join("\\s*::\\s*");
  return new RegExp(`\\b${qualified}\\b`, "u").test(code) ||
    new RegExp(`\\b${escapeRegExp(parts.at(-1))}\\b`, "u").test(code);
}

function typeBodyRanges(code, owner, declaration) {
  const matcher = new RegExp("\\b(?:" + declaration + ")\\s+" +
    escapeRegExp(owner) + "\\b", "gu");
  const ranges = [];
  for (const match of code.matchAll(matcher)) {
    let cursor = match.index + match[0].length;
    while (cursor < code.length) {
      const current = code[cursor];
      if (current === "(") {
        const close = matchingDelimiter(code, cursor, "(", ")");
        if (close < 0) break;
        cursor = close + 1;
        continue;
      }
      if (current === "[") {
        const close = matchingDelimiter(code, cursor, "[", "]");
        if (close < 0) break;
        cursor = close + 1;
        continue;
      }
      if (current === "{") {
        const close = matchingDelimiter(code, cursor, "{", "}");
        if (close >= 0) ranges.push({ begin: cursor + 1, end: close });
        break;
      }
      if (current === ";") break;
      ++cursor;
    }
  }
  return ranges;
}

function topLevelIdentifierPositions(code, begin, end, token) {
  const matcher = new RegExp("\\b" + escapeRegExp(token) + "\\b", "gu");
  const positions = [];
  let depth = 0;
  let cursor = begin;
  for (const match of code.matchAll(matcher)) {
    if (match.index < begin) continue;
    if (match.index >= end) break;
    while (cursor < match.index) {
      if (code[cursor] === "{") ++depth;
      else if (code[cursor] === "}" && depth > 0) --depth;
      ++cursor;
    }
    if (depth === 0) positions.push(match.index);
    cursor = match.index + match[0].length;
  }
  return positions;
}

function topLevelMemberStatementStart(code, begin, target) {
  let depth = 0;
  let start = begin;
  for (let index = begin; index < target; ++index) {
    if (code[index] === "{") ++depth;
    else if (code[index] === "}") {
      if (depth > 0) --depth;
      if (depth === 0) start = index + 1;
    } else if (depth === 0 && code[index] === ";") start = index + 1;
  }
  return start;
}

function topLevelMemberStatementEnd(code, target, end) {
  let depth = 0;
  for (let index = target; index < end; ++index) {
    if (code[index] === "{") {
      if (depth === 0) return { end: index, delimiter: "{" };
      ++depth;
    } else if (code[index] === "}") {
      if (depth === 0) return { end: index, delimiter: "}" };
      --depth;
    } else if (depth === 0 && code[index] === ";") {
      return { end: index, delimiter: ";" };
    }
  }
  return { end, delimiter: "" };
}

function removeAccessLabels(prefix) {
  let result = prefix.trim();
  while (true) {
    const match = result.match(/^(?:public|private|protected)\s*:\s*/u);
    if (!match) return result;
    result = result.slice(match[0].length).trim();
  }
}

function looksLikeMemberType(prefix) {
  const normalized = removeAccessLabels(prefix);
  if (!normalized || /[=(){};.'"+\-/%!?|^]/u.test(normalized)) return false;
  if (/\b(?:return|co_return|if|else|for|while|switch|case|goto|throw|friend|using|typedef)\b/u.test(normalized)) {
    return false;
  }
  if (!/^[A-Za-z0-9_\s:<>*&,\[\]]+$/u.test(normalized)) return false;
  return /[A-Za-z_]\w*/u.test(normalized);
}

function bodyDeclaresDataMember(code, body, member) {
  for (const target of topLevelIdentifierPositions(code, body.begin, body.end, member)) {
    const start = topLevelMemberStatementStart(code, body.begin, target);
    const statement = topLevelMemberStatementEnd(code, target, body.end);
    const prefix = code.slice(start, target);
    const suffix = code.slice(target + member.length, statement.end).trim();
    const suffixWithDelimiter = suffix || statement.delimiter;
    if (!/^(?:=|\{|:|,|;|\[|$)/u.test(suffixWithDelimiter)) continue;
    if (looksLikeMemberType(prefix)) return true;
  }
  return false;
}

function codeDeclaresDataMember(code, owner, member) {
  return typeBodyRanges(code, owner, "class|struct|union")
    .some((body) => bodyDeclaresDataMember(code, body, member));
}

function bodyDeclaresEnumMember(code, body, member) {
  for (const target of topLevelIdentifierPositions(code, body.begin, body.end, member)) {
    let start = body.begin;
    let depth = 0;
    for (let index = body.begin; index < target; ++index) {
      if (code[index] === "{") ++depth;
      else if (code[index] === "}" && depth > 0) --depth;
      else if (depth === 0 && code[index] === ",") start = index + 1;
    }
    const statement = topLevelMemberStatementEnd(code, target, body.end);
    const prefix = code.slice(start, target).trim();
    const suffix = code.slice(target + member.length, statement.end).trim() || statement.delimiter;
    if (!prefix && /^(?:=|,|;|\})/u.test(suffix)) return true;
    if (!prefix && suffix === "") return true;
  }
  return false;
}

function codeDeclaresEnumMember(code, owner, member) {
  return typeBodyRanges(code, owner, "enum(?:\\s+(?:class|struct))?")
    .some((body) => bodyDeclaresEnumMember(code, body, member));
}

function looksLikeFunctionDeclarationPrefix(prefix) {
  const normalized = prefix.trim();
  if (!normalized || normalized.includes("=") ||
    /[(){};.'"+\-%!?|^]/u.test(normalized) || normalized.includes("/") ||
    /\b(?:return|co_return|throw|new|delete|if|else|for|while|switch|case|goto|do|try|catch|sizeof|co_await|co_yield|not|compl|bitand|and|or|xor|bitor|not_eq|and_eq|or_eq|xor_eq)\b/u.test(normalized) ||
    normalized.replaceAll("::", "").includes(":")) return false;
  if (!/^[A-Za-z0-9_\s:<>*&,\[\]]+$/u.test(normalized)) return false;
  return /(?:\]\]|\b[A-Za-z_]\w*|[*&>])(?:\s+[A-Za-z_]\w*::)*$/u.test(normalized);
}

function functionDeclarationPrefix(code, position) {
  // Newlines are legal between a C++ return type and its declarator name.
  // Use actual statement/type-body boundaries so multiline template return
  // types stay together, while assignments, calls, and control flow remain in
  // the prefix and fail the type-shape check above.
  const boundary = Math.max(
    code.lastIndexOf(";", position - 1),
    code.lastIndexOf("{", position - 1),
    code.lastIndexOf("}", position - 1),
  );
  return code.slice(boundary + 1, position).trim();
}

function codeDeclaresFunction(code, token) {
  const matcher = new RegExp("\\b" + escapeRegExp(token) + "\\s*\\(", "gu");
  for (const match of code.matchAll(matcher)) {
    const openParen = code.indexOf("(", match.index);
    const closeParen = matchingDelimiter(code, openParen, "(", ")");
    if (closeParen < 0) continue;
    let cursor = closeParen + 1;
    while (cursor < code.length && /\s/u.test(code[cursor])) ++cursor;
    while (cursor < code.length && code[cursor] !== ";" && code[cursor] !== "{") ++cursor;
    if (cursor >= code.length) continue;
    if (looksLikeFunctionDeclarationPrefix(
      functionDeclarationPrefix(code, match.index),
    )) return true;
  }
  return false;
}

function bodyDeclaresFunction(code, body, member) {
  for (const target of topLevelIdentifierPositions(code, body.begin, body.end, member)) {
    let cursor = target + member.length;
    while (cursor < body.end && /\s/u.test(code[cursor])) ++cursor;
    if (code[cursor] !== "(") continue;
    const closeParen = matchingDelimiter(code, cursor, "(", ")");
    if (closeParen < 0 || closeParen >= body.end) continue;
    const start = topLevelMemberStatementStart(code, body.begin, target);
    if (!looksLikeMemberType(code.slice(start, target))) continue;
    const statement = topLevelMemberStatementEnd(code, target, body.end);
    if (statement.delimiter === ";" || statement.delimiter === "{") return true;
  }
  return false;
}

function codeDeclaresOwnedFunction(code, owner, member) {
  if (codeDeclaresFunction(code, `${owner}::${member}`)) return true;
  return typeBodyRanges(code, owner, "class|struct|union")
    .some((body) => bodyDeclaresFunction(code, body, member));
}

function bodyDeclaresNestedType(code, body, member) {
  for (const target of topLevelIdentifierPositions(code, body.begin, body.end, member)) {
    const start = topLevelMemberStatementStart(code, body.begin, target);
    const prefix = removeAccessLabels(code.slice(start, target));
    if (/\b(?:class|struct|union|enum(?:\s+(?:class|struct))?|using)\s*$/u.test(prefix)) {
      return true;
    }
  }
  return false;
}

function codeDeclaresNestedType(code, owner, member) {
  return typeBodyRanges(code, owner, "class|struct|union")
    .some((body) => bodyDeclaresNestedType(code, body, member));
}

function codeDeclaresEnumMemberWithoutOwner(code, member) {
  const matcher = /\benum\b[^{};]*\{/gu;
  for (const match of code.matchAll(matcher)) {
    const open = code.indexOf("{", match.index);
    const close = matchingDelimiter(code, open, "{", "}");
    if (close >= 0 && bodyDeclaresEnumMember(
      code, { begin: open + 1, end: close }, member,
    )) return true;
  }
  return false;
}

function codeDeclaresVariable(code, member) {
  const matcher = new RegExp(
    `\\b${escapeRegExp(member)}\\b(?=\\s*(?:=|,|;|\\[|\\{))`, "gu",
  );
  for (const match of code.matchAll(matcher)) {
    const prefix = functionDeclarationPrefix(code, match.index);
    if (looksLikeFunctionDeclarationPrefix(prefix)) return true;
  }
  return false;
}

function codeDeclaresSymbol(source, symbol) {
  const parts = symbolParts(symbol);
  if (!parts || typeof source !== "string") return false;
  const code = maskCommentsAndLiterals(source);
  const token = escapeRegExp(parts.at(-1));
  if (parts.length > 1) {
    // A qualified data member must be declared inside the named owner type in
    // this same production artifact. Do not let a local variable or a mere
    // use of the final component satisfy a qualified proof.
    if (codeDeclaresDataMember(code, parts.at(-2), parts.at(-1)) ||
      codeDeclaresEnumMember(code, parts.at(-2), parts.at(-1))) return true;
    return codeDeclaresNestedType(code, parts.at(-2), parts.at(-1)) ||
      codeDeclaresOwnedFunction(code, parts.at(-2), parts.at(-1));
  }
  const patterns = [
    new RegExp(`\\b(?:class|struct|union|enum(?:\\s+class)?|using)\\s+${token}\\b`, "u"),
  ];
  if (patterns.some((pattern) => pattern.test(code))) return true;
  if (codeDeclaresEnumMemberWithoutOwner(code, parts.at(-1)) ||
    codeDeclaresVariable(code, parts.at(-1))) return true;
  return codeDeclaresFunction(code, parts.at(-1));
}

function matchingDelimiter(code, open, left, right) {
  let depth = 0;
  for (let index = open; index < code.length; ++index) {
    if (code[index] === left) ++depth;
    else if (code[index] === right && --depth === 0) return index;
  }
  return -1;
}

function functionBody(source, name) {
  if (typeof source !== "string" || !/^[A-Za-z_]\w*$/u.test(name)) return undefined;
  const code = maskCommentsAndLiterals(source);
  const matcher = new RegExp(`\\b${escapeRegExp(name)}\\s*\\(`, "gu");
  for (const match of code.matchAll(matcher)) {
    const openParen = code.indexOf("(", match.index);
    const closeParen = matchingDelimiter(code, openParen, "(", ")");
    if (closeParen < 0) continue;
    let cursor = closeParen + 1;
    while (cursor < code.length && /\s/u.test(code[cursor])) ++cursor;
    while (cursor < code.length && code[cursor] !== "{" && code[cursor] !== ";") ++cursor;
    if (code[cursor] !== "{") continue;
    const closeBrace = matchingDelimiter(code, cursor, "{", "}");
    if (closeBrace >= 0) return code.slice(cursor + 1, closeBrace);
  }
  return undefined;
}

function cmakeTestRegistry() {
  if (testRegistryCache) return testRegistryCache;
  let cmakeFiles = [];
  try {
    cmakeFiles = execFileSync("git", ["-C", projectRoot, "ls-files", "*CMakeLists.txt"], {
      encoding: "utf8",
      stdio: ["ignore", "pipe", "pipe"],
    }).trim().split(/\r?\n/u).filter(Boolean).map(normalizeProjectPath);
  } catch {
    cmakeFiles = [];
  }
  const targets = new Map();
  const registered = new Set();
  for (const cmakePath of cmakeFiles) {
    const source = readTrackedText(cmakePath);
    if (!source) continue;
    const clean = source.replace(/#[^\r\n]*/gu, "");
    const cmakeDirectory = dirname(resolve(projectRoot, cmakePath));
    for (const match of clean.matchAll(/\badd_executable\s*\(\s*([^\s)]+)\s+([\s\S]*?)\)/gmu)) {
      const target = match[1];
      const files = match[2].split(/\s+/u)
        .map((path) => path.replace(/^["']|["']$/gu, ""))
        .filter((path) => path && !/[${}<>]/u.test(path))
        .map((path) => normalizeProjectPath(relative(projectRoot, resolve(cmakeDirectory, path))));
      targets.set(target, new Set(files));
    }
    for (const match of clean.matchAll(
      /\badd_test\s*\(\s*(?:NAME\s+([^\s)]+)\s+COMMAND\s+([^\s)]+)|([^\s)]+)\s+([^\s)]+))/gmu,
    )) {
      const name = match[1] ?? match[3];
      const command = match[2] ?? match[4];
      if (name && command && (command === name || command.includes(`:${name}>`))) registered.add(name);
    }
  }
  testRegistryCache = { targets, registered };
  return testRegistryCache;
}

function validateArtifactList(owner, artifacts, required, errors) {
  if (artifacts === undefined) {
    if (required) fail(errors, `${owner} claims complete without artifacts`);
    return new Set();
  }
  if (!Array.isArray(artifacts) || artifacts.length === 0) {
    fail(errors, `${owner} has an invalid artifact list`);
    return new Set();
  }
  const result = new Set();
  for (const path of artifacts) {
    if (typeof path !== "string" || path.length === 0) {
      fail(errors, `${owner} has a malformed artifact path`);
      continue;
    }
    if (result.has(path)) fail(errors, `${owner} repeats artifact ${path}`);
    result.add(path);
    const info = trackedArtifact(path);
    if (!info.ok) fail(errors, `${owner} artifact is ${info.reason}: ${path}`);
    else if (/^docs\//iu.test(path)) fail(errors, `${owner} artifact is documentation-only: ${path}`);
  }
  return result;
}

function validateEvidence(owner, path, errors) {
  if (typeof path !== "string" || path.length === 0) {
    fail(errors, `${owner} has no evidence path`);
    return;
  }
  const info = trackedArtifact(path);
  if (!info.ok) fail(errors, `${owner} evidence is ${info.reason}: ${path}`);
}

function validateImplementationProofs(owner, values, artifacts, errors) {
  if (!Array.isArray(values) || values.length === 0) {
    fail(errors, `${owner} has no implementation proof`);
    return [];
  }
  const validated = [];
  for (const proof of values) {
    if (!exactKeys(proof, ["path", "symbol"]) ||
      typeof proof.path !== "string" || typeof proof.symbol !== "string") {
      fail(errors, `${owner} has a malformed implementation proof`);
      continue;
    }
    if (!artifacts.has(proof.path)) {
      fail(errors, `${owner} implementation proof is not one of its artifacts: ${proof.path}`);
      continue;
    }
    const info = trackedArtifact(proof.path);
    if (!info.ok) {
      fail(errors, `${owner} implementation proof is ${info.reason}: ${proof.path}`);
      continue;
    }
    if (!isImplementationPath(proof.path)) {
      fail(errors, `${owner} implementation proof is not a source path: ${proof.path}`);
      continue;
    }
    const source = readTrackedText(proof.path);
    if (!symbolParts(proof.symbol) || !codeDeclaresSymbol(source, proof.symbol)) {
      fail(errors, `${owner} implementation symbol ${proof.symbol} is not a code declaration in ${proof.path}`);
      continue;
    }
    validated.push(proof);
  }
  return validated;
}

function validateTestProofs(owner, values, implementation, artifacts, errors) {
  if (!Array.isArray(values) || values.length === 0) {
    fail(errors, `${owner} has no test proof`);
    return [];
  }
  const registry = cmakeTestRegistry();
  const validated = [];
  for (const proof of values) {
    if (!exactKeys(proof, ["path", "executable", "case"]) ||
      typeof proof.path !== "string" || typeof proof.executable !== "string" ||
      typeof proof.case !== "string" || !/^[A-Za-z_]\w*$/u.test(proof.executable) ||
      !/^[A-Za-z_]\w*$/u.test(proof.case)) {
      fail(errors, `${owner} has a malformed test proof`);
      continue;
    }
    if (!artifacts.has(proof.path)) {
      fail(errors, `${owner} test proof is not one of its artifacts: ${proof.path}`);
      continue;
    }
    const info = trackedArtifact(proof.path);
    if (!info.ok) {
      fail(errors, `${owner} test proof is ${info.reason}: ${proof.path}`);
      continue;
    }
    if (!isTestPath(proof.path)) {
      fail(errors, `${owner} test proof is not a test/fixture path: ${proof.path}`);
      continue;
    }
    const source = readTrackedText(proof.path);
    const body = functionBody(source, proof.case);
    if (body === undefined) {
      fail(errors, `${owner} test case ${proof.case} is not defined in ${proof.path}`);
      continue;
    }
    if (!(registry.targets.get(proof.executable)?.has(proof.path))) {
      fail(errors, `${owner} test executable ${proof.executable} does not build ${proof.path}`);
    }
    if (!registry.registered.has(proof.executable)) {
      fail(errors, `${owner} test executable ${proof.executable} is not registered`);
    }
    if (proof.case !== "main") {
      const main = functionBody(source, "main");
      if (main === undefined ||
        !new RegExp(`\\b${escapeRegExp(proof.case)}\\s*\\(`, "u").test(main)) {
        fail(errors, `${owner} test case ${proof.case} is not called by main`);
      }
    }
    for (const implementationProof of implementation) {
      if (!codeContainsSymbol(body, implementationProof.symbol)) {
        fail(errors, `${owner} test case ${proof.case} does not exercise ${implementationProof.symbol}`);
      }
    }
    validated.push(proof);
  }
  return validated;
}

function validateProofBundle(owner, proof, artifacts, identityKey, identityValue, errors) {
  if (!exactKeys(proof, [identityKey, "implementation", "tests"]) ||
    proof?.[identityKey] !== identityValue) {
    fail(errors, `${owner} proof must identify exactly ${identityValue}`);
    return false;
  }
  const implementation = validateImplementationProofs(owner, proof.implementation, artifacts, errors);
  const tests = validateTestProofs(owner, proof.tests, implementation, artifacts, errors);
  return implementation.length > 0 && tests.length > 0;
}

function validateBinding(binding, packageName, moduleNames, errors) {
  if (!exactKeys(binding, ["selector", "modules", "kind", "class", "symbol"]) ||
    typeof binding.selector !== "string" || !Array.isArray(binding.modules) ||
    binding.modules.length === 0 || binding.modules.some((module) => typeof module !== "string") ||
    !bindingKinds.has(binding.kind) ||
    (binding.class !== null && typeof binding.class !== "string") ||
    (binding.symbol !== null && typeof binding.symbol !== "string")) {
    fail(errors, `${packageName} has a malformed selector binding`);
    return;
  }
  if (new Set(binding.modules).size !== binding.modules.length) {
    fail(errors, `${packageName} selector ${binding.selector} repeats a target module`);
  }
  for (const module of binding.modules) {
    if (!moduleNames.has(module)) {
      fail(errors, `${packageName} selector ${binding.selector} binds unknown module ${module}`);
    }
  }
}

function validateSelectorBindings(actual, expected, moduleNames, errors) {
  if (!Array.isArray(actual.selector_bindings)) {
    fail(errors, `${actual.package} has no selector bindings`);
    return;
  }
  const seen = new Set();
  for (const binding of actual.selector_bindings) {
    validateBinding(binding, actual.package, moduleNames, errors);
    if (typeof binding?.selector === "string") {
      if (seen.has(binding.selector)) fail(errors, `${actual.package} repeats selector binding ${binding.selector}`);
      seen.add(binding.selector);
    }
  }
  if (!sameValues([...seen], expected.declared_variants)) {
    fail(errors, `${actual.package} selector bindings do not exactly match catalog selectors`);
  }
  if (!sameJson(actual.selector_bindings, expected.selector_bindings)) {
    fail(errors, `${actual.package} selector bindings differ from the pinned catalog`);
  }
}

function expectedModulePath(module) {
  return `${module.module.replaceAll(".", "/")}.py`;
}

function validateCatalog(catalog, errors) {
  if (!catalog || typeof catalog !== "object" || !Array.isArray(catalog.modules) ||
    !Array.isArray(catalog.algorithm_packages) || !Array.isArray(catalog.dispatcher_modules) ||
    !Array.isArray(catalog.variant_modules)) {
    fail(errors, "catalog has a malformed top-level structure");
    return undefined;
  }
  const modules = new Map();
  const byKind = new Map([["dispatcher", []], ["variant", []], ["support", []]]);
  for (const entry of catalog.modules) {
    if (!entry || typeof entry.module !== "string" || typeof entry.path !== "string" ||
      !byKind.has(entry.kind)) {
      fail(errors, "catalog contains a malformed module entry");
      continue;
    }
    if (modules.has(entry.module)) fail(errors, `catalog repeats module ${entry.module}`);
    modules.set(entry.module, entry);
    byKind.get(entry.kind).push(entry.module);
    if (entry.path !== expectedModulePath(entry) || !entry.path.startsWith("algo/") ||
      entry.path.endsWith("/__init__.py")) {
      fail(errors, `${entry.module} has inconsistent module path ${entry.path}`);
    }
    const kind = entry.path.endsWith("/algorithm.py")
      ? "dispatcher"
      : entry.path.split("/").includes("variants") ? "variant" : "support";
    if (entry.kind !== kind) fail(errors, `${entry.module} has kind ${entry.kind}, expected ${kind}`);
  }
  if (!sameValues(catalog.dispatcher_modules, byKind.get("dispatcher"))) {
    fail(errors, "catalog.dispatcher_modules is not the exact dispatcher set");
  }
  if (!sameValues(catalog.variant_modules, byKind.get("variant"))) {
    fail(errors, "catalog.variant_modules is not the exact variant set");
  }
  const packages = new Map();
  for (const entry of catalog.algorithm_packages) {
    if (!entry || typeof entry.package !== "string" || !Array.isArray(entry.declared_variants) ||
      !Array.isArray(entry.selector_bindings) || entry.declared_variants.length === 0) {
      fail(errors, "catalog contains a malformed dispatcher entry");
      continue;
    }
    if (packages.has(entry.package)) fail(errors, `catalog repeats package ${entry.package}`);
    packages.set(entry.package, entry);
    if (!sameValues(entry.declared_variants, entry.selector_bindings.map((binding) => binding?.selector))) {
      fail(errors, `${entry.package} selectors and bindings differ`);
    }
    for (const binding of entry.selector_bindings) validateBinding(binding, entry.package, modules, errors);
  }
  const expectedPackages = byKind.get("dispatcher").map((module) => module.slice(0, -".algorithm".length));
  if (!sameValues([...packages.keys()], expectedPackages)) {
    fail(errors, "catalog algorithm packages are not the exact dispatcher set");
  }
  const expectedCounts = {
    algorithm_packages: packages.size,
    dispatcher_modules: byKind.get("dispatcher").length,
    variant_modules: byKind.get("variant").length,
    support_modules: byKind.get("support").length,
    non_init_algo_modules: modules.size,
  };
  for (const [name, value] of Object.entries(expectedCounts)) {
    if (catalog.counts?.[name] !== value) fail(errors, `catalog count ${name} is not ${value}`);
  }
  return { modules, byKind, packages };
}

function validateOwnership(manifest, catalogInfo, manifestPackages, errors) {
  const records = manifest.module_ownership?.supplemental_prefixes;
  if (manifest.module_ownership?.package_prefixes !== true || !Array.isArray(records)) {
    fail(errors, "module_ownership must enable package_prefixes and list supplemental_prefixes");
    return undefined;
  }
  const prefixes = [];
  const owners = new Set();
  for (const record of records) {
    if (!exactKeys(record, ["prefix", "owner"]) || typeof record.prefix !== "string" ||
      typeof record.owner !== "string" || !record.prefix || !record.owner.startsWith("support:")) {
      fail(errors, "module_ownership contains a malformed supplemental prefix");
      continue;
    }
    if (prefixes.includes(record.prefix)) fail(errors, `module_ownership repeats prefix ${record.prefix}`);
    if (owners.has(record.owner)) fail(errors, `module_ownership repeats owner ${record.owner}`);
    prefixes.push(record.prefix);
    owners.add(record.owner);
  }
  const candidates = [
    ...[...catalogInfo.packages.keys()].map((prefix) => ({ prefix, owner: prefix })),
    ...records,
  ];
  const hits = new Map(records.map((record) => [record.prefix, 0]));
  const ownerByModule = new Map();
  for (const module of catalogInfo.modules.keys()) {
    const matches = candidates.filter((record) =>
      module === record.prefix || module.startsWith(`${record.prefix}.`));
    if (matches.length === 0) {
      fail(errors, `unowned catalog module ${module}`);
      continue;
    }
    const longest = Math.max(...matches.map((record) => record.prefix.length));
    const winners = matches.filter((record) => record.prefix.length === longest);
    if (new Set(winners.map((record) => record.owner)).size !== 1) {
      fail(errors, `ambiguous owners for ${module}`);
      continue;
    }
    const winner = winners[0];
    ownerByModule.set(module, winner.owner);
    if (hits.has(winner.prefix)) hits.set(winner.prefix, hits.get(winner.prefix) + 1);
    if (!winner.owner.startsWith("support:") && !manifestPackages.has(winner.owner)) {
      fail(errors, `${module} resolves to unknown package owner ${winner.owner}`);
    }
  }
  for (const [prefix, count] of hits) {
    if (count === 0) fail(errors, `module ownership prefix is stale or unreachable: ${prefix}`);
  }
  return { ownerByModule };
}

function expectedModuleSelectors(catalogInfo) {
  const result = new Map([...catalogInfo.modules.keys()].map((module) => [module, []]));
  for (const entry of catalogInfo.packages.values()) {
    for (const binding of entry.selector_bindings) {
      for (const module of binding.modules) {
        result.get(module)?.push({ package: entry.package, selector: binding.selector });
      }
    }
  }
  for (const selectors of result.values()) {
    selectors.sort((left, right) =>
      left.package.localeCompare(right.package) || left.selector.localeCompare(right.selector));
  }
  return result;
}

function validatePackageProofs(entry, expected, artifacts, errors) {
  if (entry.proofs === undefined) {
    if (entry.status === "complete") fail(errors, `${entry.package} claims complete without selector proofs`);
    return false;
  }
  if (!entry.proofs || typeof entry.proofs !== "object" || Array.isArray(entry.proofs) ||
    !sameValues(Object.keys(entry.proofs), expected.declared_variants)) {
    fail(errors, `${entry.package} selector proofs do not exactly match catalog selectors`);
    return false;
  }
  let valid = true;
  for (const binding of expected.selector_bindings) {
    const proof = entry.proofs[binding.selector];
    if (!exactKeys(proof, ["selector", "binding", "implementation", "tests"]) ||
      proof.selector !== binding.selector || !sameJson(proof.binding, binding)) {
      fail(errors, `${entry.package} selector ${binding.selector} proof is not tied to its exact binding`);
      valid = false;
      continue;
    }
    if (!validateProofBundle(
      `${entry.package} selector ${binding.selector}`,
      { selector: proof.selector, implementation: proof.implementation, tests: proof.tests },
      artifacts,
      "selector",
      binding.selector,
      errors,
    )) valid = false;
  }
  return valid;
}

function validatePackages(manifest, catalogInfo, errors) {
  if (!Array.isArray(manifest.packages)) {
    fail(errors, "manifest.packages must be an array");
    return new Map();
  }
  const entries = new Map();
  for (const entry of manifest.packages) {
    if (!entry || typeof entry.package !== "string" || !Array.isArray(entry.variants)) {
      fail(errors, "manifest contains a malformed package entry");
      continue;
    }
    if (entries.has(entry.package)) fail(errors, `manifest repeats package ${entry.package}`);
    entries.set(entry.package, entry);
    const expected = catalogInfo.packages.get(entry.package);
    if (!expected) {
      fail(errors, `stale package ${entry.package}`);
      continue;
    }
    if (!sameJson(entry.variants, expected.declared_variants)) {
      fail(errors, `${entry.package} variants differ from the pinned catalog`);
    }
    if (new Set(entry.variants).size !== entry.variants.length) {
      fail(errors, `${entry.package} repeats a variant`);
    }
    if (!statuses.has(entry.status)) fail(errors, `${entry.package} has invalid status ${entry.status}`);
    validateEvidence(entry.package, entry.evidence, errors);
    validateSelectorBindings(entry, expected, catalogInfo.modules, errors);
    const artifacts = validateArtifactList(entry.package, entry.artifacts, entry.status === "complete", errors);
    validatePackageProofs(entry, expected, artifacts, errors);
  }
  for (const packageName of catalogInfo.packages.keys()) {
    if (!entries.has(packageName)) fail(errors, `unmapped package ${packageName}`);
  }
  const artifactRequired = manifest.artifact_required_packages ?? [];
  if (!Array.isArray(artifactRequired) || new Set(artifactRequired).size !== artifactRequired.length) {
    fail(errors, "artifact_required_packages must be a unique array");
  } else {
    for (const packageName of artifactRequired) {
      const entry = entries.get(packageName);
      if (!entry) fail(errors, `artifact requirement names unknown package ${packageName}`);
      else if (!Array.isArray(entry.artifacts) || entry.artifacts.length === 0) {
        fail(errors, `${packageName} requires an artifact list`);
      }
    }
  }
  return entries;
}

function validateModuleCoverage(manifest, catalogInfo, ownership, errors) {
  const expectedEntries = [...catalogInfo.modules.values()].filter((entry) => entry.kind !== "dispatcher");
  const selectors = expectedModuleSelectors(catalogInfo);
  if (!Array.isArray(manifest.module_coverage)) {
    fail(errors, "module_coverage must be an array");
    return new Map();
  }
  const actual = new Map();
  const implementationClaims = new Map();
  const testClaims = new Map();
  for (const entry of manifest.module_coverage) {
    if (!entry || typeof entry.module !== "string") {
      fail(errors, "module_coverage contains a malformed entry");
      continue;
    }
    if (actual.has(entry.module)) fail(errors, `module_coverage repeats ${entry.module}`);
    actual.set(entry.module, entry);
    const expected = catalogInfo.modules.get(entry.module);
    if (!expected || expected.kind === "dispatcher") {
      fail(errors, `module_coverage names a dispatcher or unknown module ${entry.module}`);
      continue;
    }
    const expectedSelectors = selectors.get(entry.module) ?? [];
    const classification = expectedSelectors.length > 0
      ? "selector-target"
      : expected.kind === "variant" ? "internal-variant" : "support";
    const owner = ownership?.ownerByModule.get(entry.module);
    if (entry.kind !== expected.kind) fail(errors, `${entry.module} coverage kind must be ${expected.kind}`);
    if (entry.owner !== owner) fail(errors, `${entry.module} coverage owner must be ${owner ?? "unresolved"}`);
    if (entry.classification !== classification) {
      fail(errors, `${entry.module} coverage classification must be ${classification}`);
    }
    if (!sameJson(entry.selectors, expectedSelectors)) {
      fail(errors, `${entry.module} selector references differ from the pinned bindings`);
    }
    if (!statuses.has(entry.status)) fail(errors, `${entry.module} has invalid status ${entry.status}`);
    validateEvidence(entry.module, entry.evidence, errors);
    const artifacts = validateArtifactList(entry.module, entry.artifacts, entry.status === "complete", errors);
    if (entry.proofs === undefined) {
      if (entry.status === "complete") fail(errors, `${entry.module} claims complete without module-specific proofs`);
    } else {
      validateProofBundle(entry.module, entry.proofs, artifacts, "module", entry.module, errors);
      for (const proof of Array.isArray(entry.proofs?.implementation) ? entry.proofs.implementation : []) {
        const claim = `${proof?.path}\0${proof?.symbol}`;
        const prior = implementationClaims.get(claim);
        if (prior && prior !== entry.module) {
          fail(errors, `${entry.module} reuses module-specific implementation proof from ${prior}`);
        } else implementationClaims.set(claim, entry.module);
      }
      for (const proof of Array.isArray(entry.proofs?.tests) ? entry.proofs.tests : []) {
        const claim = `${proof?.path}\0${proof?.executable}\0${proof?.case}`;
        const prior = testClaims.get(claim);
        if (prior && prior !== entry.module) {
          fail(errors, `${entry.module} reuses module-specific test proof from ${prior}`);
        } else testClaims.set(claim, entry.module);
      }
    }
  }
  for (const expected of expectedEntries) {
    if (!actual.has(expected.module)) fail(errors, `unclassified non-dispatcher module ${expected.module}`);
  }
  return actual;
}

function main() {
  const options = parseArguments(process.argv.slice(2));
  projectRoot = options.projectRoot;
  artifactCache.clear();
  textCache.clear();
  testRegistryCache = undefined;
  let manifest;
  try {
    manifest = JSON.parse(readFileSync(options.manifestPath, "utf8"));
  } catch (error) {
    throw new Error(`could not read manifest ${options.manifestPath}: ${error.message}`);
  }
  const catalog = runAudit(options);
  const errors = [];
  if (manifest.target?.tag !== catalog.tag) fail(errors, "manifest target tag differs from catalog");
  if (manifest.target?.commit !== catalog.commit) fail(errors, "manifest target commit differs from catalog");
  const catalogInfo = validateCatalog(catalog, errors);
  let packageEntries = new Map();
  let moduleEntries = new Map();
  if (catalogInfo) {
    packageEntries = validatePackages(manifest, catalogInfo, errors);
    const ownership = validateOwnership(manifest, catalogInfo, packageEntries, errors);
    moduleEntries = validateModuleCoverage(manifest, catalogInfo, ownership, errors);
    if (options.requireComplete) {
      const incompletePackages = [...catalogInfo.packages.keys()].filter((name) =>
        packageEntries.get(name)?.status !== "complete");
      const incompleteModules = [...catalogInfo.modules.values()]
        .filter((entry) => entry.kind !== "dispatcher" && moduleEntries.get(entry.module)?.status !== "complete")
        .map((entry) => entry.module);
      if (incompletePackages.length > 0 || incompleteModules.length > 0) {
        fail(errors,
          `--require-complete gate rejected ${incompletePackages.length + incompleteModules.length} ` +
          `incomplete coverage units: ${[...incompletePackages, ...incompleteModules].join(", ")}`,
        );
      }
    }
  }
  if (errors.length > 0) throw new Error(errors.join("\n"));
  const packageCounts = {};
  for (const entry of packageEntries.values()) {
    packageCounts[entry.status] = (packageCounts[entry.status] ?? 0) + 1;
  }
  const moduleCounts = {};
  for (const entry of moduleEntries.values()) {
    moduleCounts[entry.status] = (moduleCounts[entry.status] ?? 0) + 1;
  }
  const renderCounts = (counts) => Object.entries(counts)
    .sort(([left], [right]) => left.localeCompare(right))
    .map(([status, count]) => `${count} ${status}`)
    .join(", ");
  process.stdout.write(
    `verified ${packageEntries.size} dispatcher packages (${renderCounts(packageCounts)}) and ` +
    `${moduleEntries.size} non-dispatcher modules (${renderCounts(moduleCounts)})` +
    `${options.requireComplete ? " (complete release gate)" : ""}\n`,
  );
}

try {
  main();
} catch (error) {
  process.stderr.write(`parity verification failed: ${error.message}\n`);
  process.exitCode = 1;
}
