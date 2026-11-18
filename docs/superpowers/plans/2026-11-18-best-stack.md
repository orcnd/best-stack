# Best Stack Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 3 demo senaryoyu 5 C++ backend × 5 frontend (75 kombinasyon) ile birebir aynı görünüm/işlevde kurmak, kullanıcı perspektifinden ölçmek, ham veriyi `results.sqlite`'a yazmak ve bundan puan tablosu üretmek.

**Architecture:** Ortak C++ çekirdek (`bs_core`: SQLite veri katmanı, inja şablon render'ı, framework-bağımsız handler fonksiyonları) + her backend'de yalnızca HTTP/yönlendirme/eşzamanlılık katmanı. htmx/Alpine/Vanilla sayfalarını C++ render eder; SvelteKit/Astro Bun üzerinde SSR yapıp C++ `/api`'den veri alır. Node tabanlı harness Playwright + oha ile ölçer, `node:sqlite` ile ham veriyi yazar; ayrı puanlama betiği rapor üretir.

**Tech Stack:** C++20 / Apple clang 21 / CMake + Ninja, SQLite3 (WAL), nlohmann_json, inja, doctest; Drogon, Crow (+asio), Oat++, uWebSockets (+uSockets), cpp-httplib; Bun (seed, istemci bundle, SvelteKit `svelte-adapter-bun`, Astro + Preact); Node 26 (harness, `node:sqlite`, Playwright Chromium, pixelmatch); oha.

**Spec:** `docs/superpowers/specs/2026-11-18-best-stack-design.md`

## Global Constraints

- Makine: Apple M2, 8 çekirdek, 8 GB RAM, macOS; kombinasyonlar **sırayla** çalışır, aynı anda tek backend (+ en fazla bir Bun süreci).
- C++20, Release: `-O3 -DNDEBUG`; ölçümler asla Debug build ile yapılmaz.
- DB yalnızca SQLite, WAL modu, `busy_timeout=5000`, bağlantı thread başına.
- HTML'i htmx/Alpine/Vanilla için C++ üretir; SvelteKit ve Astro sunucuları **Bun** ile çalışır (Node değil). Harness Node ile çalışır (sunucu değil).
- Mock veri sabit seed `42` ile; "bugün" sabit `ANCHOR = 2026-11-01T00:00:00Z` (dashboard aralıkları buna göre).
- Sayılar: 12 kategori, 2.000 ürün, sayfa başı 24 ürün; 5 pano × 100 kart, 4 sütun `backlog,todo,doing,done`; 100.000 olay, 180 gün.
- Portlar: C++ backend daima `8080`, Bun SSR daima `3000`. Her sunucu `GET /healthz` → `200 ok`.
- Backend CLI: `bs-<name> --port 8080 --db <path> --root <best-stack dizini> --threads 8`.
- Tek CSS: `shared/static/app.css`; tüm frontend'ler bunu aynen kullanır.
- Her dış bağımlılık kesin sürüm/tag ile sabitlenir ve `shared/versions.json`'a yazılır.
- Kart başlığı 1–200 karakter, aksi halde `422`. Geçersiz `range` → `400`. Bulunamayan id → `404`.
- Görsel parite eşiği: referansa (`httplib + vanilla`) göre piksel farkı oranı `< 0.005`.
- Puan ağırlıkları: Kullanıcı hızı 40, Sunucu 25, DX 20, Kararlılık 15.
- Git: `best-stack/` içinde yerel repo; her task sonunda yerel commit (push yok).

## Review Focus

1. Arama girdisinde özel karakterler (`%`, `_`, `'`, `"`, `<script>`, `ğüşİ`) → SQL hatası yok, LIKE joker olarak yorumlanmaz, çıktı HTML-escape'li. Test: Task 3 `products_search_escapes_wildcards`, Task 5 `catalog_page_escapes_query`.
2. Geçersiz/aralık dışı parametreler (`page=0`, `page=9999`, `id=abc`, `range=13`) → sayfa clamp edilir / 404 / 400; asla 500 veya çökme. Test: Task 5 `invalid_params_*`, Task 7 contract `invalid params`.
3. Eşzamanlı yazma (Playwright akışı + yük testi aynı anda) → `SQLITE_BUSY` kaynaklı 500 yok. Test: Task 3 `concurrent_cart_writes`.
4. Boş durumlar: boş sepet, sonuçsuz arama, kartsız sütun → `data-testid="empty-state"` ile düzgün render. Test: Task 5 `empty_states_render`.
5. Kart başlığında HTML ve uzun metin → escape edilir, 200+ karakter `422`. Test: Task 5 `card_title_validation_and_escape`, Task 7 contract.

---

## File Structure

```
best-stack/
  CMakeLists.txt                     # bs_core + tests + backends (option BS_BACKENDS)
  shared/
    versions.json                    # sabitlenmiş tüm bağımlılık sürümleri
    schema.sql
    seed/seed.ts, seed/rng.ts, seed/seed.test.ts
    markup-spec.md
    static/app.css, static/img/cat-{1..12}.svg, static/vendor/{htmx,alpine}.min.js, static/js/ (bun build çıktısı)
    cpp/
      include/bs/{model.hpp,db.hpp,json.hpp,http.hpp,render.hpp,chart.hpp,handlers.hpp,config.hpp}
      src/{db.cpp,json.cpp,render.cpp,chart.cpp,handlers_catalog.cpp,handlers_kanban.cpp,handlers_dashboard.cpp,http.cpp}
      templates/_base/ templates/{htmx,alpine,vanilla}/{catalog,kanban,dashboard}/
      tests/{test_db.cpp,test_handlers.cpp,test_chart.cpp}
  frontends/
    vanilla/src/{catalog,kanban,dashboard}.ts   alpine/src/*.ts   htmx/src/htmx-config.ts
    sveltekit/   astro/                         # Bun SSR uygulamaları
  backends/{httplib,drogon,crow,oatpp,uwebsockets}/{main.cpp,CMakeLists.txt,deps.txt}
  bench/
    contract/contract.test.ts                    # bun test, çalışan :8080'e karşı
    flows/{catalog,kanban,dashboard}.ts, flows/targets.ts
    parity/{functional.ts,visual.ts,seo.ts}, parity/reference/
    harness/{run.ts,procs.ts,sampler.ts,pageload.ts,interactions.ts,loadtest.ts,dx.ts,resultsdb.ts,schema.sql}
  scoring/{score.ts,weights.json}
  data/ (pristine.sqlite, app.sqlite)   results/ (results.sqlite, report.md)
```

## Shared interfaces (Task 3 ve 5 üretir, herkes kullanır)

`shared/cpp/include/bs/model.hpp` (alan adları JSON'da aynen, snake_case):
```cpp
struct Category { int id; std::string name, slug; };
struct Product { int id, category_id; std::string category, name, description, image; int price_cents, stock; double rating; };
struct ProductQuery { std::string q, cat /*slug*/, sort /*"", "price_asc", "price_desc"*/; int page = 1; int per_page = 24; };
struct ProductPage { std::vector<Product> items; int total, page, pages; };
struct CartItem { int id; Product product; int qty; };
struct Cart { std::vector<CartItem> items; int count /*toplam adet*/; int total_cents; };
struct Card { int id, board_id, position; std::string column, title, description, assignee, due_date; std::vector<std::string> labels; };
struct BoardSummary { int id; std::string name; int card_count; };
struct Board { int id; std::string name; std::map<std::string, std::vector<Card>> columns; /* 4 anahtar her zaman var */ };
struct DayPoint { std::string day; long events; long revenue_cents; };
struct Dashboard { int range_days; long events, users, revenue_cents; double conversion;
  std::vector<DayPoint> daily; std::vector<std::pair<std::string,long>> countries /*top 8*/;
  struct Item { std::string item; long revenue_cents; long purchases; }; std::vector<Item> top_items /*top 10*/; };
```
JSON (`json.hpp`, nlohmann ADL `to_json`): yukarıdaki alanlar aynen; `Dashboard` → `{"range_days","kpis":{"events","users","revenue_cents","conversion"},"daily":[...],"countries":[{"country","events"}],"top_items":[...]}`; `Board` → `{"id","name","columns":{"backlog":[],"todo":[],"doing":[],"done":[]}}`.

`shared/cpp/include/bs/http.hpp`:
```cpp
struct Req { std::string method, path; std::map<std::string,std::string> params, query, form, headers /*küçük harf anahtar*/, cookies; std::string body; };
struct Resp { int status = 200; std::string content_type = "text/html; charset=utf-8"; std::string body; std::vector<std::pair<std::string,std::string>> headers; };
using Handler = Resp(*)(const Req&);
struct Route { const char* method; const char* pattern; /* "/{fe}/catalog/p/{id}" */ Handler fn; };
const std::vector<Route>& routes();          // tek kaynak; backend'ler bunu kendi router'ına native olarak kaydeder
std::optional<Resp> serve_static(const std::string& path); // /static/*, uzun cache + doğru MIME (wasm dahil)
void init(const Config&);                     // config.hpp: root, db_path, threads
```
Oturum: cookie `bs_sid` veya header `x-session-id`; yoksa handler 16 hex üretir ve `Set-Cookie: bs_sid=...; Path=/; SameSite=Lax` ekler.

**Route tablosu (her backend aynen; `{fe}` ∈ htmx|alpine|vanilla, başka değer → 404):**

| Method | Pattern | Davranış |
|---|---|---|
| GET | `/healthz` | `ok` |
| GET | `/static/*` | statik dosya |
| GET | `/{fe}/catalog` | liste (`q,cat,sort,page`); `hx-request` başlığı varsa sadece `#product-results` parçası |
| GET | `/{fe}/catalog/p/{id}` | detay |
| GET | `/{fe}/catalog/cart` | sepet |
| POST | `/{fe}/catalog/cart` | form `product_id,qty` → 303 `Referer`'a; htmx ise sepet rozeti parçası |
| POST | `/{fe}/catalog/cart/{item_id}` | form `qty` (0 = sil) → 303 sepet; htmx ise sepet tablosu parçası |
| GET | `/{fe}/kanban/{board_id}` | pano |
| POST | `/{fe}/kanban/{board_id}/cards` | form `title,column` → 303; htmx ise sütun parçası |
| POST | `/{fe}/kanban/cards/{id}` | form `title?` / `move=left|right` / `delete=1` → 303; htmx ise pano parçası |
| GET | `/{fe}/dashboard` | `range=7|30|90` (varsayılan 30); htmx ise `#dashboard-body` parçası |
| GET | `/api/catalog/products` | ProductPage + `categories` |
| GET | `/api/catalog/products/{id}` | Product / 404 `{"error":"not_found"}` |
| GET/POST | `/api/catalog/cart` | Cart; POST `{"product_id","qty"}` → 201 Cart |
| PATCH/DELETE | `/api/catalog/cart/{item_id}` | PATCH `{"qty"}` → Cart; DELETE → Cart |
| GET | `/api/kanban/boards` | `[BoardSummary]` |
| GET | `/api/kanban/boards/{id}` | Board |
| POST | `/api/kanban/boards/{id}/cards` | `{"title","column"}` → 201 Card / 422 |
| PATCH/DELETE | `/api/kanban/cards/{id}` | PATCH `{"title"?, "move"?}` → Card; DELETE → 204 |
| GET | `/api/dashboard` | `range` → Dashboard / 400 |

---

### Task 1: Repo iskeleti ve araç zinciri

**Files:**
- Create: `best-stack/CMakeLists.txt`, `best-stack/shared/versions.json`, `best-stack/shared/cpp/tests/test_smoke.cpp`, `best-stack/.gitignore`

**Interfaces:**
- Produces: CMake hedefleri `bs_core` (STATIC), `bs_tests` (doctest), option `BS_BACKENDS="httplib;drogon;crow;oatpp;uwebsockets"`; build dizini `build/` (Release, Ninja).

- [ ] **Step 1:** `brew install cmake oha asio drogon tokei` ; `cmake --version` ve `oha --version` çıktı verir.
- [ ] **Step 2:** `git init` (`best-stack/`), `.gitignore`: `build/ data/*.sqlite* results/*.sqlite* node_modules/ .svelte-kit/ dist/`.
- [ ] **Step 3:** FetchContent ile nlohmann_json, inja, doctest (kesin tag'ler) + brew sqlite; sürümleri `versions.json`'a yaz.
- [ ] **Step 4:** `test_smoke.cpp`: `CHECK(sqlite3_libversion_number() >= 3045000)` ve inja ile `"{{ x }}"` → `"1"`.
- [ ] **Step 5:** Run: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build && ctest --test-dir build` → `100% tests passed`.
- [ ] **Step 6:** Commit `chore: scaffold best-stack build`.

### Task 2: Şema ve deterministik mock veri

**Files:**
- Create: `shared/schema.sql`, `shared/seed/rng.ts`, `shared/seed/seed.ts`, `shared/seed/seed.test.ts`

**Interfaces:**
- Produces: `bun shared/seed/seed.ts --out data/pristine.sqlite` ; tablolar:
  `categories(id,name,slug)`, `products(id,category_id,name,description,price_cents,stock,rating,image)`, `cart_items(id,session_id,product_id,qty, UNIQUE(session_id,product_id))`, `boards(id,name)`, `cards(id,board_id,col CHECK(col IN ('backlog','todo','doing','done')),position,title,description,labels,assignee,due_date)`, `events(id,ts,user_id,type CHECK(type IN ('view','signup','purchase')),country,item,revenue_cents)`; indeksler: `products(category_id)`, `products(price_cents)`, `cards(board_id,col,position)`, `events(ts)`.

- [ ] **Step 1: Failing test** `seed.test.ts`:
  - `counts`: categories=12, products=2000, boards=5, cards=500 (her pano 100, her sütun ≥1), events=100000.
  - `deterministic`: iki ayrı üretimin tüm tablo dökümlerinin SHA-256'sı eşit.
  - `search_fixture`: `name LIKE '%Lamp%'` sayısı 20–200 arası ("lamp" akış testi bunu kullanır).
  - `events_window`: `min(ts) >= ANCHOR-180g`, `max(ts) < ANCHOR`; her olay ülkesi 12 sabit ülke listesinden; `purchase` olaylarında `revenue_cents > 0`, diğerlerinde 0.
  - `turkish_fixture`: en az bir ürün adı `ğ`/`ş`/`İ` içerir (Review Focus 1).
- [ ] **Step 2:** `bun test shared/seed` → FAIL.
- [ ] **Step 3:** `rng.ts`: mulberry32(seed=42); `seed.ts`: `bun:sqlite`, isimler sıfat×isim listelerinden (isim listesinde "Lamp" var), ürün görseli `/static/img/cat-{category_id}.svg`.
- [ ] **Step 4:** `bun test shared/seed` → PASS; `data/pristine.sqlite` oluşur.
- [ ] **Step 5:** Commit `feat: schema and deterministic seed`.

### Task 3: C++ veri katmanı + JSON

**Files:**
- Create: `shared/cpp/include/bs/{model.hpp,db.hpp,json.hpp,config.hpp}`, `shared/cpp/src/{db.cpp,json.cpp}`, `shared/cpp/tests/test_db.cpp`

**Interfaces:**
- Consumes: Task 2 şeması.
- Produces (`db.hpp`, `namespace bs`):
```cpp
class Db {
 public:
  explicit Db(const std::string& path);  // WAL, busy_timeout=5000, prepared stmt cache
  std::vector<Category> categories();
  ProductPage products(const ProductQuery&);     // page clamp [1,pages]; pages>=1
  std::optional<Product> product(int id);
  Cart cart(const std::string& sid);
  void cart_add(const std::string& sid, int product_id, int qty); // varsa qty toplanır
  bool cart_set_qty(const std::string& sid, int item_id, int qty); // 0 → sil; başka sid'in kalemi → false
  std::vector<BoardSummary> boards();
  std::optional<Board> board(int id);
  std::optional<Card> card_create(int board_id, const std::string& title, const std::string& column); // sütun sonuna
  std::optional<Card> card_update(int id, std::optional<std::string> title, std::optional<std::string> move /*left|right*/);
  bool card_delete(int id);
  Dashboard dashboard(int range_days);           // 7|30|90, ANCHOR'a göre [ANCHOR-range, ANCHOR)
};
Db& thread_db();   // thread_local, init(Config) sonrası lazy açılır
```
Arama: `name LIKE ? ESCAPE '\'` (q içindeki `% _ \` kaçışlanır), büyük/küçük harf duyarsız; dönüşüm = purchase yapan tekil kullanıcı / tekil kullanıcı.

- [ ] **Step 1: Failing tests** (`test_db.cpp`, her test pristine'in geçici kopyasıyla):
  - `products_default_page`: items=24, total=2000, pages=84, page=1.
  - `products_filter_and_sort`: `cat` slug ile tüm item'ların category_id'si aynı; `price_asc` monoton artan.
  - `products_search_escapes_wildcards`: `q="%"` ve `q="_"` → total 0 (joker değil); `q="lamp"` → total seed testindekiyle aynı, `q="' OR 1=1 --"` → 0, hata yok.
  - `page_clamp`: page=0 → 1; page=9999 → 84.
  - `cart_roundtrip`: add(p,1) iki kez → qty 2, count 2; set_qty 0 → items boş; başka sid ile set_qty → false.
  - `kanban_crud`: create("x","backlog") → backlog sonunda; move right → todo; done'da move right → değişmez; delete → board'da yok.
  - `dashboard_ranges`: range 7/30/90 için `daily.size()==range`, `events` = SQL ile doğrudan sayım, `countries.size()<=8`, `top_items.size()==10`, 0≤conversion≤1.
  - `concurrent_cart_writes`: 8 thread × 200 `cart_add` (ayrı Db'ler, aynı dosya) → istisna yok, son toplam 1600.
  - `json_shapes`: `to_json(Dashboard)` anahtarları `range_days,kpis,daily,countries,top_items`; Board `columns` 4 anahtarlı.
- [ ] **Step 2:** `ctest --test-dir build -R db` → FAIL.
- [ ] **Step 3:** `db.cpp`, `json.cpp` uygula.
- [ ] **Step 4:** `ctest --test-dir build` → PASS.
- [ ] **Step 5:** Commit `feat: shared sqlite data layer`.

### Task 4: Markup spesifikasyonu, CSS, statik varlıklar

**Files:**
- Create: `shared/markup-spec.md`, `shared/static/app.css`, `shared/static/img/cat-{1..12}.svg`, `shared/static/vendor/htmx.min.js`, `shared/static/vendor/alpine.min.js`

**Interfaces:**
- Produces: `markup-spec.md` her sayfanın görünen DOM iskeleti ve şu `data-testid`'ler (tüm frontend'ler bunlara uyar, testler bunları kullanır):
  - Ortak: `site-header`, `nav-catalog`, `nav-kanban`, `nav-dashboard`, `cart-badge`, `empty-state`.
  - Katalog: `search-form`, `search-input`, `category-filter`, `sort-select`, `product-grid`, `product-card`, `product-name`, `product-price`, `pagination`, `page-next`, `page-prev`, `product-detail`, `add-to-cart`, `cart-table`, `cart-row`, `cart-qty`, `cart-remove`, `cart-total`.
  - Kanban: `board-select`, `column-backlog|todo|doing|done`, `card`, `card-title`, `card-edit`, `card-move-left`, `card-move-right`, `card-delete`, `new-card-input`, `new-card-submit`.
  - Dashboard: `range-select`, `kpi-events`, `kpi-users`, `kpi-revenue`, `kpi-conversion`, `chart-daily`, `country-table`, `top-items-table`.
  - Para biçimi `₺1.234,50` (tr-TR), tarih `YYYY-MM-DD`; `<title>` ve `<meta name="description">` her sayfada; sabit genişlik düzeni 1280px tasarım; sistem font yığını (web font yok → görsel parite stabil); animasyon/transition yok.
- [ ] **Step 1:** Spec'i yaz; htmx 2.x ve Alpine 3.x min dosyalarını kesin sürümle vendor'la, `versions.json`'a ekle.
- [ ] **Step 2:** `app.css` ve 12 SVG (aynı 320×240 boyut).
- [ ] **Step 3:** Verify: `grep -c 'data-testid' shared/markup-spec.md` ≥ 40; dosyalar mevcut.
- [ ] **Step 4:** Commit `feat: markup spec, shared css and assets`.

### Task 5: Şablonlar, render, SVG grafik, handler'lar

**Files:**
- Create: `shared/cpp/include/bs/{http.hpp,render.hpp,chart.hpp,handlers.hpp}`, `shared/cpp/src/{http.cpp,render.cpp,chart.cpp,handlers_catalog.cpp,handlers_kanban.cpp,handlers_dashboard.cpp}`, `shared/cpp/templates/**`, `shared/cpp/tests/{test_handlers.cpp,test_chart.cpp}`

**Interfaces:**
- Consumes: Task 3 `Db`, JSON; Task 4 testid'ler.
- Produces: `routes()`, `serve_static()`, `init()` (http.hpp, yukarıda); `std::string render(std::string_view fe, std::string_view tmpl, const nlohmann::json& data)` (render.hpp, inja ortamı başlangıçta derlenir, thread-safe okuma); `std::string daily_chart_svg(const std::vector<DayPoint>&, int width=960, int height=240)` (chart.hpp) — çubuk grafik, y ekseni max'a göre, deterministik çıktı (Task 13/14 bunu TS'e birebir portlar; algoritma: `bar_w = width / n`, `h_i = round(events_i / max * (height-20))`, `<rect x=i*bar_w+1 y=height-h_i width=bar_w-2 height=h_i>`).
- Şablon düzeni: `_base/` ortak görünen markup parçaları (inja `include`); `{fe}/` her fe'nin sarmalayıcıları (htmx `hx-*`, Alpine `x-*`, vanilla `<script type=module src=/static/js/vanilla/...>`). Her fe sayfası aynı `_base` parçalarını kullanır → görünen markup tek yerde.

- [ ] **Step 1: Failing tests** (`test_handlers.cpp`, handler'ları doğrudan `Req` ile çağırır):
  - `catalog_page_all_fes`: 3 fe için `/catalog` → 200, 24 adet `data-testid="product-card"`, `<title>` dolu.
  - `htmx_fragment`: `hx-request: true` ile `/htmx/catalog?q=lamp` → `<html` içermez, `product-grid` içerir.
  - `catalog_page_escapes_query`: `q=<script>x</script>` → gövdede `&lt;script&gt;`, ham `<script>x` yok.
  - `invalid_params_*`: `/vanilla/catalog/p/abc` → 404; `/vanilla/catalog/p/999999` → 404; `/xyz/catalog` → 404; `/api/dashboard?range=13` → 400; `/vanilla/catalog?page=9999` → 200, sayfa 84.
  - `cart_post_redirects`: form POST (htmx değil) → 303 + `Location`; yeni oturumda `Set-Cookie: bs_sid=`.
  - `empty_states_render`: boş sepet, `q=zzzz`, boş sütunlu pano → `data-testid="empty-state"`.
  - `card_title_validation_and_escape`: başlık 201 karakter → 422; `<b>x</b>` başlık → kaydedilir, sayfada escape'li.
  - `api_shapes`: `/api/catalog/products` JSON'unda `items,total,page,pages,categories`; `/api/kanban/boards/1` `columns` 4 anahtar; `DELETE /api/kanban/cards/{id}` 204.
  - `static_mime`: `/static/app.css` → `text/css`; `.js` → `text/javascript`; `.svg` → `image/svg+xml`; `.wasm` → `application/wasm`; `../` içeren yol → 404.
  - `test_chart.cpp` `chart_deterministic`: sabit 3 noktalı girdi için beklenen SVG string'i birebir.
- [ ] **Step 2:** `ctest --test-dir build -R handlers` → FAIL.
- [ ] **Step 3:** Uygula (handler gövdeleri, şablonlar).
- [ ] **Step 4:** `ctest --test-dir build` → PASS.
- [ ] **Step 5:** Commit `feat: shared templates, handlers and chart`.

### Task 6: İstemci tarafı JS (htmx, Alpine, Vanilla TS)

**Files:**
- Create: `frontends/vanilla/src/{catalog,kanban,dashboard}.ts`, `frontends/alpine/src/{catalog,kanban,dashboard}.ts`, `frontends/htmx/src/htmx-config.ts`, `frontends/build.ts`

**Interfaces:**
- Consumes: Task 5 route tablosu ve `/api` JSON'u, testid'ler.
- Produces: `bun frontends/build.ts` → `shared/static/js/{vanilla,alpine,htmx}/*.js` (minify, ESM). Davranış: Vanilla/Alpine arama/filtre/sayfalama/aralık değişiminde `history.pushState` + `/api` JSON ile ilgili bölgeyi yeniden çizer; markup `_base` ile aynı olmalı (Vanilla aynı HTML'i template literal ile üretir, Alpine `x-for` ile). htmx `hx-get`/`hx-post` + `hx-push-url`.
- [ ] **Step 1:** Uygula; arama input'u 250 ms debounce (tüm fe'lerde aynı; htmx `hx-trigger="input changed delay:250ms"`).
- [ ] **Step 2:** `bun frontends/build.ts` → çıktılar oluşur, hata yok.
- [ ] **Step 3:** Commit `feat: client glue for htmx, alpine, vanilla`.
  (Davranış doğrulaması Task 8'deki akışlarla yapılır.)

### Task 7: Referans backend (cpp-httplib) + contract testi

**Files:**
- Create: `backends/httplib/{main.cpp,CMakeLists.txt,deps.txt}`, `bench/contract/contract.test.ts`, `bench/package.json`

**Interfaces:**
- Consumes: `bs::routes()`, `bs::serve_static()`, `bs::init()`.
- Produces: binary `build/backends/httplib/bs-httplib` (CLI Global Constraints'teki gibi). `contract.test.ts`: `BASE` env (varsayılan `http://localhost:8080`); route tablosundaki her satır için durum kodu, content-type, JSON şekli/testid'ler; `invalid params` grubu (Review Focus 2,5); `concurrency smoke`: 64 paralel GET `/api/catalog/products` → hepsi 200. `deps.txt`: backend'in dış bağımlılıkları, satır başına bir.

- [ ] **Step 1:** Contract testini yaz; backend yokken `bun test bench/contract` → FAIL (bağlantı reddi).
- [ ] **Step 2:** `main.cpp`: route'ları httplib router'ına native kaydet (`{param}` → httplib `:param`), thread pool = `--threads`.
- [ ] **Step 3:** Build + `bs-httplib --port 8080 --db data/app.sqlite --root . --threads 8 &` ; `bun test bench/contract` → PASS.
- [ ] **Step 4:** Commit `feat: httplib reference backend and contract tests`.

### Task 8: Akış testleri ve parite araçları

**Files:**
- Create: `bench/flows/{targets.ts,catalog.ts,kanban.ts,dashboard.ts}`, `bench/parity/{functional.ts,visual.ts,seo.ts}`, `bench/parity/reference/` (PNG'ler)

**Interfaces:**
- Produces:
  - `targets.ts`: `baseUrl(fe, usecase)` → C++ fe: `http://localhost:8080/{fe}/{usecase}`; `sveltekit|astro`: `http://localhost:3000/{usecase}`. Kanban yolu `/kanban/1`.
  - Her akış: `export async function run(page: Page, base: string, step: (name: string, fn: () => Promise<void>) => Promise<void>)`; adımlar DOM durumunu bekler (navigasyonu değil). Akışlar idempotent.
    - catalog: `load_list` (24 kart) → `search_lamp` (tüm `product-name` "lamp" içerir) → `filter_category` → `next_page` → `open_detail` → `add_to_cart` (badge 1) → `open_cart` → `qty_2` (`cart-total` güncellenir) → `remove` (`empty-state`).
    - kanban: `load_board` → `add_card` ("Bench card", backlog'da) → `edit_title` → `move_right` (todo'da) → `delete` (yok) → kart sayısı başlangıçtakine eşit.
    - dashboard: `load` (range 30) → `range_7` (`kpi-events` = `/api/dashboard?range=7` events, tr-TR biçimli).
  - `functional.ts <fe>`: 3 akışı koşar → `{ok, failures[]}`; `visual.ts <fe> [--update-reference]`: 1280×900, sabit sayfalar listesi, pixelmatch oranı; `seo.ts <fe>`: JS kapalı, liste ve detayda `<title>`, meta description, `h1`, 24 kart.
- [ ] **Step 1:** Akışları ve araçları yaz; httplib + vanilla ile `node bench/parity/visual.ts vanilla --update-reference` referansları üretir.
- [ ] **Step 2:** `node bench/parity/functional.ts {htmx,alpine,vanilla}` → üçü `ok`.
- [ ] **Step 3:** `node bench/parity/visual.ts {htmx,alpine}` → her sayfa oran < 0.005; `seo.ts` üçünde ok.
- [ ] **Step 4:** Commit `feat: user flows and parity checks`.

### Task 9–12: Diğer C++ backend'ler

Her biri aynı kalıp; ayrı task, ayrı commit. **Files:** `backends/<name>/{main.cpp,CMakeLists.txt,deps.txt}`. **Consumes:** `bs::routes()` vb. **Produces:** `build/backends/<name>/bs-<name>`, aynı CLI.

| Task | Backend | Bağımlılık | Eşzamanlılık notu |
|---|---|---|---|
| 9 | Drogon | brew `drogon` | `app().setThreadNum(threads)`; route'lar `registerHandler` ile, `{fe}` path param |
| 10 | Crow | FetchContent CrowCpp (kesin tag) + brew asio | `app.concurrency(threads).multithreaded()`; `CROW_ROUTE` dinamik değil → `app.route_dynamic` |
| 11 | Oat++ | FetchContent oatpp (kesin tag) | `HttpRouter` + `HttpConnectionHandler`/async; worker = threads |
| 12 | uWebSockets | FetchContent uWebSockets + uSockets (SSL yok) | **Önce spike:** macOS'ta thread başına App + `SO_REUSEPORT` dağılımını 8 thread'de `oha` ile ölç; dengesizse (en yoğun thread > %40 istek) tek dinleyici + iş thread havuzu modeline geç, kararı `backends/uwebsockets/NOTES.md`'ye yaz |

Her task'ın adımları:
- [ ] **Step 1:** Backend'i yaz, Release build.
- [ ] **Step 2:** `data/app.sqlite`'ı pristine'den sıfırla, başlat; `bun test bench/contract` → PASS.
- [ ] **Step 3:** `node bench/parity/functional.ts {htmx,alpine,vanilla}` ve `visual.ts` → PASS.
- [ ] **Step 4:** Commit `feat: <name> backend`.

### Task 13: SvelteKit (Bun SSR)

**Files:**
- Create: `frontends/sveltekit/**` (`svelte-adapter-bun`), `src/lib/api.ts`, `src/lib/chart.ts`, `src/routes/{catalog,kanban/[board],dashboard}/**`, `src/routes/api/[...path]/+server.ts`

**Interfaces:**
- Consumes: C++ `/api` (Task 5 JSON), `shared/static/app.css` (build sırasında kopyalanır), testid'ler, `daily_chart_svg` algoritması.
- Produces: `bun run build && API_BASE=http://localhost:8080 PORT=3000 bun build/index.js`; `/healthz`; `/api/*` → C++'a geçiş (proxy), `bs_sid` cookie'si `x-session-id` başlığıyla iletilir. Mutasyonlar SvelteKit form actions ile (JS kapalıyken de çalışır); `chart.ts` C++ algoritmasının birebir portu.
- [ ] **Step 1: Failing test** `src/lib/chart.test.ts` (`bun test`): Task 5 `chart_deterministic` ile aynı girdi → aynı SVG string.
- [ ] **Step 2:** Uygula; `bun test` PASS.
- [ ] **Step 3:** httplib backend + SvelteKit çalışırken: `functional.ts sveltekit`, `visual.ts sveltekit`, `seo.ts sveltekit` → PASS.
- [ ] **Step 4:** Commit `feat: sveltekit frontend on bun`.

### Task 14: Astro + Preact adaları (Bun SSR)

**Files:**
- Create: `frontends/astro/**` (`@astrojs/node` standalone, Bun ile çalıştırılır), `src/lib/{api.ts,chart.ts}`, `src/pages/{catalog,kanban/[board],dashboard}.astro`, `src/pages/api/[...path].ts`, `src/islands/*.tsx` (Preact)

**Interfaces:** Task 13 ile aynı sözleşme: `bun run build && API_BASE=... PORT=3000 bun dist/server/entry.mjs`, `/healthz`, `/api/*` proxy, `chart.ts` portu. Sayfa geçişleri tam sayfa; arama/sepet/kanban/aralık etkileşimleri Preact adaları.
- [ ] **Step 1: Failing test** `chart.test.ts` (Task 13 ile aynı beklenen string).
- [ ] **Step 2:** Uygula; `bun test` PASS.
- [ ] **Step 3:** `functional.ts astro`, `visual.ts astro`, `seo.ts astro` → PASS.
- [ ] **Step 4:** Commit `feat: astro + preact frontend on bun`.

### Task 15: Ölçüm harness'ı ve ham veri DB'si

**Files:**
- Create: `bench/harness/{schema.sql,resultsdb.ts,procs.ts,sampler.ts,pageload.ts,interactions.ts,loadtest.ts,dx.ts,run.ts}`, `bench/harness/resultsdb.test.ts`

**Interfaces:**
- Consumes: Task 7–14 binary'leri ve Bun uygulamaları, Task 8 akışları ve parite araçları.
- Produces:
  - `schema.sql` — spec §10 tabloları birebir, ek olarak `samples_json` yok; her ölçüm satır olarak. `resources(run_id, combo_id, phase, process_role, pid, ts, rss_kb, cpu_pct, threads, fds)`.
  - `run.ts` CLI: `node bench/harness/run.ts [--usecases catalog,kanban,dashboard] [--backends ...] [--frontends ...] [--profiles desktop,mobile] [--iters 10] [--load-duration 10] [--concurrency 1,16,64] [--seed 1] [--quick]`; `--quick` = iters 2, süre 3 sn, concurrency 16.
  - Kombinasyon sırası `--seed` ile karıştırılır (Fisher–Yates), sıra `env`'e yazılır. Backend binary'leri ve Bun build'leri koşudan önce bir kez derlenir.
  - Kombinasyon başına sıra: pristine → app.sqlite kopyala (+ `-wal/-shm` sil) → backend başlat (+ Bun) → `cold_start_ms` (spawn → `/healthz` 200) → parite (functional/visual/seo, sonuç `parity`) → ısınma (her sayfa 3 yükleme) → page loads (her profil, her sayfa, N iter, yeni context, cache kapalı) → interactions (akış adımları, N iter, adım süresi = aksiyon başlangıcı → beklenen DOM, Node `performance.now()`) → load tests (`oha --json -z <d>s -c <c>`; sayfa: senaryonun GET sayfaları; API: `/api/catalog/products`, `/api/kanban/boards/1`, `/api/dashboard?range=30`; Bun kombinasyonlarında hem Bun sayfası hem C++ API) → süreçleri durdur → 5 sn soğuma.
  - `sampler.ts`: her rol için 250 ms'de `ps -o rss=,%cpu= -p`, 1 sn'de thread (`ps -M`) ve fd (`lsof -p`) sayısı; `phase` etiketiyle yazar.
  - Page load metrikleri: navigation timing (ttfb, dcl, load), paint (fcp), LCP (`addInitScript` PerformanceObserver), TBT (longtask toplamı `max(0, d-50)`, FCP→load+1 sn), CDP `Network.loadingFinished` toplam bayt + JS bayt + istek sayısı, `Runtime.getHeapUsage`. Mobile: `Emulation.setCPUThrottlingRate(4)`, `Network.emulateNetworkConditions({latency:150, downloadThroughput:200000, uploadThroughput:93750})`.
  - `dx.ts`: `tokei` (backend klasörü / frontend klasörü ayrı, `shared/` ayrı), temiz build süresi, artefakt boyutu (binary veya Bun build dizini), bağımlılık sayısı (`deps.txt` satırları / `package.json` dependencies).
  - `env` satırı: makine, OS, `versions.json` içeriği, seed, sıra, tarih.
- [ ] **Step 1: Failing test** `resultsdb.test.ts` (`node --test`): şema oluşturma; `insertPageLoad` / `insertResource` sonrası satır sayıları; aynı `run_id` ikinci kez açılınca tablolar korunur.
- [ ] **Step 2:** `node --test bench/harness` → FAIL; uygula; → PASS.
- [ ] **Step 3:** `node bench/harness/run.ts --quick --usecases catalog --backends httplib,drogon --frontends vanilla,sveltekit` → biter; `sqlite3 results/results.sqlite "select count(*) from page_loads"` > 0, `resources`'ta hem `cpp_backend` hem `bun_ssr` rolü var.
- [ ] **Step 4:** Commit `feat: benchmark harness and raw results db`.

### Task 16: Puanlama, tam koşu ve rapor

**Files:**
- Create: `scoring/{score.ts,weights.json,score.test.ts}`

**Interfaces:**
- Consumes: `results/results.sqlite` (yalnız okur + `scores` tablosu yazar).
- Produces: `node scoring/score.ts [--run <run_id>] [--weights scoring/weights.json]` → `scores(run_id, combo_id, category, metric, value, score)` + `results/report.md`.
  - Normalizasyon (spec §11): medyan; düşük-iyi `best/value×100`, yüksek-iyi `value/best×100`, aynı senaryo içinde; hata `100×(1−oran)`; varyans `max(0, 100−5×CV%)`; Bun kombinasyonlarında kaynak = iki rolün toplamı; DX = (backend DX + frontend DX)/2; `parity_ok=0` sıralama dışı.
  - `weights.json`: `{"user":40,"server":25,"dx":20,"stability":15}` + kategori içi metrik ağırlıkları (eşit varsayılan).
  - Rapor tabloları: (1) senaryo başına 25 kombinasyon, kategori puanları + toplam /100, (2) genel sıralama (3 senaryo ortalaması), (3) backend ortalamaları, (4) frontend ortalamaları, (5) parite dışı kalanlar.
- [ ] **Step 1: Failing tests** `score.test.ts`: sentetik mini DB ile: en iyi değer 100 alır; değeri 2× kötü olan 50 alır; hata oranı 0 → 100; CV %4 → 80; `parity_ok=0` kombinasyon sıralamada yok; ağırlık dosyası değişince toplam değişir.
- [ ] **Step 2:** `node --test scoring` → FAIL; uygula; → PASS.
- [ ] **Step 3:** Tam koşu (tahmini 5–7 saat): `node bench/harness/run.ts` (tüm kombinasyonlar, iki profil, iters 10).
- [ ] **Step 4:** `node scoring/score.ts` → `results/report.md`; 75 kombinasyon ya puanlı ya da "parite dışı" listesinde.
- [ ] **Step 5:** Commit `feat: scoring and report`.
