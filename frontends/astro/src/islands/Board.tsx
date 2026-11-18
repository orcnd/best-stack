import { useState } from "preact/hooks";
import { api, COLUMN_LABELS, COLUMNS, type Board as BoardT } from "../lib/client";

export default function Board({ initial }: { initial: BoardT }) {
  const [b, setB] = useState(initial);
  const refresh = async () => setB(await api<BoardT>(`/api/kanban/boards/${initial.id}`));
  const action = `/kanban/${initial.id}`;

  const create = async (e: Event) => {
    e.preventDefault();
    const f = e.currentTarget as HTMLFormElement;
    const fd = new FormData(f);
    await api(`/api/kanban/boards/${initial.id}/cards`, { method: "POST", json: { title: fd.get("title"), column: fd.get("column") } });
    f.reset();
    await refresh();
  };
  const patch = (id: number, body: Record<string, unknown>) => async (e: Event) => {
    e.preventDefault();
    await api(`/api/kanban/cards/${id}`, { method: "PATCH", json: body });
    await refresh();
  };
  const remove = (id: number) => async (e: Event) => {
    e.preventDefault();
    await api(`/api/kanban/cards/${id}`, { method: "DELETE" });
    await refresh();
  };

  return (
    <>
      <form class="new-card" method="post" action={action} onSubmit={create}>
        <input type="text" name="title" maxLength={200} required placeholder="Yeni kart başlığı" data-testid="new-card-input" />
        <input type="hidden" name="column" value="backlog" />
        <button type="submit" data-testid="new-card-submit">Kart ekle</button>
      </form>
      <div class="board" id="board">
        {COLUMNS.map((key, i) => {
          const cards = b.columns[key] ?? [];
          return (
            <section key={key} class="column" data-testid={`column-${key}`}>
              <h2>{COLUMN_LABELS[key]} <span class="count">{cards.length}</span></h2>
              {cards.length > 0 ? (
                <ul class="cards">
                  {cards.map((c) => (
                    <li key={c.id} class="card" data-testid="card" data-card-id={c.id}>
                      <form class="title-form" method="post" action={action}
                        onSubmit={(e) => patch(c.id, { title: new FormData(e.currentTarget).get("title") })(e)}>
                        <input type="hidden" name="id" value={c.id} />
                        <input class="card-title" type="text" name="title" value={c.title} maxLength={200} data-testid="card-title" />
                        <button type="submit" class="small" data-testid="card-edit">Kaydet</button>
                      </form>
                      <p class="card-meta"><span class="labels">{c.labels.join(", ")}</span> <span class="assignee">{c.assignee}</span> <span class="due">{c.due_date}</span></p>
                      <div class="card-actions">
                        <form method="post" action={action} onSubmit={patch(c.id, { move: "left" })}><input type="hidden" name="id" value={c.id} /><input type="hidden" name="move" value="left" /><button type="submit" class="small" data-testid="card-move-left" disabled={i === 0}>←</button></form>
                        <form method="post" action={action} onSubmit={patch(c.id, { move: "right" })}><input type="hidden" name="id" value={c.id} /><input type="hidden" name="move" value="right" /><button type="submit" class="small" data-testid="card-move-right" disabled={i === 3}>→</button></form>
                        <form method="post" action={action} onSubmit={remove(c.id)}><input type="hidden" name="id" value={c.id} /><input type="hidden" name="delete" value="1" /><button type="submit" class="small danger" data-testid="card-delete">Sil</button></form>
                      </div>
                    </li>
                  ))}
                </ul>
              ) : (
                <p class="empty" data-testid="empty-state">Kart yok.</p>
              )}
            </section>
          );
        })}
      </div>
    </>
  );
}
