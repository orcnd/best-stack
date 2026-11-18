// Browser-side helpers shared by islands (same-origin /api proxy, cart badge event).
export { api, catalogQuery, COLUMN_LABELS, COLUMNS } from "../../../shared/types";
export type { Board, Card, Cart, Dashboard, ProductPage } from "../../../shared/types";
export { dailyChartSvg } from "../../../shared/chart";
export { money, num, pct, rating } from "../../../shared/format";

export const announceCart = (count: number) => window.dispatchEvent(new CustomEvent("cart:count", { detail: count }));
