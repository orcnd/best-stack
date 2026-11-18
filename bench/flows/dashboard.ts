import type { Page } from "playwright";
import { API } from "./targets.ts";
import { clientSide, instrument, settle, type StepFn } from "./types.ts";

const num = (v: number) => v.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ".");

const waitKpi = (page: Page, text: string) =>
  page.waitForFunction(
    (text) => [...document.querySelectorAll('[data-testid="kpi-events"]')].some((e) => (e as HTMLElement).offsetParent !== null && e.textContent?.trim() === text),
    text,
    {},
  );

export async function run(page: Page, root: string, step: StepFn) {
  await instrument(page);
  // expected values are fetched up front so no step times an extra API round trip
  const [d30, d7, d90] = await Promise.all([30, 7, 90].map(async (r) => (await fetch(`${API}/api/dashboard?range=${r}`)).json()));
  await step("load", async () => {
    await page.goto(`${root}/dashboard`);
    await waitKpi(page, num(d30.kpis.events));
    await settle(page);
  });
  await step("range_7", () => clientSide(page, async () => {
    await page.locator('[data-testid="range-select"]:visible').selectOption("7");
    await waitKpi(page, num(d7.kpis.events));
  }));
  await step("range_90", () => clientSide(page, async () => {
    await page.locator('[data-testid="range-select"]:visible').selectOption("90");
    await waitKpi(page, num(d90.kpis.events));
  }));
}
