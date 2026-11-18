// phys_footprint (KB) per pid via build/bs-footprint.
import { execFile } from "node:child_process";
import { join } from "node:path";
import { promisify } from "node:util";

const run = promisify(execFile);
const BIN = join(import.meta.dirname, "..", "..", "build", "bs-footprint");

export async function footprints(pids: number[]): Promise<Map<number, number>> {
  const { stdout } = await run(BIN, pids.map(String));
  const out = new Map<number, number>();
  for (const line of stdout.trim().split("\n").filter(Boolean)) {
    const [pid, kb] = line.split(" ").map(Number);
    out.set(pid, kb);
  }
  return out;
}
