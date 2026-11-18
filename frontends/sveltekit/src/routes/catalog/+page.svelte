<script lang="ts">
  import { enhance } from "$app/forms";
  import { goto } from "$app/navigation";
  import { catalogQuery, money, num } from "#lib";

  let { data } = $props();
  let form: HTMLFormElement;
  let timer: ReturnType<typeof setTimeout> | undefined;
  const p = $derived(data.products);
  const href = (page: number) => `/catalog${catalogQuery({ q: data.q, cat: data.cat, sort: data.sort, page })}`;

  function navigate() {
    const fd = new FormData(form);
    const params = { q: String(fd.get("q") ?? ""), cat: String(fd.get("cat") ?? ""), sort: String(fd.get("sort") ?? "") };
    goto(`/catalog${catalogQuery(params)}`, { keepFocus: true, noScroll: true });
  }
  function onSearchInput() {
    clearTimeout(timer);
    timer = setTimeout(navigate, 250);
  }
</script>

<svelte:head>
  <title>Ürün Kataloğu · Best Stack</title>
  <meta name="description" content="Best Stack demo ürün kataloğu">
</svelte:head>

<h1>Ürün Kataloğu</h1>
<form class="toolbar" data-testid="search-form" method="get" action="/catalog" bind:this={form} onsubmit={(e) => { e.preventDefault(); navigate(); }}>
  <input type="search" name="q" value={data.q} placeholder="Ürün ara…" data-testid="search-input" oninput={onSearchInput}>
  <select name="cat" data-testid="category-filter" onchange={navigate}>
    <option value="" selected={data.cat === ""}>Tüm kategoriler</option>
    {#each p.categories ?? [] as c (c.slug)}
      <option value={c.slug} selected={c.slug === data.cat}>{c.name}</option>
    {/each}
  </select>
  <select name="sort" data-testid="sort-select" onchange={navigate}>
    <option value="" selected={data.sort === ""}>Varsayılan sıralama</option>
    <option value="price_asc" selected={data.sort === "price_asc"}>Fiyat (artan)</option>
    <option value="price_desc" selected={data.sort === "price_desc"}>Fiyat (azalan)</option>
  </select>
  <button type="submit">Ara</button>
</form>
<section id="product-results">
  <p class="result-count">{num(p.total)} ürün</p>
  {#if p.items.length > 0}
    <div class="grid" data-testid="product-grid">
      {#each p.items as it (it.id)}
        <article class="product-card" data-testid="product-card">
          <a class="thumb" href="/catalog/p/{it.id}"><img src={it.image} alt={it.name} width="320" height="240"></a>
          <h2 class="product-name" data-testid="product-name"><a href="/catalog/p/{it.id}">{it.name}</a></h2>
          <p class="product-category">{it.category}</p>
          <p class="product-price" data-testid="product-price">{money(it.price_cents)}</p>
          <form class="add-form" method="post" action="?/add" use:enhance>
            <input type="hidden" name="product_id" value={it.id}>
            <input type="hidden" name="qty" value="1">
            <button type="submit" data-testid="add-to-cart">Sepete ekle</button>
          </form>
        </article>
      {/each}
    </div>
  {:else}
    <p class="empty" data-testid="empty-state">Sonuç bulunamadı.</p>
  {/if}
  <nav class="pagination" data-testid="pagination">
    {#if p.page > 1}
      <a data-testid="page-prev" href={href(p.page - 1)}>‹ Önceki</a>
    {:else}
      <span class="disabled">‹ Önceki</span>
    {/if}
    <span class="page-info">Sayfa {p.page} / {p.pages}</span>
    {#if p.page < p.pages}
      <a data-testid="page-next" href={href(p.page + 1)}>Sonraki ›</a>
    {:else}
      <span class="disabled">Sonraki ›</span>
    {/if}
  </nav>
</section>
