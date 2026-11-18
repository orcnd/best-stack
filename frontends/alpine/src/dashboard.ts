import { api, type Dashboard } from "../../shared/types";
import { BASE, helpers, registerCartStore } from "./common";

document.addEventListener("alpine:init", () => {
  const Alpine = window.Alpine;
  registerCartStore(Alpine);
  Alpine.data("dashboard", () => ({
    ...helpers,
    ready: false,
    range: "30",
    d: null as Dashboard | null,
    init() {
      this.range = new URLSearchParams(location.search).get("range") ?? "30";
      window.addEventListener("popstate", () => {
        this.range = new URLSearchParams(location.search).get("range") ?? "30";
        this.load(false);
      });
    },
    async load(push = true) {
      this.d = await api<Dashboard>(`/api/dashboard?range=${encodeURIComponent(this.range)}`);
      this.ready = true;
      if (push) history.pushState(null, "", `${BASE}/dashboard?range=${this.range}`);
    },
  }));
});
