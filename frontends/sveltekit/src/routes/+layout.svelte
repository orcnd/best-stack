<script lang="ts">
  import { page } from "$app/state";
  let { data, children } = $props();
  const section = $derived(page.url.pathname.split("/")[1]);
</script>

<header class="site-header" data-testid="site-header">
  <a class="brand" href="/catalog">Best Stack</a>
  <nav class="main-nav">
    <a data-testid="nav-catalog" href="/catalog" class={section === "catalog" ? "active" : undefined}>Katalog</a>
    <a data-testid="nav-kanban" href="/kanban/1" class={section === "kanban" ? "active" : undefined}>Kanban</a>
    <a data-testid="nav-dashboard" href="/dashboard" class={section === "dashboard" ? "active" : undefined}>Dashboard</a>
  </nav>
  <a class="cart-link" href="/catalog/cart">Sepet <span class="badge" id="cart-badge" data-testid="cart-badge">{data.cartCount}</span></a>
</header>
<main class="container">
  {@render children()}
</main>
