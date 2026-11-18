// Runs a user flow once and returns per-step durations (action start → expected DOM state).
import type { Browser } from "playwright";
import { FLOWS } from "../flows/index.ts";
import { type Frontend, frontendRoot } from "../flows/targets.ts";
import { applyProfile, type Profile } from "./profiles.ts";

export interface StepResult { step: string; ms: number; ok: boolean }

export async function timedFlow(browser: Browser, fe: Frontend, usecase: string, profile: Profile): Promise<StepResult[]> {
  const ctx = await browser.newContext({ viewport: { width: 1280, height: 900 } });
  const out: StepResult[] = [];
  try {
    const page = await ctx.newPage();
    page.setDefaultTimeout(profile === "mobile" ? 30_000 : 10_000);
    await applyProfile(await ctx.newCDPSession(page), profile);
    let current = "";
    try {
      await FLOWS[usecase](page, frontendRoot(fe), async (name, fn) => {
        current = name;
        const t0 = performance.now();
        await fn();
        out.push({ step: name, ms: performance.now() - t0, ok: true });
      });
    } catch {
      out.push({ step: current, ms: NaN, ok: false });
    }
  } finally {
    await ctx.close();
  }
  return out;
}
