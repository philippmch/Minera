#!/usr/bin/env node

import { execFileSync, spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

if (process.argv.length !== 3) {
  throw new Error("usage: test-reference-parity-verifier.mjs CHECKOUT");
}

const repositoryRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const audit = resolve(repositoryRoot, "scripts", "audit-reference-catalog.mjs");
const verifier = resolve(repositoryRoot, "scripts", "verify-reference-parity.mjs");
const checkout = resolve(process.argv[2]);

function runNode(script, args) {
  return execFileSync(process.execPath, [script, ...args], { encoding: "utf8" });
}

function runVerifier(args) {
  return spawnSync(process.execPath, [verifier, ...args], { encoding: "utf8" });
}

function expectSuccess(args) {
  const result = runVerifier(args);
  if (result.status !== 0) {
    throw new Error(`expected verifier success: ${result.stderr || result.stdout}`);
  }
}

function expectFailure(args, expected) {
  const result = runVerifier(args);
  const output = `${result.stdout ?? ""}${result.stderr ?? ""}`;
  if (result.status === 0 || !output.includes(expected)) {
    throw new Error(`expected verifier failure containing ${expected}: ${output}`);
  }
}

function git(directory, args) {
  return execFileSync("git", ["-C", directory, ...args], {
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  }).trim();
}

function write(path, value) {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, value, "utf8");
}

function writeJson(path, value) {
  write(path, `${JSON.stringify(value, null, 2)}\n`);
}

function clone(value) {
  return JSON.parse(JSON.stringify(value));
}

function demoEntry(manifest) {
  const entry = manifest.packages.find((candidate) => candidate.package === "algo.demo");
  if (!entry) throw new Error("fixture manifest has no algo.demo package");
  return entry;
}

function binding(catalog, packageName, selector) {
  return catalog.algorithm_packages
    .find((entry) => entry.package === packageName)?.selector_bindings
    .find((entry) => entry.selector === selector);
}

function assertModules(catalog, packageName, selector, modules) {
  const actual = binding(catalog, packageName, selector)?.modules;
  if (JSON.stringify(actual) !== JSON.stringify(modules)) {
    throw new Error(
      `unexpected binding for ${packageName}.${selector}: ${JSON.stringify(actual)}`,
    );
  }
}

const realCatalog = JSON.parse(runNode(audit, [checkout]));
const expectedCounts = {
  domains: 18,
  algorithm_packages: 86,
  dispatcher_modules: 86,
  variant_modules: 182,
  support_modules: 205,
  non_init_algo_modules: 473,
  algo_python_files: 779,
  object_python_files: 348,
  statistics_python_files: 156,
  visualization_python_files: 142,
  streaming_python_files: 54,
  util_python_files: 40,
};
for (const [name, expected] of Object.entries(expectedCounts)) {
  if (realCatalog.counts[name] !== expected) {
    throw new Error(`unexpected pinned catalog count ${name}: ${realCatalog.counts[name]} != ${expected}`);
  }
}
if (realCatalog.algorithm_packages.reduce(
  (total, entry) => total + entry.declared_variants.length,
  0,
) !== 170) throw new Error("selector inventory did not include all pinned selectors");

assertModules(realCatalog, "algo.discovery.inductive", "IM", [
  "algo.discovery.inductive.variants.instances",
]);
assertModules(realCatalog, "algo.discovery.inductive", "IMf", [
  "algo.discovery.inductive.variants.instances",
]);
assertModules(realCatalog, "algo.clustering.trace_attribute_driven", "DFG", [
  "algo.clustering.trace_attribute_driven.util.evaluation",
]);
assertModules(realCatalog, "algo.discovery.batches", "POLARS", [
  "algo.discovery.batches.variants.polars",
  "algo.discovery.batches.variants.log",
]);
assertModules(realCatalog, "algo.discovery.minimum_self_distance", "POLARS", [
  "algo.discovery.minimum_self_distance.variants.polars",
  "algo.discovery.minimum_self_distance.variants.log",
]);
assertModules(realCatalog, "algo.discovery.footprints", "POLARS_LAZYFRAMES", [
  "algo.discovery.footprints.log.variants.polars_lazyframes",
  "algo.discovery.footprints.log.variants.entire_dataframe",
]);
assertModules(realCatalog, "algo.discovery.alpha", "ALPHA_VERSION_CLASSIC", [
  "algo.discovery.alpha.variants.classic",
]);

function makeReference(directory) {
  const root = join(directory, "reference_pkg");
  const files = {
    "algo/__init__.py": "",
    "objects/__init__.py": "",
    "algo/demo/__init__.py": "",
    "algo/demo/algorithm.py": [
      "from enum import Enum",
      "from reference_pkg.algo.demo.variants import first, fallback",
      "if True:",
      "    from reference_pkg.algo.demo.variants import conditional",
      "class Variants(Enum):",
      "    FIRST = first",
      "    CONDITIONAL = conditional if ENABLED else fallback",
      "",
    ].join("\n"),
    "algo/demo/helper.py": "VALUE = 1\n",
    "algo/demo/variants/__init__.py": "",
    "algo/demo/variants/first.py": "VALUE = 1\n",
    "algo/demo/variants/fallback.py": "VALUE = 1\n",
    "algo/demo/variants/conditional.py": "VALUE = 1\n",
    "algo/demo/variants/internal.py": "VALUE = 1\n",
    "algo/member/__init__.py": "",
    "algo/member/algorithm.py": [
      "from enum import Enum",
      "from reference_pkg.algo.member.variants.instances import Instances",
      "class Variants(Enum):",
      "    MEMBER = Instances.MEMBER",
      "",
    ].join("\n"),
    "algo/member/variants/__init__.py": "",
    "algo/member/variants/instances.py": "from enum import Enum\nclass Instances(Enum):\n    MEMBER = 1\n",
    "algo/cluster/__init__.py": "",
    "algo/cluster/algorithm.py": [
      "from enum import Enum",
      "from reference_pkg.algo.cluster.util import evaluation",
      "class Variants(Enum):",
      "    DISTANCE = evaluation.distance",
      "",
    ].join("\n"),
    "algo/cluster/util/__init__.py": "",
    "algo/cluster/util/evaluation.py": "def distance():\n    return 0\n",
    "algo/support/__init__.py": "",
    "algo/support/helper.py": "VALUE = 1\n",
  };
  for (const [path, contents] of Object.entries(files)) write(join(root, path), contents);
  git(directory, ["init", "-q"]);
  git(directory, ["config", "user.email", "parity-fixture@example.invalid"]);
  git(directory, ["config", "user.name", "Parity Fixture"]);
  git(directory, ["add", "."]);
  git(directory, ["commit", "-qm", "reference fixture"]);
  git(directory, ["tag", "2.7.22"]);
  return git(directory, ["rev-parse", "HEAD"]);
}

function makeProject(directory) {
  write(join(directory, "CMakeLists.txt"), [
    "cmake_minimum_required(VERSION 3.20)",
    "project(parity_fixture)",
    "enable_testing()",
    "add_executable(demo_tests tests/test_demo.cpp)",
    "add_test(NAME demo_tests COMMAND demo_tests)",
    "add_executable(module_tests tests/test_modules.cpp)",
    "add_test(NAME module_tests COMMAND module_tests)",
    "add_executable(other_tests tests/test_other.cpp)",
    "add_test(NAME other_tests COMMAND other_tests)",
    "add_executable(unregistered_tests tests/test_unregistered.cpp)",
    "",
  ].join("\n"));
  write(join(directory, "src", "demo.cpp"), [
    "int run_first() { return 1; }",
    "int run_conditional() { return 2; }",
    "int run_member() { return 3; }",
    "int run_distance() { return 4; }",
    "// wrong_symbol exists only in a comment.",
    "const char* string_anchor = \"string_only\";",
    "",
  ].join("\n"));
  write(join(directory, "tests", "test_demo.cpp"), [
    "int run_first(); int run_conditional(); int run_member(); int run_distance();",
    "void first_case() { (void)run_first(); }",
    "void conditional_case() { (void)run_conditional(); }",
    "void member_case() { (void)run_member(); }",
    "void distance_case() { (void)run_distance(); }",
    "int main() { first_case(); conditional_case(); member_case(); distance_case(); }",
    "",
  ].join("\n"));
  const modules = [
    "demo_first", "demo_fallback", "demo_conditional", "demo_internal",
    "demo_helper", "member_instances", "cluster_evaluation", "support_helper",
  ];
  write(join(directory, "src", "modules.cpp"),
    `${modules.map((name, index) => `int module_${name}() { return ${index}; }`).join("\n")}\n`);
  write(join(directory, "tests", "test_modules.cpp"), [
    modules.map((name) => `int module_${name}();`).join(" "),
    ...modules.map((name) => `void module_${name}_case() { (void)module_${name}(); }`),
    `int main() { ${modules.map((name) => `module_${name}_case();`).join(" ")} }`,
    "",
  ].join("\n"));
  write(join(directory, "src", "other.cpp"), "int run_other() { return 9; }\n");
  write(join(directory, "tests", "test_other.cpp"),
    "int run_other();\nvoid other_case() { (void)run_other(); }\nint main() { other_case(); }\n");
  write(join(directory, "tests", "test_unregistered.cpp"),
    "int run_first();\nvoid unregistered_case() { (void)run_first(); }\nint main() { unregistered_case(); }\n");
  write(join(directory, "docs", "parity.md"), "Structural fixture evidence.\n");
  git(directory, ["init", "-q"]);
  git(directory, ["config", "user.email", "parity-project@example.invalid"]);
  git(directory, ["config", "user.name", "Parity Project"]);
  git(directory, ["add", "."]);
  git(directory, ["commit", "-qm", "project fixture"]);
}

const packageProofs = {
  "algo.demo": {
    FIRST: ["run_first", "first_case"],
    CONDITIONAL: ["run_conditional", "conditional_case"],
  },
  "algo.member": { MEMBER: ["run_member", "member_case"] },
  "algo.cluster": { DISTANCE: ["run_distance", "distance_case"] },
};
const moduleProofs = {
  "algo.demo.variants.first": "demo_first",
  "algo.demo.variants.fallback": "demo_fallback",
  "algo.demo.variants.conditional": "demo_conditional",
  "algo.demo.variants.internal": "demo_internal",
  "algo.demo.helper": "demo_helper",
  "algo.member.variants.instances": "member_instances",
  "algo.cluster.util.evaluation": "cluster_evaluation",
  "algo.support.helper": "support_helper",
};

function ownerFor(module, catalog) {
  const supplemental = [{ prefix: "algo.support", owner: "support:fixture" }];
  const candidates = [
    ...catalog.algorithm_packages.map((entry) => ({ prefix: entry.package, owner: entry.package })),
    ...supplemental,
  ].filter((entry) => module === entry.prefix || module.startsWith(`${entry.prefix}.`));
  candidates.sort((left, right) => right.prefix.length - left.prefix.length);
  return candidates[0]?.owner;
}

function completeManifest(catalog, commit) {
  const selectorReferences = new Map(catalog.modules.map((entry) => [entry.module, []]));
  for (const entry of catalog.algorithm_packages) {
    for (const selectorBinding of entry.selector_bindings) {
      for (const module of selectorBinding.modules) {
        selectorReferences.get(module)?.push({ package: entry.package, selector: selectorBinding.selector });
      }
    }
  }
  for (const values of selectorReferences.values()) {
    values.sort((left, right) =>
      left.package.localeCompare(right.package) || left.selector.localeCompare(right.selector));
  }
  return {
    target: { tag: "2.7.22", commit },
    module_ownership: {
      package_prefixes: true,
      supplemental_prefixes: [{ prefix: "algo.support", owner: "support:fixture" }],
    },
    artifact_required_packages: catalog.algorithm_packages.map((entry) => entry.package),
    packages: catalog.algorithm_packages.map((entry) => ({
      package: entry.package,
      variants: entry.declared_variants,
      selector_bindings: entry.selector_bindings,
      status: "complete",
      evidence: "docs/parity.md",
      artifacts: ["src/demo.cpp", "tests/test_demo.cpp"],
      proofs: Object.fromEntries(entry.selector_bindings.map((selectorBinding) => {
        const [symbol, caseName] = packageProofs[entry.package][selectorBinding.selector];
        return [selectorBinding.selector, {
          selector: selectorBinding.selector,
          binding: selectorBinding,
          implementation: [{ path: "src/demo.cpp", symbol }],
          tests: [{ path: "tests/test_demo.cpp", executable: "demo_tests", case: caseName }],
        }];
      })),
    })),
    module_coverage: catalog.modules
      .filter((entry) => entry.kind !== "dispatcher")
      .map((entry) => {
        const reference = moduleProofs[entry.module];
        const selectors = selectorReferences.get(entry.module) ?? [];
        return {
          module: entry.module,
          kind: entry.kind,
          owner: ownerFor(entry.module, catalog),
          classification: selectors.length > 0
            ? "selector-target"
            : entry.kind === "variant" ? "internal-variant" : "support",
          selectors,
          status: "complete",
          evidence: "docs/parity.md",
          artifacts: ["src/modules.cpp", "tests/test_modules.cpp"],
          proofs: {
            module: entry.module,
            implementation: [{ path: "src/modules.cpp", symbol: `module_${reference}` }],
            tests: [{
              path: "tests/test_modules.cpp",
              executable: "module_tests",
              case: `module_${reference}_case`,
            }],
          },
        };
      }),
  };
}

const tempRoot = mkdtempSync(join(tmpdir(), `minera-parity-${process.pid}-`));
const expectedPrefix = resolve(tmpdir(), `minera-parity-${process.pid}-`);
if (!resolve(tempRoot).startsWith(expectedPrefix)) {
  throw new Error(`refusing unsafe fixture directory: ${tempRoot}`);
}
try {
  const reference = join(tempRoot, "reference");
  const project = join(tempRoot, "project");
  mkdirSync(reference, { recursive: true });
  mkdirSync(project, { recursive: true });
  const commit = makeReference(reference);
  makeProject(project);
  const catalog = JSON.parse(runNode(audit, [
    reference, "--expected-tag", "2.7.22", "--expected-commit", commit,
  ]));
  assertModules(catalog, "algo.demo", "CONDITIONAL", [
    "algo.demo.variants.conditional", "algo.demo.variants.fallback",
  ]);
  assertModules(catalog, "algo.member", "MEMBER", ["algo.member.variants.instances"]);
  assertModules(catalog, "algo.cluster", "DISTANCE", ["algo.cluster.util.evaluation"]);

  const manifest = completeManifest(catalog, commit);
  const manifestPath = join(project, "manifest.json");
  writeJson(manifestPath, manifest);
  const args = (path = manifestPath) => [
    reference,
    "--manifest", path,
    "--project-root", project,
    "--expected-tag", "2.7.22",
    "--expected-commit", commit,
  ];
  expectSuccess(args());
  expectSuccess([...args(), "--require-complete"]);

  const cleanCatalog = JSON.stringify(catalog);
  rmSync(join(reference, "reference_pkg", "algo", "demo", "algorithm.py"));
  git(reference, ["add", "-u"]);
  write(join(reference, "reference_pkg", "algo", "demo", "algorithm.py"),
    "class Variants:\n    WORKTREE_ONLY = 1\n");
  write(join(reference, "reference_pkg", "algo", "demo", "variants", "untracked.py"), "VALUE = 1\n");
  const dirtyCatalog = runNode(audit, [
    reference, "--expected-tag", "2.7.22", "--expected-commit", commit,
  ]).trim();
  if (JSON.stringify(JSON.parse(dirtyCatalog)) !== cleanCatalog) {
    throw new Error("catalog changed when only the reference working tree changed");
  }

  const selectorDrift = clone(manifest);
  demoEntry(selectorDrift).variants.push("STALE");
  writeJson(join(project, "selector-drift.json"), selectorDrift);
  expectFailure(args(join(project, "selector-drift.json")), "variants differ");

  const bindingDrift = clone(manifest);
  demoEntry(bindingDrift).selector_bindings[0].modules = ["algo.demo.algorithm"];
  writeJson(join(project, "binding-drift.json"), bindingDrift);
  expectFailure(args(join(project, "binding-drift.json")), "bindings differ from the pinned catalog");

  const crossOwner = clone(manifest);
  demoEntry(crossOwner).proofs.FIRST.implementation = [{ path: "src/other.cpp", symbol: "run_other" }];
  writeJson(join(project, "cross-owner.json"), crossOwner);
  expectFailure(args(join(project, "cross-owner.json")), "not one of its artifacts");

  const wrongSymbol = clone(manifest);
  demoEntry(wrongSymbol).proofs.FIRST.implementation[0].symbol = "wrong_symbol";
  writeJson(join(project, "wrong-symbol.json"), wrongSymbol);
  expectFailure(args(join(project, "wrong-symbol.json")), "is not a code declaration");

  const stringOnly = clone(manifest);
  demoEntry(stringOnly).proofs.FIRST.implementation[0].symbol = "string_only";
  writeJson(join(project, "string-only.json"), stringOnly);
  expectFailure(args(join(project, "string-only.json")), "is not a code declaration");

  const unregistered = clone(manifest);
  demoEntry(unregistered).proofs.FIRST.tests[0] = {
    path: "tests/test_unregistered.cpp",
    executable: "unregistered_tests",
    case: "unregistered_case",
  };
  demoEntry(unregistered).artifacts.push("tests/test_unregistered.cpp");
  writeJson(join(project, "unregistered.json"), unregistered);
  expectFailure(args(join(project, "unregistered.json")), "is not registered");

  write(join(project, "src", "untracked.cpp"), "int run_untracked() { return 0; }\n");
  const untracked = clone(manifest);
  demoEntry(untracked).artifacts.push("src/untracked.cpp");
  demoEntry(untracked).proofs.FIRST.implementation[0] = {
    path: "src/untracked.cpp", symbol: "run_untracked",
  };
  writeJson(join(project, "untracked.json"), untracked);
  expectFailure(args(join(project, "untracked.json")), "not tracked");

  const docsOnly = clone(manifest);
  demoEntry(docsOnly).artifacts.push("docs/parity.md");
  demoEntry(docsOnly).proofs.FIRST.implementation[0] = {
    path: "docs/parity.md", symbol: "Structural",
  };
  writeJson(join(project, "docs-only.json"), docsOnly);
  expectFailure(args(join(project, "docs-only.json")), "documentation-only");

  const missingModule = clone(manifest);
  missingModule.module_coverage.pop();
  writeJson(join(project, "missing-module.json"), missingModule);
  expectFailure(args(join(project, "missing-module.json")), "unclassified non-dispatcher module");

  const wrongModuleIdentity = clone(manifest);
  wrongModuleIdentity.module_coverage[0].proofs.module = "algo.wrong.module";
  writeJson(join(project, "wrong-module-identity.json"), wrongModuleIdentity);
  expectFailure(args(join(project, "wrong-module-identity.json")), "proof must identify exactly");

  const reusedModuleProof = clone(manifest);
  reusedModuleProof.module_coverage[1].proofs = clone(reusedModuleProof.module_coverage[0].proofs);
  reusedModuleProof.module_coverage[1].proofs.module = reusedModuleProof.module_coverage[1].module;
  writeJson(join(project, "reused-module-proof.json"), reusedModuleProof);
  expectFailure(args(join(project, "reused-module-proof.json")), "reuses module-specific implementation proof");

  const partial = clone(manifest);
  demoEntry(partial).status = "partial";
  delete demoEntry(partial).proofs;
  writeJson(join(project, "partial.json"), partial);
  expectSuccess(args(join(project, "partial.json")));
  expectFailure([...args(join(project, "partial.json")), "--require-complete"],
    "--require-complete gate rejected");
} finally {
  if (resolve(tempRoot).startsWith(expectedPrefix)) {
    rmSync(tempRoot, { recursive: true, force: true });
  }
}

process.stdout.write("parity verifier self-test passed\n");
