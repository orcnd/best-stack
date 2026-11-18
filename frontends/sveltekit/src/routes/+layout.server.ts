import { api, sessionId } from "#lib/server/api";
import type { Cart } from "#lib";
import type { LayoutServerLoad } from "./$types";

export const load: LayoutServerLoad = async ({ cookies, fetch, depends }) => {
  depends("app:cart");
  const { data } = await api<Cart>(fetch, "/api/catalog/cart", sessionId(cookies));
  return { cartCount: data.count };
};
