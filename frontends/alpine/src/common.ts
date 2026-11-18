import { dailyChartSvg } from "../../shared/chart";
import { money, num, pct } from "../../shared/format";

declare global {
  interface Window { Alpine: any }
}
export const BASE = "/alpine";
export const helpers = { money, num, pct, chart: dailyChartSvg };

/** Registers the cart badge store; call inside alpine:init. */
export function registerCartStore(Alpine: any) {
  const badge = document.getElementById("cart-badge");
  Alpine.store("cart", { count: Number(badge?.textContent ?? 0) });
}
