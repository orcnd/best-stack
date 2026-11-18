export const CPP_PORT = 18080;
export const BUN_PORT = 13000;
export const API = `http://localhost:${CPP_PORT}`;
export const CPP_FES = ["htmx", "alpine", "vanilla"] as const;
export const BUN_FES = ["sveltekit", "astro"] as const;
export const FRONTENDS = [...CPP_FES, ...BUN_FES] as const;
export type Frontend = (typeof FRONTENDS)[number];
export const USECASES = ["catalog", "kanban", "dashboard"] as const;
export type Usecase = (typeof USECASES)[number];

/** Root URL of a frontend; flows append /catalog, /kanban/1, /dashboard. */
export function frontendRoot(fe: Frontend): string {
  return (CPP_FES as readonly string[]).includes(fe) ? `${API}/${fe}` : `http://localhost:${BUN_PORT}`;
}

export const isBunFrontend = (fe: string) => (BUN_FES as readonly string[]).includes(fe);
