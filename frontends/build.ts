// Bundles the C++-rendered frontends' client code into shared/static/js/<fe>/.
import { join } from "node:path";

const root = join(import.meta.dir, "..");
const targets: Record<string, string[]> = {
  vanilla: ["catalog", "kanban", "dashboard"],
  alpine: ["catalog", "kanban", "dashboard"],
  htmx: ["htmx-config"],
};

const only = process.argv[2];
for (const [fe, names] of Object.entries(targets)) {
  if (only && only !== fe) continue;
  const res = await Bun.build({
    entrypoints: names.map((n) => join(root, "frontends", fe, "src", `${n}.ts`)),
    outdir: join(root, "shared", "static", "js", fe),
    target: "browser",
    format: "esm",
    minify: true,
    splitting: false,
  });
  if (!res.success) {
    for (const l of res.logs) console.error(l);
    process.exit(1);
  }
  console.log(`${fe}: ${res.outputs.map((o) => o.path.split("/").pop()).join(", ")}`);
}
