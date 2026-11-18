import { useEffect, useRef, useState } from "preact/hooks";
import { api, catalogQuery, money, num, type ProductPage } from "../lib/client";
import AddToCart from "./AddToCart";

type Params = { q: string; cat: string; sort: string; page: number };

export default function CatalogBrowser({ initial, params: initialParams }: { initial: ProductPage; params: Params }) {
  const [params, setParams] = useState(initialParams);
  const [data, setData] = useState(initial);
  const ctrl = useRef<AbortController | null>(null);
  const timer = useRef<ReturnType<typeof setTimeout>>();
  const categories = initial.categories ?? [];

  async function load(next: Params, push: boolean) {
    setParams(next);
    ctrl.current?.abort();
    ctrl.current = new AbortController();
    const d = await api<ProductPage>(`/api/catalog/products${catalogQuery(next)}`, { signal: ctrl.current.signal });
    setData(d);
    if (push) history.pushState(null, "", `/catalog${catalogQuery({ ...next, page: d.page })}`);
  }

  useEffect(() => {
    const onPop = () => {
      const s = new URLSearchParams(location.search);
      load({ q: s.get("q") ?? "", cat: s.get("cat") ?? "", sort: s.get("sort") ?? "", page: Number(s.get("page")) || 1 }, false);
    };
    window.addEventListener("popstate", onPop);
    return () => window.removeEventListener("popstate", onPop);
  }, []);

  const href = (page: number) => `/catalog${catalogQuery({ ...params, page })}`;
  const go = (page: number) => (e: Event) => {
    e.preventDefault();
    load({ ...params, page }, true);
  };

  return (
    <>
      <form class="toolbar" data-testid="search-form" method="get" action="/catalog" onSubmit={(e) => { e.preventDefault(); load({ ...params, page: 1 }, true); }}>
        <input
          type="search" name="q" value={params.q} placeholder="Ürün ara…" data-testid="search-input"
          onInput={(e) => {
            const q = e.currentTarget.value;
            setParams((p) => ({ ...p, q }));
            clearTimeout(timer.current);
            timer.current = setTimeout(() => load({ ...params, q, page: 1 }, true), 250);
          }}
        />
        <select name="cat" data-testid="category-filter" onChange={(e) => load({ ...params, cat: e.currentTarget.value, page: 1 }, true)}>
          <option value="" selected={params.cat === ""}>Tüm kategoriler</option>
          {categories.map((c) => <option key={c.slug} value={c.slug} selected={c.slug === params.cat}>{c.name}</option>)}
        </select>
        <select name="sort" data-testid="sort-select" onChange={(e) => load({ ...params, sort: e.currentTarget.value, page: 1 }, true)}>
          <option value="" selected={params.sort === ""}>Varsayılan sıralama</option>
          <option value="price_asc" selected={params.sort === "price_asc"}>Fiyat (artan)</option>
          <option value="price_desc" selected={params.sort === "price_desc"}>Fiyat (azalan)</option>
        </select>
        <button type="submit">Ara</button>
      </form>
      <section id="product-results">
        <p class="result-count">{num(data.total)} ürün</p>
        {data.items.length > 0 ? (
          <div class="grid" data-testid="product-grid">
            {data.items.map((it) => (
              <article key={it.id} class="product-card" data-testid="product-card">
                <a class="thumb" href={`/catalog/p/${it.id}`}><img src={it.image} alt={it.name} width={320} height={240} /></a>
                <h2 class="product-name" data-testid="product-name"><a href={`/catalog/p/${it.id}`}>{it.name}</a></h2>
                <p class="product-category">{it.category}</p>
                <p class="product-price" data-testid="product-price">{money(it.price_cents)}</p>
                <AddToCart productId={it.id} />
              </article>
            ))}
          </div>
        ) : (
          <p class="empty" data-testid="empty-state">Sonuç bulunamadı.</p>
        )}
        <nav class="pagination" data-testid="pagination">
          {data.page > 1
            ? <a data-testid="page-prev" href={href(data.page - 1)} onClick={go(data.page - 1)}>‹ Önceki</a>
            : <span class="disabled">‹ Önceki</span>}
          <span class="page-info">Sayfa {data.page} / {data.pages}</span>
          {data.page < data.pages
            ? <a data-testid="page-next" href={href(data.page + 1)} onClick={go(data.page + 1)}>Sonraki ›</a>
            : <span class="disabled">Sonraki ›</span>}
        </nav>
      </section>
    </>
  );
}
