// One cold page load (fresh context, cache disabled) with navigation/paint/LCP/TBT and CDP byte counts.
import type { Browser } from "playwright";
import { applyProfile, type Profile } from "./profiles.ts";

export interface PageLoad { ttfb: number; fcp: number | null; lcp: number | null; dcl: number; load: number; bytes: number; js_bytes: number; requests: number; tbt: number; heap: number }

const OBSERVERS = () => {
  const w = window as unknown as { __lcp: number | null; __tbt: number };
  w.__lcp = null;
  w.__tbt = 0;
  new PerformanceObserver((l) => { for (const e of l.getEntries()) w.__lcp = e.startTime; }).observe({ type: "largest-contentful-paint", buffered: true });
  new PerformanceObserver((l) => { for (const e of l.getEntries()) w.__tbt += Math.max(0, e.duration - 50); }).observe({ type: "longtask", buffered: true });
};

export async function measurePageLoad(browser: Browser, url: string, profile: Profile): Promise<PageLoad> {
  const ctx = await browser.newContext({ viewport: { width: 1280, height: 900 } });
  try {
    const page = await ctx.newPage();
    await page.addInitScript(OBSERVERS);
    const cdp = await ctx.newCDPSession(page);
    await cdp.send("Network.enable");
    await cdp.send("Network.setCacheDisabled", { cacheDisabled: true });
    await applyProfile(cdp, profile);
    const types = new Map<string, string>();
    let bytes = 0, js = 0, requests = 0;
    cdp.on("Network.responseReceived", (e) => types.set(e.requestId, e.type));
    cdp.on("Network.loadingFinished", (e) => {
      requests++;
      bytes += e.encodedDataLength;
      if (types.get(e.requestId) === "Script") js += e.encodedDataLength;
    });
    const res = await page.goto(url, { waitUntil: "load", timeout: 60_000 });
    if (!res || res.status() !== 200) throw new Error(`HTTP ${res?.status()} for ${url}`);
    await page.waitForTimeout(profile === "mobile" ? 1500 : 500); // let LCP/long tasks settle
    const m = await page.evaluate(() => {
      const nav = performance.getEntriesByType("navigation")[0] as PerformanceNavigationTiming;
      const fcp = performance.getEntriesByName("first-contentful-paint")[0]?.startTime ?? null;
      const w = window as unknown as { __lcp: number | null; __tbt: number };
      return { ttfb: nav.responseStart, dcl: nav.domContentLoadedEventEnd, load: nav.loadEventEnd, fcp, lcp: w.__lcp, tbt: w.__tbt };
    });
    const heap = (await cdp.send("Runtime.getHeapUsage")).usedSize;
    return { ...m, bytes, js_bytes: js, requests, heap };
  } finally {
    await ctx.close();
  }
}
