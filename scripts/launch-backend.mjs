#!/usr/bin/env node
// Cross-platform launcher for the Minera backend.
//
// CMake's binary location depends on the generator: single-config builds
// (Make/Ninja, typical on macOS/Linux) emit `build/minera_server`, while
// multi-config builds (Visual Studio) nest a per-config subdir and add a
// `.exe` suffix, e.g. `build/Debug/minera_server.exe`. No single literal path
// works for both, so this script resolves whichever binary exists and execs it,
// forwarding arguments, stdio, and termination signals.
import { spawn } from "node:child_process";
import { existsSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join, resolve } from "node:path";

const repoRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");

const candidates = [
  "build/minera_server",
  "build/minera_server.exe",
  "build/Debug/minera_server.exe",
  "build/Release/minera_server.exe",
  "build/RelWithDebInfo/minera_server.exe",
].map((relative) => join(repoRoot, relative));

const binary = candidates.find(existsSync);
if (!binary) {
  console.error(
    "minera_server not found — build it first (cmake --build build).\n" +
      "looked in:\n  " + candidates.join("\n  "),
  );
  process.exit(1);
}

const child = spawn(binary, process.argv.slice(2), { stdio: "inherit" });
child.on("exit", (code, signal) => process.exit(signal ? 1 : code ?? 0));
child.on("error", (error) => {
  console.error("failed to launch minera_server:", error.message);
  process.exit(1);
});
for (const signal of ["SIGINT", "SIGTERM"]) {
  process.on(signal, () => child.kill(signal));
}
