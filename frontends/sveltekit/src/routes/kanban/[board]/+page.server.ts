import { error, fail } from "@sveltejs/kit";
import { api, sessionId } from "#lib/server/api";
import type { Board, BoardSummary } from "#lib";
import type { Actions, PageServerLoad } from "./$types";

export const load: PageServerLoad = async ({ params, cookies, fetch }) => {
  if (!/^\d+$/.test(params.board)) error(404, "Bulunamadı");
  const sid = sessionId(cookies);
  const [b, all] = await Promise.all([
    api<Board>(fetch, `/api/kanban/boards/${params.board}`, sid),
    api<BoardSummary[]>(fetch, "/api/kanban/boards", sid),
  ]);
  if (b.status !== 200) error(404, "Bulunamadı");
  return { board: b.data, boards: all.data };
};

export const actions: Actions = {
  create: async ({ request, params, cookies, fetch }) => {
    const fd = await request.formData();
    const r = await api(fetch, `/api/kanban/boards/${params.board}/cards`, sessionId(cookies), {
      method: "POST",
      json: { title: String(fd.get("title") ?? ""), column: String(fd.get("column") ?? "backlog") },
    });
    if (r.status !== 201) return fail(r.status, { error: "invalid" });
  },
  update: async ({ request, cookies, fetch }) => {
    const fd = await request.formData();
    const id = Number(fd.get("id"));
    const sid = sessionId(cookies);
    if (fd.get("delete") === "1") {
      await api(fetch, `/api/kanban/cards/${id}`, sid, { method: "DELETE" });
      return;
    }
    const patch: Record<string, string> = {};
    if (fd.has("title")) patch.title = String(fd.get("title"));
    if (fd.has("move")) patch.move = String(fd.get("move"));
    const r = await api(fetch, `/api/kanban/cards/${id}`, sid, { method: "PATCH", json: patch });
    if (r.status !== 200) return fail(r.status, { error: "invalid" });
  },
};
