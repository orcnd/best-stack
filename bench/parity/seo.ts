// Usage: node bench/parity/seo.ts <frontend>
// With JavaScript disabled, list and detail pages must carry full content and metadata.
import { chromium } from "playwright";
import { type Frontend, frontendRoot } from "../flows/targets.ts";

export async function seo(fe: Frontend) {
  const browser = await chromium.launch();
  const problems: string[] = [];
  try {
    const ctx = await browser.newContext({ javaScriptEnabled: false });
    const page = await ctx.newPage();
    for (const [path, cards] of [["/catalog", 24], ["/catalog/p/1", 0], ["/kanban/1", 0], ["/dashboard", 0]] as const) {
      await page.goto(frontendRoot(fe) + path);
      const title = await page.title();
      const desc = await page.locator('meta[name="description"]').getAttribute("content");
      const h1 = await page.locator("h1").first().textContent();
      if (!title.endsWith("· Best Stack")) problems.push(`${path}: title "${title}"`);
      if (!desc) problems.push(`${path}: no meta description`);
      if (!h1?.trim()) problems.push(`${path}: no h1`);
      if (cards) {
        const n = await page.locator('[data-testid="product-card"]').count();
        if (n !== cards) problems.push(`${path}: ${n} product cards without JS`);
      }
    }
    if ((await (await page.goto(frontendRoot(fe) + "/kanban/1"))?.text())?.includes('data-testid="card"') !== true)
      problems.push("/kanban/1: cards not server-rendered");
    await ctx.close();
  } finally {
    await browser.close();
  }
  return { ok: problems.length === 0, problems };
}

if (import.meta.main) {
  const fe = process.argv[2] as Frontend;
  const res = await seo(fe);
  console.log(JSON.stringify({ frontend: fe, ...res }));
  process.exit(res.ok ? 0 : 1);
}
