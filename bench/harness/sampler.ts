// Samples RSS, CPU (from cumulative CPU time deltas), threads and open fds per process.
import { execFile } from "node:child_process";
import { promisify } from "node:util";
import { footprints } from "./footprint.ts";
import type { Running } from "./procs.ts";
import type { ResultsDb } from "./resultsdb.ts";

const run = promisify(execFile);

/** "MM:SS.ss" or "HH:MM:SS(.ss)" → seconds */
export function parseCpuTime(s: string): number {
  return s.trim().split(":").map(Number).reduce((acc, v) => acc * 60 + v, 0);
}

export class Sampler {
  private timer: ReturnType<typeof setInterval> | undefined;
  private rows: Record<string, string | number | null>[] = [];
  private last = new Map<number, { cpu: number; t: number }>();
  private tick = 0;
  private busy = false;
  phase = "idle";

  private db: ResultsDb;
  private runId: string;
  private comboId: number;
  private procs: Running[];
  private intervalMs: number;

  constructor(db: ResultsDb, runId: string, comboId: number, procs: Running[], intervalMs = 250) {
    this.db = db;
    this.runId = runId;
    this.comboId = comboId;
    this.procs = procs;
    this.intervalMs = intervalMs;
  }

  start() {
    this.timer = setInterval(() => void this.sample(), this.intervalMs);
  }

  private async sample() {
    if (this.busy) return;
    this.busy = true;
    const withFds = this.tick++ % 4 === 0;
    try {
      const fp = await footprints(this.procs.map((p) => p.pid)).catch(() => new Map<number, number>());
      for (const p of this.procs) {
        const t = performance.now();
        let rss = null as number | null, cpuPct = null as number | null, threads = null as number | null, fds = null as number | null;
        try {
          const { stdout } = await run("ps", ["-o", "rss=,time=", "-p", String(p.pid)]);
          const [rssS, timeS] = stdout.trim().split(/\s+/);
          rss = Number(rssS);
          const cpu = parseCpuTime(timeS);
          const prev = this.last.get(p.pid);
          if (prev) cpuPct = ((cpu - prev.cpu) / ((t - prev.t) / 1000)) * 100;
          this.last.set(p.pid, { cpu, t });
          const th = await run("ps", ["-M", "-p", String(p.pid)]);
          threads = th.stdout.trim().split("\n").length - 1;
          if (withFds) fds = (await run("lsof", ["-n", "-P", "-p", String(p.pid)])).stdout.trim().split("\n").length - 1;
        } catch {
          continue; // process gone
        }
        this.rows.push({ run_id: this.runId, combo_id: this.comboId, phase: this.phase, process_role: p.role, pid: p.pid, ts: Date.now() / 1000, rss_kb: rss, cpu_pct: cpuPct, threads, fds, footprint_kb: fp.get(p.pid) ?? null });
      }
    } finally {
      this.busy = false;
    }
  }

  async stop() {
    clearInterval(this.timer);
    while (this.busy) await new Promise((r) => setTimeout(r, 10));
    this.db.insertMany("resources", this.rows);
    this.rows = [];
  }
}
