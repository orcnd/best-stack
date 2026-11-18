import type { Page } from "playwright";
import { API } from "./targets.ts";
import { type StepFn, TIMEOUT } from "./types.ts";

const num = (v: number) => v.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ".");

const waitKpi = (page: Page, text: string) =>
  page.waitForFunction(
    (text) => [...document.querySelectorAll('[data-testid="kpi-events"]')].some((e) => (e as HTMLElement).offsetParent !== null && e.textContent?.trim() === text),
    text,
    { timeout: TIMEOUT },
  );

export async function run(page: Page, root: string, step: StepFn) {
  const d30 = await (await fetch(`${API}/api/dashboard?range=30`)).json();
  const d7 = await (await fetch(`${API}/api/dashboard?range=7`)).json();
  await step("load", async () => {
    await page.goto(`${root}/dashboard`);
    await waitKpi(page, num(d30.kpis.events));
  });
  await step("range_7", async () => {
    await page.locator('[data-testid="range-select"]:visible').selectOption("7");
    await waitKpi(page, num(d7.kpis.events));
  });
  await step("range_90", async () => {
    const d90 = await (await fetch(`${API}/api/dashboard?range=90`)).json();
    await page.locator('[data-testid="range-select"]:visible').selectOption("90");
    await waitKpi(page, num(d90.kpis.events));
  });
}
