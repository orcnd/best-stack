import type { Page } from "playwright";
export type StepFn = (name: string, fn: () => Promise<void>) => Promise<void>;
export type Flow = (page: Page, root: string, step: StepFn) => Promise<void>;
/** Timeouts come from page.setDefaultTimeout (profile-aware: harness sets 10 s desktop / 30 s mobile). */
export const TIMEOUT = undefined;

/** Per-page network tracker fed by Playwright's request events: every request the page makes (documents,
 *  scripts, dynamically imported hydration chunks, fetch/XHR), including ones still downloading. */
const trackers = new WeakMap<Page, { inflight: number; last: number }>();
function track(page: Page) {
  const t = { inflight: 0, last: performance.now() };
  trackers.set(page, t);
  page.on("request", () => { t.inflight++; t.last = performance.now(); });
  const done = () => { t.inflight = Math.max(0, t.inflight - 1); t.last = performance.now(); };
  page.on("requestfinished", done);
  page.on("requestfailed", done);
}

/** Navigation steps end only when the page is interactive, so slow JS download/hydration shows up as
 *  load time instead of a failed click: load event done, no request in flight, and 50 ms without
 *  network activity. The same window applies to every frontend and to full-page and client-side
 *  navigations alike (Playwright's networkidle waits 500 ms and resolves immediately for client-side
 *  routes, which biased the comparison). */
export async function settle(page: Page) {
  const t = trackers.get(page)!;
  const deadline = performance.now() + 30_000;
  while (performance.now() < deadline) {
    if (t.inflight === 0 && performance.now() - t.last >= 50 && (await page.evaluate(() => document.readyState)) === "complete") return;
    await new Promise((r) => setTimeout(r, 10));
  }
  throw new Error("page did not settle");
}

/** Counts in-flight fetch/XHR requests in every document of the page (htmx uses XHR, the others fetch). */
export async function instrument(page: Page) {
  track(page);
  await page.addInitScript(() => {
    const w = window as unknown as { __bsInflight: number };
    w.__bsInflight = 0;
    const origFetch = window.fetch;
    window.fetch = (...args: Parameters<typeof fetch>) => {
      w.__bsInflight++;
      return origFetch(...args).finally(() => { w.__bsInflight--; });
    };
    const send = XMLHttpRequest.prototype.send;
    XMLHttpRequest.prototype.send = function (this: XMLHttpRequest, ...a: Parameters<XMLHttpRequest["send"]>) {
      w.__bsInflight++;
      this.addEventListener("loadend", () => { w.__bsInflight--; }, { once: true });
      return send.apply(this, a);
    };
  });
}

/** Runs an action that must be handled client-side (JS frontends): fails if it caused a full page load
 *  (e.g. a form fell back to its no-JS POST because the page was not yet interactive). */
export async function clientSide(page: Page, fn: () => Promise<void>) {
  await page.evaluate(() => { (window as unknown as { __bsMarker: number }).__bsMarker = 1; });
  await fn();
  // done = expected DOM state reached AND no request still pending (e.g. a swap that is about to land)
  await page.waitForFunction(() => ((window as unknown as { __bsInflight?: number }).__bsInflight ?? 0) === 0);
  const still = await page.evaluate(() => (window as unknown as { __bsMarker?: number }).__bsMarker === 1);
  if (!still) throw new Error("action caused a full page load instead of a client-side update");
}
