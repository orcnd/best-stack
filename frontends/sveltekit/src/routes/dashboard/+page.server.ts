import { error } from "@sveltejs/kit";
import { api, sessionId } from "#lib/server/api";
import type { Dashboard } from "#lib";
import type { PageServerLoad } from "./$types";

export const load: PageServerLoad = async ({ url, cookies, fetch }) => {
  const range = url.searchParams.get("range") || "30";
  if (!["7", "30", "90"].includes(range)) error(400, "Geçersiz aralık");
  const { data } = await api<Dashboard>(fetch, `/api/dashboard?range=${range}`, sessionId(cookies));
  return { range, d: data };
};
