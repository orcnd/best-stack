import { run as catalog } from "./catalog.ts";
import { run as dashboard } from "./dashboard.ts";
import { run as kanban } from "./kanban.ts";
import type { Flow } from "./types.ts";
export const FLOWS: Record<string, Flow> = { catalog, kanban, dashboard };
