import { Database } from "bun:sqlite";
import { existsSync, readFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import { rng } from "./rng";

export const SEED = 42;
/** Fixed "today": 2026-11-01T00:00:00Z. Dashboard ranges are relative to this. */
export const ANCHOR_TS = Date.UTC(2026, 10, 1) / 1000;
export const COUNTRIES = ["TR", "DE", "US", "GB", "FR", "NL", "ES", "IT", "SE", "PL", "JP", "BR"] as const;

const DAY = 86400;
const CATEGORIES: [string, string[]][] = [
  ["Aydınlatma", ["Lamp", "Avize", "Aplik", "Spot", "Şerit Led"]],
  ["Mobilya", ["Sandalye", "Masa", "Koltuk", "Raf", "Sehpa"]],
  ["Mutfak", ["Tencere", "Tava", "Bıçak Seti", "Kupa", "Demlik"]],
  ["Tekstil", ["Halı", "Perde", "Yastık", "Battaniye", "Örtü"]],
  ["Bahçe", ["Saksı", "Hortum", "Şezlong", "Çit", "Fener"]],
  ["Ofis", ["Defter", "Kalem", "Klasör", "Monitör Standı", "Çanta"]],
  ["Elektronik", ["Kulaklık", "Hoparlör", "Şarj Cihazı", "Kablo", "Saat"]],
  ["Oyuncak", ["Puzzle", "Blok Seti", "Peluş", "Araba", "Top"]],
  ["Kitap", ["Roman", "Atlas", "Sözlük", "Dergi", "Ajanda"]],
  ["Spor", ["Mat", "Dambıl", "Matara", "Bileklik", "İp"]],
  ["Banyo", ["Havlu", "Bornoz", "Sabunluk", "Ayna", "Paspas"]],
  ["Dekorasyon", ["Vazo", "Çerçeve", "Mum", "Tablo", "Heykel"]],
];
const ADJECTIVES = ["Modern", "Klasik", "Doğal", "Şık", "Minimal", "Rustik", "İnce", "Kompakt", "Lüks", "Vintage"];
const DESCR = ["Günlük kullanım için tasarlandı.", "Dayanıklı malzemeden üretildi.", "Kolay temizlenir.", "Sınırlı stok.", "Hediye için ideal."];
const BOARDS = ["Web Sitesi", "Mobil Uygulama", "Pazarlama", "Altyapı", "Müşteri Destek"];
const COLUMNS = ["backlog", "todo", "doing", "done"] as const;
const VERBS = ["Tasarla", "Düzelt", "Test et", "Gözden geçir", "Yayınla", "Belgele", "Ölç", "Planla"];
const OBJECTS = ["giriş sayfası", "ödeme akışı", "arama", "bildirimler", "raporlar", "API", "veritabanı", "e-posta şablonu"];
const LABELS = ["bug", "feature", "ux", "backend", "frontend", "acil"];
const PEOPLE = ["Ayşe", "Mehmet", "Zeynep", "Can", "Elif", "Burak"];

const isoDate = (ts: number) => new Date(ts * 1000).toISOString().slice(0, 10);

export function generate(outPath: string): void {
  for (const suffix of ["", "-wal", "-shm"]) if (existsSync(outPath + suffix)) rmSync(outPath + suffix);
  const db = new Database(outPath, { create: true });
  db.exec(readFileSync(join(import.meta.dir, "..", "schema.sql"), "utf8"));
  const r = rng(SEED);

  db.transaction(() => {
    const cat = db.prepare("INSERT INTO categories (id, name, slug) VALUES (?, ?, ?)");
    CATEGORIES.forEach(([name], i) =>
      cat.run(i + 1, name, name.toLocaleLowerCase("tr").replace(/ı/g, "i").replace(/ş/g, "s").replace(/ç/g, "c").replace(/ğ/g, "g").replace(/ö/g, "o").replace(/ü/g, "u").replace(/\s+/g, "-")),
    );

    const productNames: string[] = [];
    const prod = db.prepare(
      "INSERT INTO products (id, category_id, name, description, price_cents, stock, rating, image) VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
    );
    for (let id = 1; id <= 2000; id++) {
      const cid = r.int(1, 12);
      const name = `${r.pick(ADJECTIVES)} ${r.pick(CATEGORIES[cid - 1][1])} ${r.int(100, 999)}`;
      productNames.push(name);
      prod.run(id, cid, name, `${r.pick(DESCR)} ${r.pick(DESCR)}`, r.int(20, 10000) * 50, r.int(0, 200), r.int(10, 50) / 10, `/static/img/cat-${cid}.svg`);
    }

    const board = db.prepare("INSERT INTO boards (id, name) VALUES (?, ?)");
    const card = db.prepare(
      "INSERT INTO cards (board_id, col, position, title, description, labels, assignee, due_date) VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
    );
    BOARDS.forEach((name, i) => {
      board.run(i + 1, name);
      const pos: Record<string, number> = { backlog: 0, todo: 0, doing: 0, done: 0 };
      for (let k = 0; k < 100; k++) {
        const col = k < 4 ? COLUMNS[k] : r.pick(COLUMNS);
        const labels = [r.pick(LABELS), r.pick(LABELS)].filter((v, j, xs) => xs.indexOf(v) === j).join(",");
        card.run(i + 1, col, pos[col]++, `${r.pick(VERBS)}: ${r.pick(OBJECTS)}`, "Kabul kriterleri kartın açıklamasında.", labels, r.pick(PEOPLE), isoDate(ANCHOR_TS + r.int(-30, 60) * DAY));
      }
    });

    const ev = db.prepare("INSERT INTO events (ts, user_id, type, country, item, revenue_cents) VALUES (?, ?, ?, ?, ?, ?)");
    for (let k = 0; k < 100000; k++) {
      const roll = r.next();
      const type = roll < 0.85 ? "view" : roll < 0.93 ? "signup" : "purchase";
      ev.run(ANCHOR_TS - r.int(1, 180 * DAY), r.int(1, 8000), type, r.pick(COUNTRIES), r.pick(productNames), type === "purchase" ? r.int(5, 500) * 100 : 0);
    }
  })();
  db.exec("PRAGMA wal_checkpoint(TRUNCATE)");
  db.close();
}

if (import.meta.main) {
  const i = process.argv.indexOf("--out");
  const out = i > 0 ? process.argv[i + 1] : join(import.meta.dir, "..", "..", "data", "pristine.sqlite");
  generate(out);
  console.log(`wrote ${out}`);
}
