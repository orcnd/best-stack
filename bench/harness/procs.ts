// Start/stop the servers of one combo and measure cold start (spawn → first 200 on /healthz).
import { type ChildProcess, spawn } from "node:child_process";
import { copyFileSync, existsSync, rmSync } from "node:fs";
import { join } from "node:path";
import { BUN_PORT, CPP_PORT, isBunFrontend } from "../flows/targets.ts";

export const ROOT = join(import.meta.dirname, "..", "..");
const BUN = join(ROOT, "frontends", "runtime", "node_modules", ".bin", "bun");

export type Role = "cpp_backend" | "bun_ssr";
export interface Running { role: Role; proc: ChildProcess; pid: number; coldStartMs: number }

export function resetDb() {
  for (const s of ["", "-wal", "-shm"]) rmSync(join(ROOT, "data", `app.sqlite${s}`), { force: true });
  copyFileSync(join(ROOT, "data", "pristine.sqlite"), join(ROOT, "data", "app.sqlite"));
}

async function waitHealthy(url: string, proc: ChildProcess, t0: number, timeoutMs = 20_000): Promise<number> {
  while (performance.now() - t0 < timeoutMs) {
    if (proc.exitCode !== null) throw new Error(`process exited early (${proc.exitCode}) for ${url}`);
    try {
      const r = await fetch(url);
      if (r.status === 200) return performance.now() - t0;
    } catch {}
    await new Promise((r) => setTimeout(r, 5));
  }
  throw new Error(`timeout waiting for ${url}`);
}

async function portFree(port: number) {
  for (let i = 0; i < 200; i++) {
    try {
      await fetch(`http://localhost:${port}/healthz`);
    } catch {
      return;
    }
    await new Promise((r) => setTimeout(r, 25));
  }
  throw new Error(`port ${port} still in use`);
}

export async function startBackend(backend: string, threads = 8): Promise<Running> {
  await portFree(CPP_PORT);
  const bin = join(ROOT, "build", "backends", backend, `bs-${backend}`);
  if (!existsSync(bin)) throw new Error(`missing binary ${bin}`);
  const t0 = performance.now();
  const proc = spawn(bin, ["--port", String(CPP_PORT), "--db", join(ROOT, "data", "app.sqlite"), "--root", ROOT, "--threads", String(threads)], { stdio: "ignore" });
  const coldStartMs = await waitHealthy(`http://localhost:${CPP_PORT}/healthz`, proc, t0);
  return { role: "cpp_backend", proc, pid: proc.pid!, coldStartMs };
}

export async function startBunFrontend(fe: string): Promise<Running | null> {
  if (!isBunFrontend(fe)) return null;
  await portFree(BUN_PORT);
  const dir = join(ROOT, "frontends", fe);
  const entry = fe === "sveltekit" ? "build/index.js" : "dist/server/entry.mjs";
  const env = { ...process.env, HOST: "0.0.0.0", PORT: String(BUN_PORT), ORIGIN: `http://127.0.0.1:${BUN_PORT}`, API_BASE: `http://127.0.0.1:${CPP_PORT}`, NODE_ENV: "production" };
  const t0 = performance.now();
  const proc = spawn(BUN, [entry], { cwd: dir, env, stdio: "ignore" });
  const coldStartMs = await waitHealthy(`http://localhost:${BUN_PORT}/healthz`, proc, t0);
  return { role: "bun_ssr", proc, pid: proc.pid!, coldStartMs };
}

export async function stop(r: Running | null) {
  if (!r || r.proc.exitCode !== null) return;
  const exited = new Promise((res) => r.proc.once("exit", res));
  r.proc.kill("SIGTERM");
  const t = setTimeout(() => r.proc.kill("SIGKILL"), 3000);
  await exited;
  clearTimeout(t);
}
