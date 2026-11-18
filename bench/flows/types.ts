import type { Page } from "playwright";
export type StepFn = (name: string, fn: () => Promise<void>) => Promise<void>;
export type Flow = (page: Page, root: string, step: StepFn) => Promise<void>;
export const TIMEOUT = 10_000;
