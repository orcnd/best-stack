import { api, sessionId } from "#lib/server/api";
import type { Cart } from "#lib";
import type { Actions, PageServerLoad } from "./$types";

export const load: PageServerLoad = async ({ cookies, fetch }) => {
  const { data } = await api<Cart>(fetch, "/api/catalog/cart", sessionId(cookies));
  return { cart: data };
};

export const actions: Actions = {
  qty: async ({ request, cookies, fetch }) => {
    const fd = await request.formData();
    const qty = Math.max(0, Math.min(99, Number(fd.get("qty")) || 0));
    await api(fetch, `/api/catalog/cart/${Number(fd.get("item_id"))}`, sessionId(cookies), { method: "PATCH", json: { qty } });
  },
};
