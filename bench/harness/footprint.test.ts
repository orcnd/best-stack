import assert from "node:assert/strict";
import { test } from "node:test";
import { footprints } from "./footprint.ts";

test("reads phys_footprint for live pids and skips dead ones", async () => {
  const m = await footprints([process.pid, 999999]);
  assert.ok((m.get(process.pid) ?? 0) > 1024, "node itself uses more than 1 MB");
  assert.equal(m.has(999999), false);
});
