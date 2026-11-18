// Server-side client for the C++ JSON API. The browser never talks to C++ directly:
// session id lives in this origin's `bs_sid` cookie and is forwarded as x-session-id.
import { env } from "$env/dynamic/private";
import type { Cookies } from "@sveltejs/kit";

export const API_BASE = () => env.API_BASE ?? "http://localhost:18080";

export function sessionId(cookies: Cookies): string {
  let sid = cookies.get("bs_sid");
  if (!sid || !/^[a-zA-Z0-9]{16}$/.test(sid)) {
    sid = [...crypto.getRandomValues(new Uint8Array(8))].map((b) => b.toString(16).padStart(2, "0")).join("");
    cookies.set("bs_sid", sid, { path: "/", sameSite: "lax", httpOnly: true, secure: false });
  }
  return sid;
}

export async function api<T>(
  fetchFn: typeof fetch,
  path: string,
  sid: string,
  init?: { method?: string; json?: unknown },
): Promise<{ status: number; data: T }> {
  const res = await fetchFn(API_BASE() + path, {
    method: init?.method ?? "GET",
    headers: { "x-session-id": sid, ...(init?.json !== undefined ? { "content-type": "application/json" } : {}) },
    body: init?.json !== undefined ? JSON.stringify(init.json) : undefined,
  });
  const text = await res.text();
  return { status: res.status, data: (text ? JSON.parse(text) : undefined) as T };
}
