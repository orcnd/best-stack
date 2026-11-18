<script lang="ts">
  import { enhance } from "$app/forms";
  import { money } from "#lib";
  let { data } = $props();
  const cart = $derived(data.cart);
</script>

<svelte:head>
  <title>Sepet · Best Stack</title>
  <meta name="description" content="Alışveriş sepeti">
</svelte:head>

<h1>Sepet</h1>
<section id="cart">
  {#if cart.items.length > 0}
    <table class="cart-table" data-testid="cart-table">
      <thead><tr><th>Ürün</th><th>Fiyat</th><th>Adet</th><th>Tutar</th><th></th></tr></thead>
      <tbody>
        {#each cart.items as it (it.id)}
          <tr data-testid="cart-row">
            <td><a href="/catalog/p/{it.product.id}">{it.product.name}</a></td>
            <td>{money(it.product.price_cents)}</td>
            <td><form class="qty-form" method="post" action="?/qty" use:enhance={() => async ({ update }) => update({ reset: false })}>
                  <input type="hidden" name="item_id" value={it.id}>
                  <input type="number" name="qty" min="0" max="99" value={it.qty} data-testid="cart-qty">
                  <button type="submit" data-testid="cart-update">Güncelle</button></form></td>
            <td>{money(it.qty * it.product.price_cents)}</td>
            <td><form method="post" action="?/qty" use:enhance>
                  <input type="hidden" name="item_id" value={it.id}>
                  <input type="hidden" name="qty" value="0">
                  <button type="submit" data-testid="cart-remove">Kaldır</button></form></td>
          </tr>
        {/each}
      </tbody>
    </table>
    <p class="cart-total">Toplam: <strong data-testid="cart-total">{money(cart.total_cents)}</strong></p>
  {:else}
    <p class="empty" data-testid="empty-state">Sepetiniz boş.</p>
  {/if}
</section>
