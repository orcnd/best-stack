import { describe, expect, test, beforeAll } from "bun:test";
import { Database } from "bun:sqlite";
import { createHash } from "node:crypto";
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { generate, ANCHOR_TS, COUNTRIES } from "./seed";

const DAY = 86400;
let dir: string;
let a: Database;

function dump(db: Database): string {
  const h = createHash("sha256");
  for (const t of ["categories", "products", "boards", "cards", "events"]) {
    for (const row of db.query(`SELECT * FROM ${t} ORDER BY id`).values()) h.update(JSON.stringify(row));
  }
  return h.digest("hex");
}

beforeAll(() => {
  dir = mkdtempSync(join(tmpdir(), "bs-seed-"));
  generate(join(dir, "a.sqlite"));
  a = new Database(join(dir, "a.sqlite"), { readonly: true });
});

const one = (sql: string) => (a.query(sql).values()[0] as number[])[0];

describe("seed", () => {
  test("counts", () => {
    expect(one("SELECT count(*) FROM categories")).toBe(12);
    expect(one("SELECT count(*) FROM products")).toBe(2000);
    expect(one("SELECT count(*) FROM boards")).toBe(5);
    expect(one("SELECT count(*) FROM cards")).toBe(500);
    expect(one("SELECT min(n) FROM (SELECT count(*) n FROM cards GROUP BY board_id)")).toBe(100);
    expect(one("SELECT min(n) FROM (SELECT count(*) n FROM cards GROUP BY board_id, col)")).toBeGreaterThanOrEqual(1);
    expect(one("SELECT count(DISTINCT board_id || col) FROM cards")).toBe(20);
    expect(one("SELECT count(*) FROM events")).toBe(100000);
  });

  test("deterministic", () => {
    generate(join(dir, "b.sqlite"));
    const b = new Database(join(dir, "b.sqlite"), { readonly: true });
    expect(dump(b)).toBe(dump(a));
  });

  test("search_fixture", () => {
    const n = one("SELECT count(*) FROM products WHERE name LIKE '%Lamp%'");
    expect(n).toBeGreaterThanOrEqual(20);
    expect(n).toBeLessThanOrEqual(200);
  });

  test("events_window", () => {
    expect(one("SELECT min(ts) FROM events")).toBeGreaterThanOrEqual(ANCHOR_TS - 180 * DAY);
    expect(one("SELECT max(ts) FROM events")).toBeLessThan(ANCHOR_TS);
    const countries = a.query("SELECT DISTINCT country FROM events").values().map((r) => r[0]);
    for (const c of countries) expect(COUNTRIES).toContain(c as string);
    expect(one("SELECT count(*) FROM events WHERE type='purchase' AND revenue_cents<=0")).toBe(0);
    expect(one("SELECT count(*) FROM events WHERE type!='purchase' AND revenue_cents!=0")).toBe(0);
  });

  test("turkish_fixture", () => {
    const n = one("SELECT count(*) FROM products WHERE name LIKE '%ğ%' OR name LIKE '%ş%' OR name LIKE '%İ%'");
    expect(n).toBeGreaterThanOrEqual(1);
  });
});
