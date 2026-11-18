// Same-origin pass-through to the C++ API for islands.
import type { APIRoute } from "astro";
import { API_BASE, sessionId } from "../../lib/server";

export const ALL: APIRoute = async ({ request, params, url, cookies }) => {
  const res = await fetch(`${API_BASE()}/api/${params.path}${url.search}`, {
    method: request.method,
    headers: { "x-session-id": sessionId(cookies), "content-type": request.headers.get("content-type") ?? "application/json" },
    body: request.method === "GET" || request.method === "DELETE" ? undefined : await request.text(),
  });
  return new Response(res.status === 204 ? null : await res.text(), {
    status: res.status,
    headers: { "content-type": res.headers.get("content-type") ?? "application/json" },
  });
};
