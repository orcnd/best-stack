// Usage: node bench/parity/visual.ts <frontend> [--update-reference]
// Screenshots VISUAL_PAGES at 1280x900 (full page) and compares to bench/parity/reference/*.png.
import { mkdirSync, readFileSync, writeFileSync, existsSync } from "node:fs";
import { join } from "node:path";
import pixelmatch from "pixelmatch";
import { PNG } from "pngjs";
import { chromium } from "playwright";
import { type Frontend, frontendRoot } from "../flows/targets.ts";
import { VISUAL_PAGES } from "./pages.ts";

export const THRESHOLD = 0.005;
const REF = join(import.meta.dirname, "reference");
const OUT = join(import.meta.dirname, "..", "..", "results", "screens");

export async function visual(fe: Frontend, update = false) {
  mkdirSync(REF, { recursive: true });
  mkdirSync(join(OUT, fe), { recursive: true });
  const browser = await chromium.launch();
  const pages: { page: string; ratio: number; note?: string }[] = [];
  try {
    for (const vp of VISUAL_PAGES) {
      const ctx = await browser.newContext({ viewport: { width: 1280, height: 900 }, deviceScaleFactor: 1 });
      const page = await ctx.newPage();
      await page.goto(frontendRoot(fe) + vp.path, { waitUntil: "networkidle" });
      if (vp.act) {
        await vp.act(page);
        await page.waitForLoadState("networkidle");
      }
      await page.evaluate(() => document.fonts.ready);
      const buf = await page.screenshot({ fullPage: true, animations: "disabled", caret: "hide" });
      await ctx.close();
      writeFileSync(join(OUT, fe, `${vp.name}.png`), buf);
      const refPath = join(REF, `${vp.name}.png`);
      if (update || !existsSync(refPath)) {
        writeFileSync(refPath, buf);
        pages.push({ page: vp.name, ratio: 0, note: "reference written" });
        continue;
      }
      const a = PNG.sync.read(readFileSync(refPath));
      const b = PNG.sync.read(buf);
      if (a.width !== b.width || a.height !== b.height) {
        pages.push({ page: vp.name, ratio: 1, note: `size ${b.width}x${b.height} vs ref ${a.width}x${a.height}` });
        continue;
      }
      const diff = new PNG({ width: a.width, height: a.height });
      const n = pixelmatch(a.data, b.data, diff.data, a.width, a.height, { threshold: 0.1 });
      const ratio = n / (a.width * a.height);
      if (ratio >= THRESHOLD) writeFileSync(join(OUT, fe, `${vp.name}.diff.png`), PNG.sync.write(diff));
      pages.push({ page: vp.name, ratio });
    }
  } finally {
    await browser.close();
  }
  const worst = Math.max(...pages.map((p) => p.ratio));
  return { ok: worst < THRESHOLD, worst, pages };
}

if (import.meta.main) {
  const fe = process.argv[2] as Frontend;
  const res = await visual(fe, process.argv.includes("--update-reference"));
  console.log(JSON.stringify({ frontend: fe, ...res }));
  process.exit(res.ok ? 0 : 1);
}
