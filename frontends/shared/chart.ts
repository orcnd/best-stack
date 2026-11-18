// Port of shared/cpp/src/chart.cpp; output must be byte-identical.
const fmt = (v: number): string => {
  let s = (Math.round(v * 100) / 100).toFixed(2);
  s = s.replace(/0+$/, "").replace(/\.$/, "");
  return s === "-0" ? "0" : s;
};

export function dailyChartSvg(days: { events: number }[], width = 960, height = 240): string {
  const max = Math.max(0, ...days.map((d) => d.events)) || 1;
  const barW = width / (days.length || 1);
  let out = `<svg class="chart-svg" viewBox="0 0 ${width} ${height}" width="${width}" height="${height}" role="img" aria-label="Günlük olaylar">`;
  days.forEach((d, i) => {
    const h = Math.round((d.events / max) * (height - 20));
    out += `<rect x="${fmt(i * barW + 1)}" y="${height - h}" width="${fmt(barW - 2)}" height="${h}" fill="#4f6bed"/>`;
  });
  return out + "</svg>";
}
