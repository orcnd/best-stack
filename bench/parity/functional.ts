// Usage: node bench/parity/functional.ts <frontend> [usecase...]
// Runs the user flows in a fresh browser context each; prints JSON {ok, failures}.
import { chromium } from "playwright";
import { FLOWS } from "../flows/index.ts";
import { type Frontend, frontendRoot, USECASES } from "../flows/targets.ts";

export async function functional(fe: Frontend, usecases: readonly string[] = USECASES) {
  const browser = await chromium.launch();
  const failures: { usecase: string; step: string; error: string }[] = [];
  try {
    for (const uc of usecases) {
      const ctx = await browser.newContext({ viewport: { width: 1280, height: 900 } });
      const page = await ctx.newPage();
      let current = "";
      try {
        await FLOWS[uc](page, frontendRoot(fe), async (name, fn) => {
          current = name;
          await fn();
        });
      } catch (e) {
        failures.push({ usecase: uc, step: current, error: String((e as Error).message).split("\n")[0] });
      } finally {
        await ctx.close();
      }
    }
  } finally {
    await browser.close();
  }
  return { ok: failures.length === 0, failures };
}

if (import.meta.main) {
  const [fe, ...ucs] = process.argv.slice(2);
  const res = await functional(fe as Frontend, ucs.length ? ucs : USECASES);
  console.log(JSON.stringify({ frontend: fe, ...res }));
  process.exit(res.ok ? 0 : 1);
}
