import { esc } from "../../shared/format";
import { api, COLUMN_LABELS, COLUMNS, type Board } from "../../shared/types";
import { BASE } from "./common";

const boardEl = document.getElementById("board");
const boardId = Number(location.pathname.split("/").pop());

function renderBoard(b: Board): string {
  return COLUMNS.map((key, i) => {
    const cards = b.columns[key] ?? [];
    const items = cards
      .map(
        (c) => `
      <li class="card" data-testid="card" data-card-id="${c.id}">
        <form class="title-form" method="post" action="${BASE}/kanban/cards/${c.id}">
          <input class="card-title" type="text" name="title" value="${esc(c.title)}" maxlength="200" data-testid="card-title">
          <button type="submit" class="small" data-testid="card-edit">Kaydet</button>
        </form>
        <p class="card-meta"><span class="labels">${esc(c.labels.join(", "))}</span> <span class="assignee">${esc(c.assignee)}</span> <span class="due">${esc(c.due_date)}</span></p>
        <div class="card-actions">
          <form method="post" action="${BASE}/kanban/cards/${c.id}"><input type="hidden" name="move" value="left"><button type="submit" class="small" data-testid="card-move-left"${i === 0 ? " disabled" : ""}>←</button></form>
          <form method="post" action="${BASE}/kanban/cards/${c.id}"><input type="hidden" name="move" value="right"><button type="submit" class="small" data-testid="card-move-right"${i === 3 ? " disabled" : ""}>→</button></form>
          <form method="post" action="${BASE}/kanban/cards/${c.id}"><input type="hidden" name="delete" value="1"><button type="submit" class="small danger" data-testid="card-delete">Sil</button></form>
        </div>
      </li>`,
      )
      .join("");
    return `
  <section class="column" data-testid="column-${key}">
    <h2>${COLUMN_LABELS[key]} <span class="count">${cards.length}</span></h2>
    ${cards.length ? `<ul class="cards">${items}</ul>` : `<p class="empty" data-testid="empty-state">Kart yok.</p>`}
  </section>`;
  }).join("");
}

async function refresh() {
  const b = await api<Board>(`/api/kanban/boards/${boardId}`);
  boardEl!.innerHTML = renderBoard(b);
}

document.querySelector<HTMLFormElement>(".new-card")?.addEventListener("submit", async (e) => {
  e.preventDefault();
  const f = e.target as HTMLFormElement;
  const fd = new FormData(f);
  await api(`/api/kanban/boards/${boardId}/cards`, { method: "POST", json: { title: fd.get("title"), column: fd.get("column") } });
  f.reset();
  await refresh();
});

boardEl?.addEventListener("submit", async (e) => {
  e.preventDefault();
  const f = e.target as HTMLFormElement;
  const id = f.action.split("/").pop();
  const fd = new FormData(f);
  if (fd.get("delete") === "1") await api(`/api/kanban/cards/${id}`, { method: "DELETE" });
  else if (fd.has("move")) await api(`/api/kanban/cards/${id}`, { method: "PATCH", json: { move: fd.get("move") } });
  else await api(`/api/kanban/cards/${id}`, { method: "PATCH", json: { title: fd.get("title") } });
  await refresh();
});
