import { error } from "@sveltejs/kit";
import { api, sessionId } from "#lib/server/api";
import type { Product } from "#lib";
import type { Actions, PageServerLoad } from "./$types";

export const load: PageServerLoad = async ({ params, cookies, fetch }) => {
  if (!/^\d+$/.test(params.id)) error(404, "Bulunamadı");
  const { status, data } = await api<Product>(fetch, `/api/catalog/products/${params.id}`, sessionId(cookies));
  if (status !== 200) error(404, "Bulunamadı");
  return { product: data };
};

export const actions: Actions = {
  add: async ({ request, cookies, fetch }) => {
    const fd = await request.formData();
    await api(fetch, "/api/catalog/cart", sessionId(cookies), { method: "POST", json: { product_id: Number(fd.get("product_id")), qty: 1 } });
  },
};
