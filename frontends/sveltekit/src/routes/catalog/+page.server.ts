import { api, sessionId } from "#lib/server/api";
import { catalogQuery, type ProductPage } from "#lib";
import type { Actions, PageServerLoad } from "./$types";

export const load: PageServerLoad = async ({ url, cookies, fetch }) => {
  const q = url.searchParams.get("q") ?? "";
  const cat = url.searchParams.get("cat") ?? "";
  const sort = url.searchParams.get("sort") ?? "";
  const page = Number(url.searchParams.get("page")) || 1;
  const { data } = await api<ProductPage>(fetch, `/api/catalog/products${catalogQuery({ q, cat, sort, page })}`, sessionId(cookies));
  return { q, cat, sort, products: data };
};

export const actions: Actions = {
  add: async ({ request, cookies, fetch }) => {
    const fd = await request.formData();
    await api(fetch, "/api/catalog/cart", sessionId(cookies), { method: "POST", json: { product_id: Number(fd.get("product_id")), qty: 1 } });
  },
};
