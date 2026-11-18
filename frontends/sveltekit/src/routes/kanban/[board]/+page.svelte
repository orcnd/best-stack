<script lang="ts">
  import { enhance } from "$app/forms";
  import { COLUMN_LABELS, COLUMNS } from "#lib";
  let { data } = $props();
  const b = $derived(data.board);
</script>

<svelte:head>
  <title>{b.name} · Kanban · Best Stack</title>
  <meta name="description" content="Kanban panosu">
</svelte:head>

<div class="board-head">
  <h1>{b.name}</h1>
  <nav class="board-tabs" data-testid="board-select">
    {#each data.boards as t (t.id)}
      <a href="/kanban/{t.id}" class={t.id === b.id ? "active" : undefined}>{t.name}</a>
    {/each}
  </nav>
</div>
<form class="new-card" method="post" action="?/create" use:enhance>
  <input type="text" name="title" maxlength="200" required placeholder="Yeni kart başlığı" data-testid="new-card-input">
  <input type="hidden" name="column" value="backlog">
  <button type="submit" data-testid="new-card-submit">Kart ekle</button>
</form>
<div class="board" id="board">
  {#each COLUMNS as key, i (key)}
    {@const cards = b.columns[key] ?? []}
    <section class="column" data-testid="column-{key}">
      <h2>{COLUMN_LABELS[key]} <span class="count">{cards.length}</span></h2>
      {#if cards.length > 0}
        <ul class="cards">
          {#each cards as c (c.id)}
            <li class="card" data-testid="card" data-card-id={c.id}>
              <form class="title-form" method="post" action="?/update" use:enhance={() => async ({ update }) => update({ reset: false })}>
                <input type="hidden" name="id" value={c.id}>
                <input class="card-title" type="text" name="title" value={c.title} maxlength="200" data-testid="card-title">
                <button type="submit" class="small" data-testid="card-edit">Kaydet</button>
              </form>
              <p class="card-meta"><span class="labels">{c.labels.join(", ")}</span> <span class="assignee">{c.assignee}</span> <span class="due">{c.due_date}</span></p>
              <div class="card-actions">
                <form method="post" action="?/update" use:enhance><input type="hidden" name="id" value={c.id}><input type="hidden" name="move" value="left"><button type="submit" class="small" data-testid="card-move-left" disabled={i === 0}>←</button></form>
                <form method="post" action="?/update" use:enhance><input type="hidden" name="id" value={c.id}><input type="hidden" name="move" value="right"><button type="submit" class="small" data-testid="card-move-right" disabled={i === 3}>→</button></form>
                <form method="post" action="?/update" use:enhance><input type="hidden" name="id" value={c.id}><input type="hidden" name="delete" value="1"><button type="submit" class="small danger" data-testid="card-delete">Sil</button></form>
              </div>
            </li>
          {/each}
        </ul>
      {:else}
        <p class="empty" data-testid="empty-state">Kart yok.</p>
      {/if}
    </section>
  {/each}
</div>
