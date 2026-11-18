// HTTP load via oha (JSON output). Latencies converted to ms.
import { execFile } from "node:child_process";
import { promisify } from "node:util";

const run = promisify(execFile);

export interface LoadResult { rps: number; p50: number | null; p95: number | null; p99: number | null; errors: number; total: number; raw: unknown }

export async function oha(url: string, concurrency: number, seconds: number): Promise<LoadResult> {
  const { stdout } = await run("oha", ["-z", `${seconds}s`, "-c", String(concurrency), "--no-tui", "--output-format", "json", url], { maxBuffer: 64 * 1024 * 1024 });
  const j = JSON.parse(stdout);
  const ms = (v: number | null | undefined) => (v == null ? null : v * 1000);
  const codes = j.statusCodeDistribution as Record<string, number>;
  const total = Object.values(codes).reduce((a, b) => a + b, 0) + Object.values(j.errorDistribution as Record<string, number>).reduce((a, b) => a + b, 0);
  const non2xx = Object.entries(codes).filter(([c]) => !c.startsWith("2")).reduce((a, [, n]) => a + n, 0);
  const errs = Object.entries(j.errorDistribution as Record<string, number>).filter(([k]) => !k.includes("deadline")).reduce((a, [, n]) => a + n, 0);
  return { rps: j.summary.requestsPerSec, p50: ms(j.latencyPercentiles.p50), p95: ms(j.latencyPercentiles.p95), p99: ms(j.latencyPercentiles.p99), errors: errs + non2xx, total, raw: { summary: j.summary, latencyPercentiles: j.latencyPercentiles, statusCodeDistribution: codes, errorDistribution: j.errorDistribution } };
}
