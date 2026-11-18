import { esc, money, num } from "../../shared/format";
import { api, catalogQuery, type Cart, type ProductPage } from "../../shared/types";
import { BASE, setBadge } from "./common";

const results = document.getElementById("product-results");
const form = document.querySelector<HTMLFormElement>('[data-testid="search-form"]');
const cartSection = document.getElementById("cart");

function renderResults(p: ProductPage, q: { q: string; cat: string; sort: string }): string {
  const href = (page: number) => `${BASE}/catalog${catalogQuery({ ...q, page })}`;
  const cards = p.items
    .map(
      (it) => `
    <article class="product-card" data-testid="product-card">
      <a class="thumb" href="${BASE}/catalog/p/${it.id}"><img src="${esc(it.image)}" alt="${esc(it.name)}" width="320" height="240"></a>
      <h2 class="product-name" data-testid="product-name"><a href="${BASE}/catalog/p/${it.id}">${esc(it.name)}</a></h2>
      <p class="product-category">${esc(it.category)}</p>
      <p class="product-price" data-testid="product-price">${money(it.price_cents)}</p>
      <form class="add-form" method="post" action="${BASE}/catalog/cart">
        <input type="hidden" name="product_id" value="${it.id}">
        <input type="hidden" name="qty" value="1">
        <button type="submit" data-testid="add-to-cart">Sepete ekle</button>
      </form>
    </article>`,
    )
    .join("");
  const prev = p.page > 1
    ? `<a data-testid="page-prev" href="${esc(href(p.page - 1))}">‹ Önceki</a>`
    : `<span class="disabled">‹ Önceki</span>`;
  const next = p.page < p.pages
    ? `<a data-testid="page-next" href="${esc(href(p.page + 1))}">Sonraki ›</a>`
    : `<span class="disabled">Sonraki ›</span>`;
  return `
  <p class="result-count">${num(p.total)} ürün</p>
  ${p.items.length ? `<div class="grid" data-testid="product-grid">${cards}</div>` : `<p class="empty" data-testid="empty-state">Sonuç bulunamadı.</p>`}
  <nav class="pagination" data-testid="pagination">
    ${prev}
    <span class="page-info">Sayfa ${p.page} / ${p.pages}</span>
    ${next}
  </nav>`;
}

let inflight: AbortController | null = null;
async function load(params: URLSearchParams, push: boolean) {
  if (!results) return;
  const q = { q: params.get("q") ?? "", cat: params.get("cat") ?? "", sort: params.get("sort") ?? "" };
  const page = Number(params.get("page")) || 1;
  inflight?.abort();
  inflight = new AbortController();
  const data = await api<ProductPage>(`/api/catalog/products${catalogQuery({ ...q, page })}`, { signal: inflight.signal });
  results.innerHTML = renderResults(data, q);
  if (push) history.pushState(null, "", `${BASE}/catalog${catalogQuery({ ...q, page: data.page })}`);
}

const formParams = () => {
  const fd = new FormData(form!);
  const p = new URLSearchParams();
  for (const k of ["q", "cat", "sort"]) p.set(k, String(fd.get(k) ?? ""));
  return p;
};

if (form && results) {
  let timer: ReturnType<typeof setTimeout> | undefined;
  form.addEventListener("submit", (e) => { e.preventDefault(); load(formParams(), true); });
  form.addEventListener("input", (e) => {
    if ((e.target as HTMLElement).getAttribute("name") !== "q") return;
    clearTimeout(timer);
    timer = setTimeout(() => load(formParams(), true), 250);
  });
  form.addEventListener("change", (e) => {
    if ((e.target as HTMLElement).tagName === "SELECT") load(formParams(), true);
  });
  results.addEventListener("click", (e) => {
    const a = (e.target as HTMLElement).closest<HTMLAnchorElement>("a[data-testid=page-prev], a[data-testid=page-next]");
    if (!a) return;
    e.preventDefault();
    load(new URL(a.href).searchParams, true);
  });
  window.addEventListener("popstate", () => load(new URLSearchParams(location.search), false));
}

// add to cart (list + detail)
document.addEventListener("submit", async (e) => {
  const f = e.target as HTMLFormElement;
  if (!f.classList.contains("add-form")) return;
  e.preventDefault();
  const cart = await api<Cart>("/api/catalog/cart", {
    method: "POST",
    json: { product_id: Number(new FormData(f).get("product_id")), qty: 1 },
  });
  setBadge(cart.count);
});

function renderCart(c: Cart): string {
  if (!c.items.length) return `<p class="empty" data-testid="empty-state">Sepetiniz boş.</p>`;
  const rows = c.items
    .map(
      (it) => `
      <tr data-testid="cart-row">
        <td><a href="${BASE}/catalog/p/${it.product.id}">${esc(it.product.name)}</a></td>
        <td>${money(it.product.price_cents)}</td>
        <td><form class="qty-form" method="post" action="${BASE}/catalog/cart/${it.id}">
              <input type="number" name="qty" min="0" max="99" value="${it.qty}" data-testid="cart-qty">
              <button type="submit" data-testid="cart-update">Güncelle</button></form></td>
        <td>${money(it.qty * it.product.price_cents)}</td>
        <td><form method="post" action="${BASE}/catalog/cart/${it.id}">
              <input type="hidden" name="qty" value="0">
              <button type="submit" data-testid="cart-remove">Kaldır</button></form></td>
      </tr>`,
    )
    .join("");
  return `
  <table class="cart-table" data-testid="cart-table">
    <thead><tr><th>Ürün</th><th>Fiyat</th><th>Adet</th><th>Tutar</th><th></th></tr></thead>
    <tbody>${rows}
    </tbody>
  </table>
  <p class="cart-total">Toplam: <strong data-testid="cart-total">${money(c.total_cents)}</strong></p>`;
}

cartSection?.addEventListener("submit", async (e) => {
  const f = e.target as HTMLFormElement;
  e.preventDefault();
  const id = f.action.split("/").pop();
  const qty = Number(new FormData(f).get("qty"));
  const cart = await api<Cart>(`/api/catalog/cart/${id}`, { method: "PATCH", json: { qty } });
  cartSection.innerHTML = renderCart(cart);
  setBadge(cart.count);
});
