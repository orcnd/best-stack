// Server-side helpers: C++ API base, session cookie, form-post handling.
import type { AstroCookies } from "astro";

export const API_BASE = () => process.env.API_BASE ?? "http://localhost:18080";

export function sessionId(cookies: AstroCookies): string {
  let sid = cookies.get("bs_sid")?.value;
  if (!sid || !/^[a-zA-Z0-9]{16}$/.test(sid)) {
    sid = [...crypto.getRandomValues(new Uint8Array(8))].map((b) => b.toString(16).padStart(2, "0")).join("");
    cookies.set("bs_sid", sid, { path: "/", sameSite: "lax", httpOnly: true });
  }
  return sid;
}

export async function api<T>(path: string, sid: string, init?: { method?: string; json?: unknown }): Promise<{ status: number; data: T }> {
  const res = await fetch(API_BASE() + path, {
    method: init?.method ?? "GET",
    headers: { "x-session-id": sid, ...(init?.json !== undefined ? { "content-type": "application/json" } : {}) },
    body: init?.json !== undefined ? JSON.stringify(init.json) : undefined,
  });
  const text = await res.text();
  return { status: res.status, data: (text ? JSON.parse(text) : undefined) as T };
}
