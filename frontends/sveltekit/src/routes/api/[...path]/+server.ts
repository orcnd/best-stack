// Same-origin pass-through to the C++ API (used by clients that call /api directly).
import { API_BASE, sessionId } from "#lib/server/api";
import type { RequestHandler } from "./$types";

const proxy: RequestHandler = async ({ request, params, url, cookies, fetch }) => {
  const res = await fetch(`${API_BASE()}/api/${params.path}${url.search}`, {
    method: request.method,
    headers: { "x-session-id": sessionId(cookies), "content-type": request.headers.get("content-type") ?? "application/json" },
    body: request.method === "GET" || request.method === "DELETE" ? undefined : await request.text(),
  });
  return new Response(res.status === 204 ? null : await res.text(), { status: res.status, headers: { "content-type": res.headers.get("content-type") ?? "application/json" } });
};
export const GET = proxy;
export const POST = proxy;
export const PATCH = proxy;
export const DELETE = proxy;
