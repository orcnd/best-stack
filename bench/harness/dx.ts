// Developer-experience metrics per component: LOC (tokei), dependency count, clean build time, artifact size.
import { execFile, execFileSync } from "node:child_process";
import { readdirSync, readFileSync, statSync } from "node:fs";
import { join } from "node:path";
import { promisify } from "node:util";
import { ROOT } from "./procs.ts";

const run = promisify(execFile);
const BUN = join(ROOT, "frontends", "runtime", "node_modules", ".bin", "bun");

export interface DxRow { component: "backend" | "frontend" | "shared"; name: string; loc: number; deps: number; build_s: number | null; artifact_kb: number | null; details: unknown }

function tokei(...paths: string[]): { code: number; byLang: Record<string, number> } {
  const out = JSON.parse(execFileSync("tokei", ["--output", "json", ...paths], { cwd: ROOT, maxBuffer: 32 << 20 }).toString());
  const byLang: Record<string, number> = {};
  for (const [lang, v] of Object.entries(out as Record<string, { code: number }>)) if (lang !== "Total" && v.code) byLang[lang] = v.code;
  return { code: (out.Total as { code: number }).code, byLang };
}

function sizeKb(path: string): number {
  const st = statSync(path);
  if (!st.isDirectory()) return st.size / 1024;
  return readdirSync(path).reduce((a, f) => a + sizeKb(join(path, f)), 0);
}

async function timed(cmd: string, args: string[], cwd = ROOT): Promise<number> {
  const t0 = performance.now();
  await run(cmd, args, { cwd, maxBuffer: 64 << 20 });
  return (performance.now() - t0) / 1000;
}

export async function backendDx(name: string, build: boolean): Promise<DxRow> {
  const t = tokei(`backends/${name}`);
  const deps = readFileSync(join(ROOT, "backends", name, "deps.txt"), "utf8").trim().split("\n").filter(Boolean).length;
  let build_s: number | null = null;
  if (build) {
    await run("ninja", ["-C", "build", "-t", "clean", `bs-${name}`], { cwd: ROOT });
    build_s = await timed("cmake", ["--build", "build", "--target", `bs-${name}`]);
  }
  return { component: "backend", name, loc: t.code, deps, build_s, artifact_kb: sizeKb(join(ROOT, "build", "backends", name, `bs-${name}`)), details: t.byLang };
}

const VENDOR: Record<string, string[]> = { htmx: ["vendor/htmx.min.js"], alpine: ["vendor/alpine.min.js"], vanilla: [] };

export async function frontendDx(name: string, build: boolean): Promise<DxRow> {
  if (name in VENDOR) {
    const t = tokei(`frontends/${name}/src`);
    const build_s = build ? await timed(BUN, ["frontends/build.ts", name]) : null;
    const files = [join(ROOT, "shared/static/js", name), ...VENDOR[name].map((v) => join(ROOT, "shared/static", v))];
    return { component: "frontend", name, loc: t.code, deps: VENDOR[name].length, build_s, artifact_kb: files.reduce((a, f) => a + sizeKb(f), 0), details: { ...t.byLang, note: "inja templates are shared (counted under shared)" } };
  }
  const dir = join(ROOT, "frontends", name);
  const pkg = JSON.parse(readFileSync(join(dir, "package.json"), "utf8"));
  const deps = Object.keys(pkg.dependencies ?? {}).length + Object.keys(pkg.devDependencies ?? {}).length;
  const t = tokei(`frontends/${name}/src`, ...["vite.config.ts", "astro.config.mjs"].filter((f) => { try { statSync(join(dir, f)); return true; } catch { return false; } }).map((f) => `frontends/${name}/${f}`));
  const build_s = build ? await timed(BUN, ["run", "build"], dir) : null;
  return { component: "frontend", name, loc: t.code, deps, build_s, artifact_kb: sizeKb(join(dir, name === "sveltekit" ? "build" : "dist")), details: t.byLang };
}

export function sharedDx(): DxRow[] {
  const cpp = tokei("shared/cpp");
  const fe = tokei("frontends/shared");
  return [
    { component: "shared", name: "cpp-core", loc: cpp.code, deps: 4, build_s: null, artifact_kb: null, details: cpp.byLang },
    { component: "shared", name: "frontend-shared", loc: fe.code, deps: 0, build_s: null, artifact_kb: null, details: fe.byLang },
  ];
}
