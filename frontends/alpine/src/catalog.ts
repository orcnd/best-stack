import { api, catalogQuery, type Cart, type ProductPage } from "../../shared/types";
import { BASE, helpers, registerCartStore } from "./common";

document.addEventListener("alpine:init", () => {
  const Alpine = window.Alpine;
  registerCartStore(Alpine);
  Alpine.data("shop", () => ({
    ...helpers,
    ready: false,
    q: "",
    cat: "",
    sort: "",
    page: 1,
    pages: 1,
    total: 0,
    items: [] as ProductPage["items"],
    cart: { items: [], count: 0, total_cents: 0 } as Cart,
    ctrl: null as AbortController | null,
    init() {
      const p = new URLSearchParams(location.search);
      this.q = p.get("q") ?? "";
      this.cat = p.get("cat") ?? "";
      this.sort = p.get("sort") ?? "";
      this.page = Number(p.get("page")) || 1;
      window.addEventListener("popstate", () => {
        const s = new URLSearchParams(location.search);
        this.q = s.get("q") ?? "";
        this.cat = s.get("cat") ?? "";
        this.sort = s.get("sort") ?? "";
        this.page = Number(s.get("page")) || 1;
        this.load(false);
      });
    },
    hrefFor(page: number) {
      return `${BASE}/catalog${catalogQuery({ q: this.q, cat: this.cat, sort: this.sort, page })}`;
    },
    search() {
      this.page = 1;
      return this.load(true);
    },
    go(page: number) {
      this.page = page;
      return this.load(true);
    },
    async load(push: boolean) {
      this.ctrl?.abort();
      this.ctrl = new AbortController();
      const qs = catalogQuery({ q: this.q, cat: this.cat, sort: this.sort, page: this.page });
      const d = await api<ProductPage>(`/api/catalog/products${qs}`, { signal: this.ctrl.signal });
      this.items = d.items;
      this.total = d.total;
      this.page = d.page;
      this.pages = d.pages;
      this.ready = true;
      if (push) history.pushState(null, "", this.hrefFor(d.page));
    },
    async add(productId: number) {
      const c = await api<Cart>("/api/catalog/cart", { method: "POST", json: { product_id: productId, qty: 1 } });
      Alpine.store("cart").count = c.count;
    },
    async setQty(itemId: number, qty: string | number) {
      const c = await api<Cart>(`/api/catalog/cart/${itemId}`, { method: "PATCH", json: { qty: Number(qty) } });
      this.cart = c;
      this.ready = true;
      Alpine.store("cart").count = c.count;
    },
  }));
});
