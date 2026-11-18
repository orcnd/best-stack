// Full measurement run. Usage:
//   node bench/harness/run.ts [--usecases a,b] [--backends ...] [--frontends ...] [--profiles desktop,mobile]
//     [--iters 10] [--flow-iters 5] [--load-duration 10] [--concurrency 1,16,64] [--seed 1] [--quick] [--skip-dx] [--notes "..."]
import { execFileSync } from "node:child_process";
import { mkdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { chromium } from "playwright";
import { rng } from "../../shared/seed/rng.ts";
import { functional } from "../parity/functional.ts";
import { seo } from "../parity/seo.ts";
import { visual } from "../parity/visual.ts";
import { API, BUN_PORT, CPP_PORT, type Frontend, FRONTENDS, frontendRoot, USECASES } from "../flows/targets.ts";
import { backendDx, frontendDx, sharedDx } from "./dx.ts";
import { timedFlow } from "./interactions.ts";
import { oha } from "./loadtest.ts";
import { measurePageLoad } from "./pageload.ts";
import { resetDb, ROOT, type Running, startBackend, startBunFrontend, stop } from "./procs.ts";
import type { Profile } from "./profiles.ts";
import { ResultsDb } from "./resultsdb.ts";
import { Sampler } from "./sampler.ts";

export const BACKENDS = ["drogon", "crow", "oatpp", "uwebsockets", "httplib", "axum", "actix", "gohttp", "fiber", "httpzig", "zap"] as const;
export const PAGES: Record<string, string[]> = { catalog: ["/catalog", "/catalog/p/1", "/catalog/cart"], kanban: ["/kanban/1"], dashboard: ["/dashboard"] };
export const LOAD_PAGE: Record<string, string> = { catalog: "/catalog", kanban: "/kanban/1", dashboard: "/dashboard" };
export const LOAD_API: Record<string, string> = { catalog: "/api/catalog/products", kanban: "/api/kanban/boards/1", dashboard: "/api/dashboard?range=30" };

function args() {
  const a = process.argv.slice(2);
  const get = (k: string, d: string) => { const i = a.indexOf(`--${k}`); return i >= 0 ? a[i + 1] : d; };
  const list = (k: string, d: readonly string[]) => get(k, d.join(",")).split(",").filter(Boolean);
  const quick = a.includes("--quick");
  return {
    usecases: list("usecases", USECASES), backends: list("backends", BACKENDS), frontends: list("frontends", FRONTENDS) as Frontend[],
    profiles: list("profiles", ["desktop", "mobile"]) as Profile[],
    iters: Number(get("iters", quick ? "2" : "10")), flowIters: Number(get("flow-iters", quick ? "1" : "5")),
    loadDuration: Number(get("load-duration", quick ? "3" : "10")), concurrency: list("concurrency", quick ? ["16"] : ["1", "16", "64"]).map(Number),
    seed: Number(get("seed", "1")), quick, skipDx: a.includes("--skip-dx"), notes: get("notes", ""), cooldownMs: quick ? 1000 : 5000,
  };
}

function shuffle<T>(xs: T[], seed: number): T[] {
  const r = rng(seed);
  const out = [...xs];
  for (let i = out.length - 1; i > 0; i--) {
    const j = Math.floor(r.next() * (i + 1));
    [out[i], out[j]] = [out[j], out[i]];
  }
  return out;
}

const sh = (cmd: string, ...xs: string[]) => execFileSync(cmd, xs).toString().trim();
const tryVersion = (cmd: string, ...xs: string[]) => { try { return sh(cmd, ...xs).split("\n")[0]; } catch { return null; } };

/** Exact code and toolchains this run measured (spec §9.4). */
function provenance() {
  return {
    git_commit: tryVersion("git", "-C", ROOT, "rev-parse", "HEAD"),
    git_dirty: (tryVersion("git", "-C", ROOT, "status", "--porcelain") ?? "") !== "",
    clang: tryVersion("clang", "--version"), rustc: tryVersion("rustc", "--version"), go: tryVersion("go", "version"),
    zig: tryVersion("zig", "version"), node: process.version,
    bun_ssr: tryVersion(join(ROOT, "frontends", "runtime", "node_modules", ".bin", "bun"), "--version"),
  };
}
const log = (...m: unknown[]) => console.log(new Date().toISOString().slice(11, 19), ...m);

async function main() {
  const o = args();
  mkdirSync(join(ROOT, "results"), { recursive: true });
  const db = new ResultsDb(join(ROOT, "results", "results.sqlite"));
  const combos = shuffle(o.backends.flatMap((b) => o.frontends.flatMap((f) => o.usecases.map((u) => ({ b, f, u })))), o.seed);
  const runId = db.startRun({
    seed: o.seed, order: combos, options: o,
    versions: { ...JSON.parse(readFileSync(join(ROOT, "shared", "versions.json"), "utf8")), provenance: provenance(), playwright: JSON.parse(readFileSync(join(ROOT, "bench", "node_modules", "playwright", "package.json"), "utf8")).version },
    machine: `${sh("sysctl", "-n", "machdep.cpu.brand_string")}, ${sh("sysctl", "-n", "hw.ncpu")} cpu, ${Math.round(Number(sh("sysctl", "-n", "hw.memsize")) / 2 ** 30)} GB`,
    os: `${sh("sw_vers", "-productName")} ${sh("sw_vers", "-productVersion")}`, notes: o.notes,
  });
  log(`run ${runId}: ${combos.length} combos`);

  if (!o.skipDx) {
    log("dx: clean builds + LOC");
    for (const b of o.backends) { const r = await backendDx(b, true); db.insertDx({ run_id: runId, component: r.component, name: r.name, loc: r.loc, deps: r.deps, build_s: r.build_s, artifact_kb: r.artifact_kb, details_json: JSON.stringify(r.details) }); }
    for (const f of o.frontends) { const r = await frontendDx(f, true); db.insertDx({ run_id: runId, component: r.component, name: r.name, loc: r.loc, deps: r.deps, build_s: r.build_s, artifact_kb: r.artifact_kb, details_json: JSON.stringify(r.details) }); }
    for (const r of sharedDx()) db.insertDx({ run_id: runId, component: r.component, name: r.name, loc: r.loc, deps: r.deps, build_s: r.build_s, artifact_kb: r.artifact_kb, details_json: JSON.stringify(r.details) });
  }

  const browser = await chromium.launch();
  let n = 0;
  for (const { b, f, u } of combos) {
    n++;
    const cid = db.comboId(b, f, u);
    log(`[${n}/${combos.length}] ${b} × ${f} × ${u}`);
    resetDb();
    let backend: Running | null = null, bun: Running | null = null, sampler: Sampler | null = null;
    try {
      backend = await startBackend(b);
      bun = await startBunFrontend(f);
      for (const p of [backend, bun]) if (p) db.insertStart({ run_id: runId, combo_id: cid, process_role: p.role, cold_start_ms: p.coldStartMs });
      sampler = new Sampler(db, runId, cid, [backend, bun].filter(Boolean) as Running[]);
      sampler.start();
      await new Promise((r) => setTimeout(r, 1000));

      sampler.phase = "parity";
      const fn = await functional(f, [u]);
      const vis = await visual(f, false, u);
      const s = await seo(f);
      db.upsertParity({ run_id: runId, combo_id: cid, functional_ok: fn.ok ? 1 : 0, visual_diff_ratio: vis.worst, seo_ok: s.ok ? 1 : 0, parity_ok: fn.ok && vis.ok && s.ok ? 1 : 0, details_json: JSON.stringify({ functional: fn.failures, visual: vis.pages, seo: s.problems }) });
      if (!(fn.ok && vis.ok && s.ok)) log("  parity FAILED", JSON.stringify({ fn: fn.failures, vis: vis.pages.filter((p) => p.ratio > 0), seo: s.problems }));

      sampler.phase = "page_load";
      const root = frontendRoot(f);
      for (const path of PAGES[u]) for (let i = 0; i < 3; i++) await measurePageLoad(browser, root + path, "desktop"); // warm-up
      for (const profile of o.profiles) for (const path of PAGES[u]) for (let i = 0; i < o.iters; i++) {
        const m = await measurePageLoad(browser, root + path, profile);
        db.insertPageLoad({ run_id: runId, combo_id: cid, profile, page: path, iter: i, ...m });
      }

      sampler.phase = "interactions";
      for (const profile of o.profiles) for (let i = 0; i < o.flowIters; i++) {
        for (const st of await timedFlow(browser, f, u, profile))
          db.insertInteraction({ run_id: runId, combo_id: cid, profile, step: st.step, iter: i, duration_ms: Number.isNaN(st.ms) ? null : st.ms, ok: st.ok ? 1 : 0 });
      }

      for (const [phase, target, url] of [["load_test_page", "page", root + LOAD_PAGE[u]], ["load_test_api", "api", API + LOAD_API[u]]] as const) {
        sampler.phase = phase;
        for (const c of o.concurrency) {
          await oha(url, c, 2); // warm-up, not recorded
          const r = await oha(url, c, o.loadDuration);
          db.insertLoadTest({ run_id: runId, combo_id: cid, target, url, concurrency: c, duration_s: o.loadDuration, rps: r.rps, p50: r.p50, p95: r.p95, p99: r.p99, errors: r.errors, total: r.total, raw_json: JSON.stringify(r.raw) });
        }
      }
    } catch (e) {
      log("  combo error:", String((e as Error).message).split("\n")[0]);
      db.upsertParity({ run_id: runId, combo_id: cid, functional_ok: 0, visual_diff_ratio: null, seo_ok: 0, parity_ok: 0, details_json: JSON.stringify({ error: String(e) }) });
    } finally {
      await sampler?.stop();
      await stop(bun);
      await stop(backend);
      await new Promise((r) => setTimeout(r, o.cooldownMs));
    }
  }
  await browser.close();
  db.finishRun(runId);
  db.close();
  log(`done: ${runId}`);
  void CPP_PORT; void BUN_PORT;
}

await main();
