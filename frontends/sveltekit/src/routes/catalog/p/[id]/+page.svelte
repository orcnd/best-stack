<script lang="ts">
  import { enhance } from "$app/forms";
  import { money, rating } from "#lib";
  let { data } = $props();
  const p = $derived(data.product);
</script>

<svelte:head>
  <title>{p.name} · Best Stack</title>
  <meta name="description" content={p.description}>
</svelte:head>

<article class="product-detail" data-testid="product-detail">
  <img src={p.image} alt={p.name} width="320" height="240">
  <div class="info">
    <h1>{p.name}</h1>
    <p class="product-category">{p.category}</p>
    <p class="product-price" data-testid="product-price">{money(p.price_cents)}</p>
    <p class="rating">{rating(p.rating)}</p>
    <p class="stock">Stok: {p.stock}</p>
    <p class="description">{p.description}</p>
    <form class="add-form" method="post" action="?/add" use:enhance>
      <input type="hidden" name="product_id" value={p.id}>
      <input type="hidden" name="qty" value="1">
      <button type="submit" data-testid="add-to-cart">Sepete ekle</button>
    </form>
  </div>
</article>
<a class="back" href="/catalog">‹ Kataloğa dön</a>
