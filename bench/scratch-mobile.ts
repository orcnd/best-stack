import { chromium } from "playwright";
import { timedFlow } from "./harness/interactions.ts";
const b = await chromium.launch();
const fes = process.argv.slice(2);
for (const fe of fes) for (const uc of ["catalog", "kanban", "dashboard"]) for (const prof of ["desktop", "mobile"] as const)
  console.log(fe, uc, prof, JSON.stringify((await timedFlow(b, fe as never, uc, prof)).map((s) => `${s.step}:${s.ok ? Math.round(s.ms) : "FAIL"}`)));
await b.close();
