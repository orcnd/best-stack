import type { Page } from "playwright";

/** Pages compared pixel-by-pixel; `act` drives a client-side state change first. */
export const VISUAL_PAGES: { name: string; usecase: string; path: string; act?: (p: Page) => Promise<void> }[] = [
  { name: "catalog", usecase: "catalog", path: "/catalog" },
  { name: "catalog-filtered", usecase: "catalog", path: "/catalog", act: async (p) => {
    await p.locator('[data-testid="category-filter"]:visible').selectOption("mobilya");
    await p.waitForFunction(() => [...document.querySelectorAll(".product-category")].filter((e) => (e as HTMLElement).offsetParent).every((e) => e.textContent === "Mobilya"));
  } },
  { name: "product", usecase: "catalog", path: "/catalog/p/1" },
  { name: "cart-empty", usecase: "catalog", path: "/catalog/cart" },
  { name: "kanban", usecase: "kanban", path: "/kanban/1" },
  { name: "dashboard", usecase: "dashboard", path: "/dashboard" },
  { name: "dashboard-7", usecase: "dashboard", path: "/dashboard", act: async (p) => {
    await p.locator('[data-testid="range-select"]:visible').selectOption("7");
    await p.waitForFunction(() => [...document.querySelectorAll(".chart-svg")].some((e) => e.getClientRects().length > 0 && e.querySelectorAll("rect").length === 7));
  } },
];
