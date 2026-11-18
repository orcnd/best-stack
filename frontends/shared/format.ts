// tr-TR formatting, byte-identical to shared/cpp/src/render.cpp.
const group = (v: number): string => {
  const s = String(Math.abs(Math.trunc(v)));
  let out = "";
  for (let i = 0; i < s.length; i++) {
    if (i && (s.length - i) % 3 === 0) out += ".";
    out += s[i];
  }
  return (v < 0 ? "-" : "") + out;
};

export const num = (v: number): string => group(v);
export const money = (cents: number): string =>
  "₺" + group(Math.trunc(cents / 100)) + "," + String(Math.abs(cents % 100)).padStart(2, "0");
export const pct = (ratio: number): string => "%" + (Math.round(ratio * 1000) / 10).toFixed(1).replace(".", ",");
export const rating = (r: number): string => "★ " + r.toFixed(1).replace(".", ",");

const ESC: Record<string, string> = { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" };
export const esc = (s: unknown): string => String(s).replace(/[&<>"']/g, (c) => ESC[c]);
