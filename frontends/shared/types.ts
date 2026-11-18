export interface Category { id: number; name: string; slug: string }
export interface Product {
  id: number; category_id: number; category: string; name: string; description: string;
  image: string; price_cents: number; stock: number; rating: number;
}
export interface ProductPage { items: Product[]; total: number; page: number; pages: number; categories?: Category[] }
export interface CartItem { id: number; qty: number; product: Product }
export interface Cart { items: CartItem[]; count: number; total_cents: number }
export interface Card {
  id: number; board_id: number; position: number; column: string; title: string;
  description: string; assignee: string; due_date: string; labels: string[];
}
export interface BoardSummary { id: number; name: string; card_count: number }
export interface Board { id: number; name: string; columns: Record<string, Card[]> }
export interface Dashboard {
  range_days: number;
  kpis: { events: number; users: number; revenue_cents: number; conversion: number };
  daily: { day: string; events: number; revenue_cents: number }[];
  countries: { country: string; events: number }[];
  top_items: { item: string; revenue_cents: number; purchases: number }[];
}
export const COLUMNS = ["backlog", "todo", "doing", "done"] as const;
export const COLUMN_LABELS: Record<string, string> = { backlog: "Backlog", todo: "Yapılacak", doing: "Yapılıyor", done: "Bitti" };

/** Query string in the canonical order q, cat, sort, page (page only when > 1). */
export function catalogQuery(p: { q?: string; cat?: string; sort?: string; page?: number }): string {
  const parts: string[] = [];
  const add = (k: string, v?: string) => { if (v) parts.push(`${k}=${encodeURIComponent(v)}`); };
  add("q", p.q); add("cat", p.cat); add("sort", p.sort);
  if (p.page && p.page > 1) add("page", String(p.page));
  return parts.length ? "?" + parts.join("&") : "";
}

export async function api<T>(url: string, init?: RequestInit & { json?: unknown }): Promise<T> {
  const res = await fetch(url, {
    ...init,
    credentials: "same-origin",
    headers: init?.json !== undefined ? { "content-type": "application/json" } : undefined,
    body: init?.json !== undefined ? JSON.stringify(init.json) : init?.body,
  });
  if (!res.ok) throw new Error(`${res.status} ${url}`);
  return (res.status === 204 ? undefined : await res.json()) as T;
}
