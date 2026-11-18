<script lang="ts">
  import { goto } from "$app/navigation";
  import { dailyChartSvg, money, num, pct } from "#lib";
  let { data } = $props();
  const d = $derived(data.d);
  const ranges = [["7", "Son 7 gün"], ["30", "Son 30 gün"], ["90", "Son 90 gün"]];
  const go = (range: string) => goto(`/dashboard?range=${range}`, { keepFocus: true, noScroll: true });
</script>

<svelte:head>
  <title>Analitik · Best Stack</title>
  <meta name="description" content="Analitik dashboard">
</svelte:head>

<div class="dash-head">
  <h1>Analitik</h1>
  <form class="range-form" method="get" action="/dashboard" onsubmit={(e) => { e.preventDefault(); go(String(new FormData(e.currentTarget).get("range"))); }}>
    <select name="range" data-testid="range-select" onchange={(e) => go(e.currentTarget.value)}>
      {#each ranges as [v, label] (v)}
        <option value={v} selected={v === data.range}>{label}</option>
      {/each}
    </select>
    <button type="submit">Uygula</button>
  </form>
</div>
<section id="dashboard-body">
  <div class="kpis">
    <div class="kpi"><span class="kpi-label">Olay</span><strong data-testid="kpi-events">{num(d.kpis.events)}</strong></div>
    <div class="kpi"><span class="kpi-label">Tekil kullanıcı</span><strong data-testid="kpi-users">{num(d.kpis.users)}</strong></div>
    <div class="kpi"><span class="kpi-label">Gelir</span><strong data-testid="kpi-revenue">{money(d.kpis.revenue_cents)}</strong></div>
    <div class="kpi"><span class="kpi-label">Dönüşüm</span><strong data-testid="kpi-conversion">{pct(d.kpis.conversion)}</strong></div>
  </div>
  <div class="panel"><h2>Günlük olaylar</h2><div class="chart" data-testid="chart-daily">{@html dailyChartSvg(d.daily)}</div></div>
  <div class="two-col">
    <div class="panel"><h2>Ülkeler</h2>
      <table data-testid="country-table"><thead><tr><th>Ülke</th><th>Olay</th></tr></thead>
        <tbody>{#each d.countries as c (c.country)}<tr><td>{c.country}</td><td>{num(c.events)}</td></tr>{/each}</tbody></table></div>
    <div class="panel"><h2>En çok gelir getirenler</h2>
      <table data-testid="top-items-table"><thead><tr><th>Ürün</th><th>Satış</th><th>Gelir</th></tr></thead>
        <tbody>{#each d.top_items as it (it.item)}<tr><td>{it.item}</td><td>{num(it.purchases)}</td><td>{money(it.revenue_cents)}</td></tr>{/each}</tbody></table></div>
  </div>
</section>
