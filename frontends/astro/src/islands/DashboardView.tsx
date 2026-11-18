import { useEffect, useState } from "preact/hooks";
import { api, dailyChartSvg, money, num, pct, type Dashboard } from "../lib/client";

const RANGES: [string, string][] = [["7", "Son 7 gün"], ["30", "Son 30 gün"], ["90", "Son 90 gün"]];

export default function DashboardView({ initial, range: initialRange }: { initial: Dashboard; range: string }) {
  const [range, setRange] = useState(initialRange);
  const [d, setD] = useState(initial);
  const load = async (r: string, push: boolean) => {
    setRange(r);
    setD(await api<Dashboard>(`/api/dashboard?range=${r}`));
    if (push) history.pushState(null, "", `/dashboard?range=${r}`);
  };
  useEffect(() => {
    const onPop = () => load(new URLSearchParams(location.search).get("range") ?? "30", false);
    window.addEventListener("popstate", onPop);
    return () => window.removeEventListener("popstate", onPop);
  }, []);

  return (
    <>
      <div class="dash-head">
        <h1>Analitik</h1>
        <form class="range-form" method="get" action="/dashboard" onSubmit={(e) => { e.preventDefault(); load(range, true); }}>
          <select name="range" data-testid="range-select" onChange={(e) => load(e.currentTarget.value, true)}>
            {RANGES.map(([v, label]) => <option key={v} value={v} selected={v === range}>{label}</option>)}
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
        <div class="panel"><h2>Günlük olaylar</h2><div class="chart" data-testid="chart-daily" dangerouslySetInnerHTML={{ __html: dailyChartSvg(d.daily) }} /></div>
        <div class="two-col">
          <div class="panel"><h2>Ülkeler</h2>
            <table data-testid="country-table"><thead><tr><th>Ülke</th><th>Olay</th></tr></thead>
              <tbody>{d.countries.map((c) => <tr key={c.country}><td>{c.country}</td><td>{num(c.events)}</td></tr>)}</tbody></table></div>
          <div class="panel"><h2>En çok gelir getirenler</h2>
            <table data-testid="top-items-table"><thead><tr><th>Ürün</th><th>Satış</th><th>Gelir</th></tr></thead>
              <tbody>{d.top_items.map((it) => <tr key={it.item}><td>{it.item}</td><td>{num(it.purchases)}</td><td>{money(it.revenue_cents)}</td></tr>)}</tbody></table></div>
        </div>
      </section>
    </>
  );
}
