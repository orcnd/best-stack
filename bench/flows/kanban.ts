import type { Page } from "playwright";
import { API } from "./targets.ts";
import { clientSide, instrument, settle, type StepFn } from "./types.ts";

/** Visible card whose title input currently has value `title`, optionally inside a column. */
const findCard = (page: Page, title: string, column?: string) =>
  page.waitForFunction(
    ([title, column]) => {
      const scope = column ? document.querySelectorAll(`[data-testid="column-${column}"]`) : [document];
      for (const s of scope) {
        if (s instanceof HTMLElement && s.offsetParent === null) continue;
        for (const li of s.querySelectorAll('[data-testid="card"]')) {
          const input = li.querySelector<HTMLInputElement>('[data-testid="card-title"]');
          if ((li as HTMLElement).offsetParent !== null && input?.value === title) return true;
        }
      }
      return false;
    },
    [title, column ?? ""],
    {},
  );

async function cardHandle(page: Page, title: string) {
  const h = await page.evaluateHandle((title) => {
    for (const li of document.querySelectorAll('[data-testid="card"]')) {
      const input = li.querySelector<HTMLInputElement>('[data-testid="card-title"]');
      if ((li as HTMLElement).offsetParent !== null && input?.value === title) return li;
    }
    return null;
  }, title);
  const el = h.asElement();
  if (!el) throw new Error(`card not found: ${title}`);
  return el;
}

/** Click a control inside the card titled `title`; re-find the card if a re-render detached it. */
async function clickInCard(page: Page, title: string, testid: string) {
  for (let attempt = 0; attempt < 20; attempt++) {
    try {
      const card = await cardHandle(page, title);
      await (await card.$(`[data-testid="${testid}"]`))!.click({ timeout: 2000 });
      return;
    } catch (e) {
      if (!/not attached|detached|card not found/.test(String(e))) throw e;
      await page.waitForTimeout(10);
    }
  }
  throw new Error(`could not click ${testid} in card ${title}`);
}

async function apiHasTitle(title: string, present: boolean) {
  const deadline = Date.now() + 30_000;
  while (Date.now() < deadline) {
    const b = await (await fetch(`${API}/api/kanban/boards/1`)).json();
    const found = Object.values(b.columns as Record<string, { title: string }[]>).flat().some((c) => c.title === title);
    if (found === present) return;
    await new Promise((r) => setTimeout(r, 5));
  }
  throw new Error(`api title ${present ? "missing" : "still present"}: ${title}`);
}

const visibleCardCount = (page: Page) =>
  page.evaluate(() => [...document.querySelectorAll('[data-testid="card"]')].filter((e) => (e as HTMLElement).offsetParent !== null).length);

export async function run(page: Page, root: string, step: StepFn) {
  await instrument(page);
  const tag = Math.random().toString(36).slice(2, 8);
  const title = `Bench card ${tag}`;
  const renamed = `Bench renamed ${tag}`;
  await step("load_board", async () => {
    await page.goto(`${root}/kanban/1`);
    await page.waitForFunction(() => document.querySelectorAll('[data-testid="card"]').length >= 100);
    await settle(page);
  });
  const startCount = await visibleCardCount(page);
  await step("add_card", () => clientSide(page, async () => {
    await page.locator('[data-testid="new-card-input"]:visible').fill(title);
    await page.locator('[data-testid="new-card-submit"]:visible').click();
    await findCard(page, title, "backlog");
  }));
  await step("edit_title", () => clientSide(page, async () => {
    const card = await cardHandle(page, title);
    const input = await card.$('[data-testid="card-title"]');
    await input!.fill(renamed);
    await (await card.$('[data-testid="card-edit"]'))!.click();
    await apiHasTitle(renamed, true);
    await findCard(page, renamed, "backlog");
  }));
  await step("move_right", () => clientSide(page, async () => {
    await clickInCard(page, renamed, "card-move-right");
    await findCard(page, renamed, "todo");
  }));
  await step("delete", () => clientSide(page, async () => {
    await clickInCard(page, renamed, "card-delete");
    await page.waitForFunction(
      (t) => ![...document.querySelectorAll<HTMLInputElement>('[data-testid="card-title"]')].some((i) => i.offsetParent !== null && i.value === t),
      renamed,
    );
  }));
  const endCount = await visibleCardCount(page);
  if (endCount !== startCount) throw new Error(`card count changed: ${startCount} -> ${endCount}`);
}
