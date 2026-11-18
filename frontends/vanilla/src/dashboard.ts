import { dailyChartSvg } from "../../shared/chart";
import { esc, money, num, pct } from "../../shared/format";
import { api, type Dashboard } from "../../shared/types";
import { BASE } from "./common";

const body = document.getElementById("dashboard-body");
const form = document.querySelector<HTMLFormElement>(".range-form");

function render(d: Dashboard): string {
  return `
  <div class="kpis">
    <div class="kpi"><span class="kpi-label">Olay</span><strong data-testid="kpi-events">${num(d.kpis.events)}</strong></div>
    <div class="kpi"><span class="kpi-label">Tekil kullanıcı</span><strong data-testid="kpi-users">${num(d.kpis.users)}</strong></div>
    <div class="kpi"><span class="kpi-label">Gelir</span><strong data-testid="kpi-revenue">${money(d.kpis.revenue_cents)}</strong></div>
    <div class="kpi"><span class="kpi-label">Dönüşüm</span><strong data-testid="kpi-conversion">${pct(d.kpis.conversion)}</strong></div>
  </div>
  <div class="panel"><h2>Günlük olaylar</h2><div class="chart" data-testid="chart-daily">${dailyChartSvg(d.daily)}</div></div>
  <div class="two-col">
    <div class="panel"><h2>Ülkeler</h2>
      <table data-testid="country-table"><thead><tr><th>Ülke</th><th>Olay</th></tr></thead>
        <tbody>${d.countries.map((c) => `<tr><td>${esc(c.country)}</td><td>${num(c.events)}</td></tr>`).join("")}</tbody></table></div>
    <div class="panel"><h2>En çok gelir getirenler</h2>
      <table data-testid="top-items-table"><thead><tr><th>Ürün</th><th>Satış</th><th>Gelir</th></tr></thead>
        <tbody>${d.top_items.map((it) => `<tr><td>${esc(it.item)}</td><td>${num(it.purchases)}</td><td>${money(it.revenue_cents)}</td></tr>`).join("")}</tbody></table></div>
  </div>`;
}

async function load(range: string, push: boolean) {
  const d = await api<Dashboard>(`/api/dashboard?range=${encodeURIComponent(range)}`);
  body!.innerHTML = render(d);
  if (push) history.pushState(null, "", `${BASE}/dashboard?range=${range}`);
}

form?.addEventListener("submit", (e) => { e.preventDefault(); load(String(new FormData(form).get("range")), true); });
form?.addEventListener("change", () => load(String(new FormData(form).get("range")), true));
window.addEventListener("popstate", () => {
  const r = new URLSearchParams(location.search).get("range") ?? "30";
  const sel = form?.querySelector<HTMLSelectElement>("select");
  if (sel) sel.value = r;
  load(r, false);
});
