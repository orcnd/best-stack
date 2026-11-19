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
  // Generated code (templ's *_templ.go) is not written by hand; its .templ source is counted instead.
  const out = JSON.parse(execFileSync("tokei", ["--output", "json", "--exclude", "*_templ.go", ...paths], { cwd: ROOT, maxBuffer: 32 << 20 }).toString());
  const byLang: Record<string, number> = {};
  for (const [lang, v] of Object.entries(out as Record<string, { code: number }>)) if (lang !== "Total" && v.code) byLang[lang] = v.code;
  return { code: (out.Total as { code: number }).code, byLang };
}

function sizeKb(path: string): number {
  const st = statSync(path);
  if (!st.isDirectory()) return st.size / 1024;
  return readdirSync(path).reduce((a, f) => a + sizeKb(join(path, f)), 0);
}

async function timed(cmd: string, args: string[], cwd = ROOT, env?: Record<string, string>): Promise<number> {
  const t0 = performance.now();
  await run(cmd, args, { cwd, maxBuffer: 64 << 20, env: env ? { ...process.env, ...env } : process.env });
  return (performance.now() - t0) / 1000;
}

/** Where each backend's code lives. LOC = adapter + that language's shared core (templates/HTML included,
 *  so a language that writes HTML in code is not penalized against one that uses template files). */
interface BackendMeta { lang: "cpp" | "rust" | "go" | "zig"; paths: string[]; deps: string; clean: () => Promise<number> }
const tmpDir = () => execFileSync("mktemp", ["-d"]).toString().trim();
const CPP_CORE = ["shared/cpp/src", "shared/cpp/include", "shared/cpp/templates"];
function meta(name: string): BackendMeta {
  if (["crow-opt", "uwebsockets-opt"].includes(name))
    return { lang: "cpp", paths: [`backends/${name.replace("-opt", "")}`, `backends/${name}`, "shared/cpp-fast", "shared/cpp/include", "shared/cpp/src/db.cpp", "shared/cpp/src/http.cpp", "shared/cpp/src/version.cpp"], deps: `backends/${name}/deps.txt`, clean: async () => {
      await run("ninja", ["-C", "build", "-t", "clean", `bs-${name}`], { cwd: ROOT });
      return timed("cmake", ["--build", "build", "--target", `bs-${name}`]);
    } };
  if (["axum-opt", "actix-opt"].includes(name))
    return { lang: "rust", paths: [`backends/rust/${name.replace("-opt", "")}/src`, "backends/rust/core/src", "backends/rust/core-fast/src", "backends/rust/core-fast/templates"], deps: `backends/rust/${name}/deps.txt`,
      clean: () => timed("cargo", ["build", "--release", "-p", `bs-${name}`, "--target-dir", tmpDir()], join(ROOT, "backends", "rust")) };
  if (["gohttp-opt", "fiber-opt"].includes(name))
    return { lang: "go", paths: [`backends/go/cmd/${name}`, "backends/go/corefast", "backends/go/core"], deps: `backends/go/cmd/${name}/deps.txt`,
      clean: () => timed("go", ["build", "-a", "-o", join(tmpDir(), "bin"), `./cmd/${name}`], join(ROOT, "backends", "go")) };
  if (["drogon", "crow", "oatpp", "uwebsockets", "httplib"].includes(name))
    return { lang: "cpp", paths: [`backends/${name}`, ...CPP_CORE], deps: `backends/${name}/deps.txt`, clean: async () => {
      await run("ninja", ["-C", "build", "-t", "clean", `bs-${name}`], { cwd: ROOT });
      return timed("cmake", ["--build", "build", "--target", `bs-${name}`]);
    } };
  if (["axum", "actix"].includes(name))
    return { lang: "rust", paths: [`backends/rust/${name}/src`, "backends/rust/core/src", "shared/cpp/templates"], deps: `backends/rust/${name}/deps.txt`,
      clean: () => timed("cargo", ["build", "--release", "-p", `bs-${name}`, "--target-dir", tmpDir()], join(ROOT, "backends", "rust")) };
  if (["gohttp", "fiber"].includes(name))
    return { lang: "go", paths: [`backends/go/cmd/${name}`, "backends/go/core"], deps: `backends/go/cmd/${name}/deps.txt`,
      clean: () => timed("go", ["build", "-a", "-o", join(tmpDir(), "bin"), `./cmd/${name}`], join(ROOT, "backends", "go")) };
  if (["httpzig", "zap"].includes(name))
    return { lang: "zig", paths: [`backends/zig/${name}`, "backends/zig/core"], deps: `backends/zig/${name}/deps.txt`,
      clean: () => { const d = tmpDir(); return timed("zig", ["build", "-Doptimize=ReleaseFast", "--prefix", join(d, "out")], join(ROOT, "backends", "zig"), { ZIG_LOCAL_CACHE_DIR: join(d, "c"), ZIG_GLOBAL_CACHE_DIR: join(d, "g") }); } };
  throw new Error(`unknown backend ${name}`);
}

export async function backendDx(name: string, build: boolean): Promise<DxRow> {
  const m = meta(name);
  const t = tokei(...m.paths);
  const deps = readFileSync(join(ROOT, m.deps), "utf8").trim().split("\n").filter(Boolean).length;
  const build_s = build ? await m.clean() : null;
  return { component: "backend", name, loc: t.code, deps, build_s, artifact_kb: sizeKb(join(ROOT, "build", "backends", name, `bs-${name}`)), details: { lang: m.lang, ...t.byLang } };
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
