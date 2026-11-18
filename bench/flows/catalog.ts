import type { Page } from "playwright";
import { clientSide, instrument, settle, type StepFn } from "./types.ts";

const visible = (page: Page, id: string) => page.locator(`[data-testid="${id}"]:visible`);

/** Wait until every visible element with testid has text matching pred (and at least one exists). */
async function waitAll(page: Page, id: string, pred: string) {
  await page.waitForFunction(
    ([id, pred]) => {
      const els = [...document.querySelectorAll(`[data-testid="${id}"]`)].filter((e) => (e as HTMLElement).offsetParent !== null);
      const f = new Function("t", `return ${pred}`) as (t: string) => boolean;
      return els.length > 0 && els.every((e) => f((e.textContent ?? "").trim()));
    },
    [id, pred],
    {},
  );
}

const waitText = (page: Page, id: string, text: string) =>
  page.waitForFunction(
    ([id, text]) => [...document.querySelectorAll(`[data-testid="${id}"]`)].some((e) => (e as HTMLElement).offsetParent !== null && e.textContent?.trim() === text),
    [id, text],
    {},
  );

export async function run(page: Page, root: string, step: StepFn) {
  await instrument(page);
  await step("load_list", async () => {
    await page.goto(`${root}/catalog`);
    await page.waitForFunction(() => document.querySelectorAll('[data-testid="product-card"]').length >= 24);
    await settle(page);
  });
  await step("filter_category", () => clientSide(page, async () => {
    await visible(page, "category-filter").selectOption("mobilya");
    await waitAll(page, "product-card", "t.includes('Mobilya')");
  }));
  await step("next_page", () => clientSide(page, async () => {
    await visible(page, "page-next").click();
    await page.waitForFunction(() => [...document.querySelectorAll(".page-info")].some((e) => (e as HTMLElement).offsetParent !== null && e.textContent?.startsWith("Sayfa 2 /")));
    await waitAll(page, "product-card", "t.includes('Mobilya')");
  }));
  await step("search", () => clientSide(page, async () => {
    await visible(page, "search-input").fill("Sandalye");
    await waitAll(page, "product-name", "t.includes('Sandalye')");
    await page.waitForFunction(() => [...document.querySelectorAll(".page-info")].some((e) => (e as HTMLElement).offsetParent !== null && e.textContent?.startsWith("Sayfa 1 /")));
  }));
  await step("open_detail", async () => {
    await visible(page, "product-name").first().locator("a").click();
    await visible(page, "product-detail").waitFor();
    await settle(page);
  });
  await step("add_to_cart", () => clientSide(page, async () => {
    await visible(page, "add-to-cart").first().click();
    await waitText(page, "cart-badge", "1");
  }));
  await step("open_cart", async () => {
    await page.locator(".cart-link").click();
    await page.waitForFunction(() => document.querySelectorAll('[data-testid="cart-row"]').length === 1 && location.pathname.endsWith("/cart"));
    await settle(page);
  });
  await step("qty_2", () => clientSide(page, async () => {
    await visible(page, "cart-qty").fill("2");
    await visible(page, "cart-update").click();
    await waitText(page, "cart-badge", "2");
  }));
  await step("remove", () => clientSide(page, async () => {
    await visible(page, "cart-remove").click();
    await visible(page, "empty-state").waitFor();
    await waitText(page, "cart-badge", "0");
  }));
}
