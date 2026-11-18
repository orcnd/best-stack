// Thin writer over results/results.sqlite (node:sqlite). Raw rows only.
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { DatabaseSync } from "node:sqlite";

type Row = Record<string, string | number | null>;

export class ResultsDb {
  readonly db: DatabaseSync;
  constructor(path: string) {
    this.db = new DatabaseSync(path);
    this.db.exec("PRAGMA journal_mode = WAL");
    this.db.exec(readFileSync(join(import.meta.dirname, "schema.sql"), "utf8"));
    // migrate run-1 databases
    const cols = (this.db.prepare("PRAGMA table_info(resources)").all() as { name: string }[]).map((c) => c.name);
    if (!cols.includes("footprint_kb")) this.db.exec("ALTER TABLE resources ADD COLUMN footprint_kb INTEGER");
  }

  close() { this.db.close(); }

  count(table: string): number {
    return (this.db.prepare(`SELECT count(*) AS n FROM ${table}`).get() as { n: number }).n;
  }

  startRun(e: { seed: number; order: unknown; options: unknown; versions: unknown; machine: string; os: string; notes?: string }): string {
    const run = `run-${new Date().toISOString().replace(/[:.]/g, "-")}`;
    this.db.prepare(
      "INSERT INTO env (run_id, started_at, machine, os, versions_json, seed, order_json, options_json, notes) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
    ).run(run, new Date().toISOString(), e.machine, e.os, JSON.stringify(e.versions), e.seed, JSON.stringify(e.order), JSON.stringify(e.options), e.notes ?? null);
    return run;
  }

  finishRun(run: string) {
    this.db.prepare("UPDATE env SET finished_at = ? WHERE run_id = ?").run(new Date().toISOString(), run);
  }

  comboId(backend: string, frontend: string, usecase: string): number {
    this.db.prepare("INSERT OR IGNORE INTO combos (backend, frontend, usecase) VALUES (?, ?, ?)").run(backend, frontend, usecase);
    return (this.db.prepare("SELECT combo_id FROM combos WHERE backend = ? AND frontend = ? AND usecase = ?").get(backend, frontend, usecase) as { combo_id: number }).combo_id;
  }

  private insert(table: string, row: Row) {
    const keys = Object.keys(row);
    this.db.prepare(`INSERT INTO ${table} (${keys.join(", ")}) VALUES (${keys.map(() => "?").join(", ")})`).run(...keys.map((k) => row[k]));
  }

  insertPageLoad(r: Row) { this.insert("page_loads", r); }
  insertInteraction(r: Row) { this.insert("interactions", r); }
  insertLoadTest(r: Row) { this.insert("load_tests", r); }
  insertResource(r: Row) { this.insert("resources", r); }
  insertStart(r: Row) { this.insert("starts", r); }
  insertDx(r: Row) { this.insert("dx_components", r); }
  upsertParity(r: Row) {
    const keys = Object.keys(r);
    this.db.prepare(`INSERT OR REPLACE INTO parity (${keys.join(", ")}) VALUES (${keys.map(() => "?").join(", ")})`).run(...keys.map((k) => r[k]));
  }
  /** Many rows in one transaction (sampler flushes). */
  insertMany(table: string, rows: Row[]) {
    if (!rows.length) return;
    this.db.exec("BEGIN");
    try {
      for (const r of rows) this.insert(table, r);
      this.db.exec("COMMIT");
    } catch (e) {
      this.db.exec("ROLLBACK");
      throw e;
    }
  }
}
