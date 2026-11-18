import { api, COLUMN_LABELS, COLUMNS, type Board, type Card } from "../../shared/types";
import { registerCartStore } from "./common";

document.addEventListener("alpine:init", () => {
  const Alpine = window.Alpine;
  registerCartStore(Alpine);
  Alpine.data("kanban", (boardId: number) => ({
    ready: false,
    columns: [] as { key: string; label: string; cards: Card[] }[],
    async refresh() {
      const b = await api<Board>(`/api/kanban/boards/${boardId}`);
      this.columns = COLUMNS.map((key) => ({ key, label: COLUMN_LABELS[key], cards: b.columns[key] ?? [] }));
      this.ready = true;
    },
    async create(form: HTMLFormElement) {
      const fd = new FormData(form);
      await api(`/api/kanban/boards/${boardId}/cards`, { method: "POST", json: { title: fd.get("title"), column: fd.get("column") } });
      form.reset();
      await this.refresh();
    },
    async update(id: number, patch: { title?: string; move?: string }) {
      await api(`/api/kanban/cards/${id}`, { method: "PATCH", json: patch });
      await this.refresh();
    },
    async remove(id: number) {
      await api(`/api/kanban/cards/${id}`, { method: "DELETE" });
      await this.refresh();
    },
  }));
});
