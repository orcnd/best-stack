# Markup spesifikasyonu

Tüm frontend'ler (htmx, Alpine, Vanilla — C++ inja; SvelteKit, Astro — Bun SSR) **görünen** yapıyı burada tanımlandığı gibi üretir. Framework'e özgü öznitelikler (`hx-*`, `x-*`, hidrasyon işaretleri, `<astro-island>`) serbesttir; görünen eleman, sınıf, metin ve `data-testid` aynı olmalıdır. Testler yalnız `data-testid`'leri kullanır.

## Genel kurallar

- `<html lang="tr">`, `<meta charset="utf-8">`, `<meta name="viewport" content="width=device-width, initial-scale=1">`.
- `<title>{Sayfa başlığı} · Best Stack</title>` ve `<meta name="description" content="…">` her sayfada.
- Tek stil dosyası: `/static/app.css` (Bun frontend'leri kendi kökünden aynı dosyayı sunar). Web font yok, animasyon/transition yok.
- `{base}` = frontend'in kökü: C++ render için `/{fe}`, Bun frontend'leri için `` (boş).
- Para: `₺1.234,50` (binlik `.`, ondalık `,`, her zaman 2 hane). Sayı: `12.345`. Oran: `%12,3`. Puan: `★ 4,5`. Tarih: `YYYY-MM-DD`.
- Linkler gerçek `<a href>`, formlar gerçek `GET`/`POST` form; JS kapalıyken her şey çalışır.
- Kullanıcı girdisi her yerde HTML-escape edilir.

## Ortak iskelet

```html
<body>
  <header class="site-header" data-testid="site-header">
    <a class="brand" href="{base}/catalog">Best Stack</a>
    <nav class="main-nav">
      <a data-testid="nav-catalog" href="{base}/catalog">Katalog</a>
      <a data-testid="nav-kanban" href="{base}/kanban/1">Kanban</a>
      <a data-testid="nav-dashboard" href="{base}/dashboard">Dashboard</a>
    </nav>
    <a class="cart-link" href="{base}/catalog/cart">Sepet <span class="badge" data-testid="cart-badge">{cart.count}</span></a>
  </header>
  <main class="container">…sayfa…</main>
</body>
```
Aktif bölümün nav linki `class="active"` alır.

## Katalog — liste `{base}/catalog?q&cat&sort&page`

Başlık: `Ürün Kataloğu`, description: `Best Stack demo ürün kataloğu`.
```html
<h1>Ürün Kataloğu</h1>
<form class="toolbar" data-testid="search-form" method="get" action="{base}/catalog">
  <input type="search" name="q" value="{q}" placeholder="Ürün ara…" data-testid="search-input">
  <select name="cat" data-testid="category-filter">
    <option value="">Tüm kategoriler</option>
    <option value="{slug}" [selected]>{name}</option>…
  </select>
  <select name="sort" data-testid="sort-select">
    <option value="">Varsayılan sıralama</option>
    <option value="price_asc">Fiyat (artan)</option>
    <option value="price_desc">Fiyat (azalan)</option>
  </select>
  <button type="submit">Ara</button>
</form>
<section id="product-results">
  <p class="result-count">{total} ürün</p>
  <div class="grid" data-testid="product-grid">
    <article class="product-card" data-testid="product-card">
      <a class="thumb" href="{base}/catalog/p/{id}"><img src="{image}" alt="{name}" width="320" height="240"></a>
      <h2 class="product-name" data-testid="product-name"><a href="{base}/catalog/p/{id}">{name}</a></h2>
      <p class="product-category">{category}</p>
      <p class="product-price" data-testid="product-price">{price}</p>
      <form class="add-form" method="post" action="{base}/catalog/cart">
        <input type="hidden" name="product_id" value="{id}">
        <input type="hidden" name="qty" value="1">
        <button type="submit" data-testid="add-to-cart">Sepete ekle</button>
      </form>
    </article>…
  </div>
  <!-- sonuç yoksa grid yerine: -->
  <p class="empty" data-testid="empty-state">Sonuç bulunamadı.</p>
  <nav class="pagination" data-testid="pagination">
    <a data-testid="page-prev" href="{base}/catalog?{query, page-1}">‹ Önceki</a>  <!-- page=1 ise: <span class="disabled">‹ Önceki</span> -->
    <span class="page-info">Sayfa {page} / {pages}</span>
    <a data-testid="page-next" href="{base}/catalog?{query, page+1}">Sonraki ›</a> <!-- son sayfada: <span class="disabled">Sonraki ›</span> -->
  </nav>
</section>
```
Query string sırası: `q`, `cat`, `sort`, `page`; boş değerler yazılmaz.

## Katalog — detay `{base}/catalog/p/{id}`

Başlık: `{name}`, description: `{description}`.
```html
<article class="product-detail" data-testid="product-detail">
  <img src="{image}" alt="{name}" width="320" height="240">
  <div class="info">
    <h1>{name}</h1>
    <p class="product-category">{category}</p>
    <p class="product-price" data-testid="product-price">{price}</p>
    <p class="rating">{★ rating}</p>
    <p class="stock">Stok: {stock}</p>
    <p class="description">{description}</p>
    <form class="add-form" method="post" action="{base}/catalog/cart">
      <input type="hidden" name="product_id" value="{id}">
      <input type="hidden" name="qty" value="1">
      <button type="submit" data-testid="add-to-cart">Sepete ekle</button>
    </form>
  </div>
</article>
<a class="back" href="{base}/catalog">‹ Kataloğa dön</a>
```

## Katalog — sepet `{base}/catalog/cart`

Başlık: `Sepet`, description: `Alışveriş sepeti`.
```html
<h1>Sepet</h1>
<section id="cart">
  <table class="cart-table" data-testid="cart-table">
    <thead><tr><th>Ürün</th><th>Fiyat</th><th>Adet</th><th>Tutar</th><th></th></tr></thead>
    <tbody>
      <tr data-testid="cart-row">
        <td><a href="{base}/catalog/p/{product.id}">{product.name}</a></td>
        <td>{price}</td>
        <td><form class="qty-form" method="post" action="{base}/catalog/cart/{item.id}">
              <input type="number" name="qty" min="0" max="99" value="{qty}" data-testid="cart-qty">
              <button type="submit" data-testid="cart-update">Güncelle</button></form></td>
        <td>{line total}</td>
        <td><form method="post" action="{base}/catalog/cart/{item.id}">
              <input type="hidden" name="qty" value="0">
              <button type="submit" data-testid="cart-remove">Kaldır</button></form></td>
      </tr>…
    </tbody>
  </table>
  <p class="cart-total">Toplam: <strong data-testid="cart-total">{total}</strong></p>
  <!-- boşsa tablo ve toplam yerine: -->
  <p class="empty" data-testid="empty-state">Sepetiniz boş.</p>
</section>
```

## Kanban `{base}/kanban/{board_id}`

Başlık: `{board.name} · Kanban`, description: `Kanban panosu`.
```html
<div class="board-head">
  <h1>{board.name}</h1>
  <nav class="board-tabs" data-testid="board-select">
    <a href="{base}/kanban/{id}" [class="active"]>{name}</a>…
  </nav>
</div>
<form class="new-card" method="post" action="{base}/kanban/{board_id}/cards">
  <input type="text" name="title" maxlength="200" required placeholder="Yeni kart başlığı" data-testid="new-card-input">
  <input type="hidden" name="column" value="backlog">
  <button type="submit" data-testid="new-card-submit">Kart ekle</button>
</form>
<div class="board" id="board">
  <section class="column" data-testid="column-{backlog|todo|doing|done}">
    <h2>{Backlog|Yapılacak|Yapılıyor|Bitti} <span class="count">{n}</span></h2>
    <ul class="cards">
      <li class="card" data-testid="card" data-card-id="{id}">
        <form class="title-form" method="post" action="{base}/kanban/cards/{id}">
          <input class="card-title" type="text" name="title" value="{title}" maxlength="200" data-testid="card-title">
          <button type="submit" class="small" data-testid="card-edit">Kaydet</button>
        </form>
        <p class="card-meta"><span class="labels">{labels virgülle}</span> <span class="assignee">{assignee}</span> <span class="due">{due_date}</span></p>
        <div class="card-actions">
          <form method="post" action="{base}/kanban/cards/{id}"><input type="hidden" name="move" value="left"><button type="submit" class="small" data-testid="card-move-left" [disabled]>←</button></form>
          <form method="post" action="{base}/kanban/cards/{id}"><input type="hidden" name="move" value="right"><button type="submit" class="small" data-testid="card-move-right" [disabled]>→</button></form>
          <form method="post" action="{base}/kanban/cards/{id}"><input type="hidden" name="delete" value="1"><button type="submit" class="small danger" data-testid="card-delete">Sil</button></form>
        </div>
      </li>…
    </ul>
    <!-- sütun boşsa ul yerine: -->
    <p class="empty" data-testid="empty-state">Kart yok.</p>
  </section>×4
</div>
```
`←` backlog'da, `→` done'da `disabled`. Boş meta alanları boş `span` olarak kalır.

## Dashboard `{base}/dashboard?range=7|30|90`

Başlık: `Analitik`, description: `Analitik dashboard`.
```html
<div class="dash-head">
  <h1>Analitik</h1>
  <form class="range-form" method="get" action="{base}/dashboard">
    <select name="range" data-testid="range-select">
      <option value="7">Son 7 gün</option><option value="30" selected>Son 30 gün</option><option value="90">Son 90 gün</option>
    </select>
    <button type="submit">Uygula</button>
  </form>
</div>
<section id="dashboard-body">
  <div class="kpis">
    <div class="kpi"><span class="kpi-label">Olay</span><strong data-testid="kpi-events">{events}</strong></div>
    <div class="kpi"><span class="kpi-label">Tekil kullanıcı</span><strong data-testid="kpi-users">{users}</strong></div>
    <div class="kpi"><span class="kpi-label">Gelir</span><strong data-testid="kpi-revenue">{revenue ₺}</strong></div>
    <div class="kpi"><span class="kpi-label">Dönüşüm</span><strong data-testid="kpi-conversion">{%conversion}</strong></div>
  </div>
  <div class="panel"><h2>Günlük olaylar</h2><div class="chart" data-testid="chart-daily">{daily_chart_svg}</div></div>
  <div class="two-col">
    <div class="panel"><h2>Ülkeler</h2>
      <table data-testid="country-table"><thead><tr><th>Ülke</th><th>Olay</th></tr></thead>
        <tbody><tr><td>{country}</td><td>{events}</td></tr>…</tbody></table></div>
    <div class="panel"><h2>En çok gelir getirenler</h2>
      <table data-testid="top-items-table"><thead><tr><th>Ürün</th><th>Satış</th><th>Gelir</th></tr></thead>
        <tbody><tr><td>{item}</td><td>{purchases}</td><td>{revenue ₺}</td></tr>…</tbody></table></div>
  </div>
</section>
```

## Grafik (`daily_chart_svg`)

`<svg class="chart-svg" viewBox="0 0 960 240" width="960" height="240" role="img" aria-label="Günlük olaylar">`, `max = max(events)` (0 ise 1), `n = gün sayısı`, `bar_w = 960 / n`; her gün `i` için `h = round(events_i / max * 220)`; `<rect x="{fmt(i*bar_w + 1)}" y="{240 - h}" width="{fmt(bar_w - 2)}" height="{h}" fill="#4f6bed"/>`. `fmt` = en fazla 2 ondalık, sondaki sıfırlar ve nokta atılır (`32`, `10.67`).
