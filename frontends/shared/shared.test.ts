import { expect, test } from "bun:test";
import { dailyChartSvg } from "./chart";
import { money, num, pct, rating } from "./format";

test("chart_deterministic (same expectation as C++ test_chart.cpp)", () => {
  const svg = dailyChartSvg([{ events: 10 }, { events: 20 }, { events: 5 }]);
  expect(svg).toBe(
    '<svg class="chart-svg" viewBox="0 0 960 240" width="960" height="240" role="img" aria-label="Günlük olaylar">' +
      '<rect x="1" y="130" width="318" height="110" fill="#4f6bed"/>' +
      '<rect x="321" y="20" width="318" height="220" fill="#4f6bed"/>' +
      '<rect x="641" y="185" width="318" height="55" fill="#4f6bed"/>' +
      "</svg>",
  );
});

test("chart fractional widths and empty data", () => {
  const svg = dailyChartSvg(Array.from({ length: 7 }, () => ({ events: 0 })));
  expect(svg).toContain('<rect x="138.14" y="240" width="135.14" height="0"');
});

test("tr-TR formatting matches C++", () => {
  expect(money(123450)).toBe("₺1.234,50");
  expect(money(5)).toBe("₺0,05");
  expect(money(123456789)).toBe("₺1.234.567,89");
  expect(num(12345)).toBe("12.345");
  expect(num(999)).toBe("999");
  expect(pct(0.1234)).toBe("%12,3");
  expect(rating(4.5)).toBe("★ 4,5");
});
