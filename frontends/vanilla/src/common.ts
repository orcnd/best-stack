import type { Cart } from "../../shared/types";
export const BASE = "/vanilla";
export function setBadge(count: number) {
  const b = document.getElementById("cart-badge");
  if (b) b.textContent = String(count);
}
export type { Cart };
