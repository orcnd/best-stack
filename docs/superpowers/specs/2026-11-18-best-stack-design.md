# Best Stack — Tasarım Dokümanı

Tarih: 2026-11-18 · Durum: Taslak (onay bekliyor)

## 1. Amaç ve neden

"Best Stack", web için en iyi yazılım stack'ini (backend + frontend) **ölçerek** bulmak için bir deney düzeneği. Backend her zaman C++, veritabanı her zaman SQLite. Aynı üç demo uygulama 5 C++ backend × 5 frontend kombinasyonuyla birebir aynı görünüm ve işlevle yazılır, kullanıcı perspektifinden test edilir, puanlanır. Tüm ham veri ayrı bir SQLite veritabanında saklanır ki sonradan farklı ağırlıklarla yeniden yorumlanabilsin.

Başarı: 75 kombinasyonun hepsi aynı fonksiyonel testi geçiyor, ekran görüntüleri eşdeğer, her biri için tekrarlanabilir ölçümler `results.sqlite`'ta, ve bu ham veriden üretilen puan tablosu.

## 2. Hedefler / Hedef olmayanlar / Anti-hedefler

**Hedefler**
- 3 senaryo × 5 backend × 5 frontend = 75 çalışan kombinasyon.
- Her senaryoda tüm kombinasyonlar aynı DOM yapısı, aynı CSS, aynı davranış.
- Kullanıcı perspektifli performans ölçümü (gerçek tarayıcı) + sunucu yük ölçümü + geliştirici deneyimi metrikleri.
- Ham veri `results.sqlite`'ta; puanlama ayrı, yeniden çalıştırılabilir bir betik.
- Puan tablosu (markdown) — kombinasyon, backend, frontend ve senaryo bazında.

**Hedef olmayanlar**
- Kimlik doğrulama, yetkilendirme, üretim güvenliği, HTTPS.
- Docker, Linux üzerinde doğrulama, dağıtım.
- Sürükle-bırak (Kanban'da taşıma butonlarla yapılır; deterministik test için).
- Gerçek ödeme, e-posta vb. yan etkiler.

**Anti-hedefler** (teknik olarak çalışsa da başarısızlık sayılır)
- Kombinasyonlar arasında farklı markup, farklı CSS, farklı sorgu veya farklı veri → karşılaştırma geçersiz.
- Debug build ile ölçüm, ısınmamış ölçüm, cache'li sonuçların tekrar kullanımı.
- Deterministik olmayan mock veri.
- Fonksiyonel veya görsel parite testinden geçmeyen bir kombinasyonun sıralamaya girmesi.

## 3. Kısıtlar

- Makine: Apple M2, 8 çekirdek, 8 GB RAM, macOS. Her şey tek makinede, kombinasyonlar **sırayla** ayağa kalkar.
- C++20, Apple clang 21, Release (`-O3`, `-DNDEBUG`). Bağımlılıklar Homebrew veya CMake FetchContent ile.
- Veritabanı yalnızca SQLite (WAL modu).
- HTML üretimi: mümkün olan her yerde C++. Buna uymayan frontend'ler kendi SSR sunucusunu **Bun** ile çalıştırır (Node değil).
- Mock veri sabit seed ile üretilir; her ölçüm koşusu öncesi uygulama DB'si temiz kopyadan sıfırlanır.

## 4. Yaklaşımlar (değerlendirilen ve seçilen)

| Yaklaşım | Özet | Artı | Eksi |
|---|---|---|---|
| A | C++ sadece JSON API; tüm frontend'ler kendi Node/Bun SSR sunucusuyla | Modern, temiz ayrım | Kullanıcının hissettiği hızı JS sunucusu belirler, C++ geri planda kalır |
| **B (seçildi)** | HTML'i C++ üretir; üstünde hafif JS kütüphanesi | Runtime'da JS sunucusu yok, "her şey C++" felsefesi | Ağır framework'ler bu modele uymaz |
| C | Karışık | İki mimariyi kıyaslar | — |

**Karar (kullanıcı):** Mümkün olan her yerde B. Uyumsuz framework'ler Bun SSR sunucusuyla, veriyi C++ JSON API'sinden alarak. Dağılım 3 (C++ render) + 2 (Bun SSR).

## 5. Kullanıcının verdiği kararlar

- Backend'ler: **Drogon, Crow, Oat++, uWebSockets, cpp-httplib**.
- Frontend'ler: **htmx, Alpine.js, Vanilla TS** (HTML'i C++ üretir) + **SvelteKit, Astro** (Bun SSR, C++ JSON API'sinden veri alır).
- DB: yalnızca SQLite.
- Senaryolar: Ürün kataloğu, Kanban, Analitik dashboard (aşağıda).
- Ham veri ayrı SQLite'ta (`results.sqlite`).

## 6. Mimari

```
best-stack/
  shared/
    schema.sql               # 3 senaryonun tabloları
    seed/                    # deterministik mock veri üreteci → data/pristine.sqlite
    css/app.css              # TEK stil dosyası, tüm frontend'ler aynen kullanır
    markup-spec.md           # her sayfanın DOM iskeleti, data-testid'ler
    cpp/
      datalayer/             # sqlite3 C API, prepared statement'lar; 3 senaryonun tüm sorguları
      render/                # inja + nlohmann::json; frontend başına şablon setleri
        templates/{htmx,alpine,vanilla}/{catalog,kanban,dashboard}/*.html
      api/                   # JSON şekillerinin (DTO) tek tanımı
  backends/{drogon,crow,oatpp,uwebsockets,httplib}/   # SADECE HTTP katmanı + yönlendirme
  frontends/
    htmx/  alpine/  vanilla/  # istemci tarafı JS/TS (C++ şablonlarıyla birlikte)
    sveltekit/  astro/        # Bun SSR uygulamaları
  bench/                      # Playwright + yük testi + metrik toplayıcı
  scoring/                    # results.sqlite → puan tabloları
  data/  results/
```

**Adillik ilkesi:** Veri katmanı ve şablon render'ı tüm backend'lerde ortak kütüphanedir. Backend'ler arasında değişen tek şey HTTP sunucusu, yönlendirme, eşzamanlılık modeli ve framework'ün istek/yanıt yığınıdır. Bu bilinçli bir seçim: ölçülen fark framework'ten gelsin, ORM veya şablon motorundan değil.

**URL şeması (her backend aynı):**
- `/{fe}/{usecase}/...` → C++'ın render ettiği sayfalar, `fe ∈ {htmx, alpine, vanilla}`.
- `/{fe}/{usecase}/_frag/...` → htmx için HTML parçaları.
- `/api/{usecase}/...` → JSON API (Alpine/Vanilla etkileşimleri ve Bun SSR frontend'leri için).
- `/static/...` → CSS ve JS varlıkları (statik sunum C++ backend'den).
- Bun SSR frontend'leri kendi portlarında çalışır, `API_BASE` ortam değişkeniyle seçilen C++ backend'e bağlanır; statik varlıkları kendileri sunar.

**Eşzamanlılık:** Her backend 8 çekirdeği kendi doğal yöntemiyle kullanacak şekilde yapılandırılır (thread havuzu veya çekirdek başına event loop). SQLite bağlantısı thread başına.

## 7. Senaryolar

Her senaryonun DOM iskeleti ve `data-testid`'leri `shared/markup-spec.md`'de tanımlanır; tüm frontend'ler buna uyar.

### 7.1 Ürün kataloğu (okuma + SEO ağırlıklı)
- Veri: ~2.000 ürün, 12 kategori, ürün başına 1 görsel (yerel, aynı boyutta SVG/WebP placeholder), fiyat, stok, puan, açıklama.
- Sayfalar: liste (24/sayfa, sayfalama), arama (`q`), kategori filtresi, fiyat sıralaması; ürün detay; sepet.
- Etkileşimler: arama yazınca liste güncellenir; filtre değiştirince liste güncellenir; "Sepete ekle" → sepet rozeti artar; sepet sayfasında adet değiştir / sil.
- Sepet: cookie'deki oturum id'si ile SQLite'ta.
- SEO şartı: liste ve detay sayfaları JS kapalıyken tam içerikle gelir.

### 7.2 Kanban (yazma + etkileşim ağırlıklı)
- Veri: 5 pano, pano başına ~100 kart, 4 sütun (Backlog, Todo, Doing, Done), kart: başlık, açıklama, etiketler, atanan kişi, tarih.
- Etkileşimler: kart ekle, başlık düzenle (satır içi), sütunlar arası taşı (← → butonları), sil, pano değiştir.
- Her yazma SQLite'a gider; sayfa yenilenince durum korunur.

### 7.3 Analitik dashboard (ağır sorgu)
- Veri: ~100.000 olay (zaman, kullanıcı, olay tipi, ülke, gelir), son 180 gün.
- Görünüm: tarih aralığı seçici (7/30/90 gün), 4 KPI kartı (olay, tekil kullanıcı, gelir, dönüşüm), günlük zaman serisi grafiği, ülke dağılımı, en çok gelir getiren 10 kalem tablosu.
- Grafikler **sunucu tarafında inline SVG** olarak, aynı algoritmayla üretilir (C++ render eden frontend'lerde C++, Bun frontend'lerinde TS portu; görsel parite testi doğrular). Ek grafik kütüphanesi yok.
- Etkileşim: aralık değiştirince tüm paneller güncellenir.

### 7.4 Frontend'lere göre etkileşim modeli
| Frontend | İlk render | Etkileşim |
|---|---|---|
| htmx | C++ (inja) | Sunucudan HTML parçası, `hx-*` ile değiştirme |
| Alpine.js | C++ (inja) | `/api` JSON + Alpine reaktivitesi |
| Vanilla TS | C++ (inja) | `/api` JSON + elle DOM güncelleme (esbuild/bun build ile tek bundle) |
| SvelteKit | Bun SSR | Hidrasyon + `/api` JSON |
| Astro | Bun SSR | Varsayılan sıfır JS; etkileşimli parçalar **Preact** adası olarak |

**Kullanıcı kararı — doğal kullanım:** Her frontend sayfa geçişlerini ve güncellemeleri kendi doğal yöntemiyle yapar (SvelteKit istemci tarafı yönlendirme, htmx HTML parçası, Alpine/Vanilla JSON + DOM güncelleme, Astro tam sayfa geçiş + adalar). Ölçüm "bu stack'i normal kullanırsam ne alırım" sorusunu cevaplar.

**Ortak kurallar (builder kararı):**
- URL her zaman durumu yansıtır: arama `q`, kategori, sıralama, sayfa, pano, tarih aralığı query/path parametresidir.
- İlerlemeli geliştirme: ortak markup gerçek `<a href>` linkleri ve `GET` formları kullanır; JS kapalıyken arama/filtre/sayfalama tam sayfa yüklemesiyle çalışır. JS bunların üstüne bağlanır.
- Playwright akışı her adımda "navigasyon" değil "beklenen DOM durumu"nu bekler (`data-testid` + içerik); böylece tam sayfa ve istemci tarafı güncelleme aynı testle ölçülür.
- Astro adaları için Preact seçildi: Svelte'i SvelteKit zaten temsil ediyor, Preact çeşitlilik ve küçük boyut sağlıyor.

## 8. Doğrulama (sıralamaya girmenin şartı)

- **Fonksiyonel parite:** Her senaryo için tek bir Playwright akış testi; 75 kombinasyonun hepsinde aynen çalışır ve geçmelidir.
- **Görsel parite:** Her sayfanın ekran görüntüsü, referans kombinasyonla piksel farkı (pixelmatch) eşik altında olmalıdır. Referans: `httplib + vanilla`.
- **SEO paritesi:** JS kapalı yüklemede liste/detay içeriği, başlık ve meta etiketleri mevcut.
- Geçmeyen kombinasyon ölçülür ama ham veride `parity_ok = 0` işaretlenir ve sıralamaya girmez.
- **Kapı yalnızca bu üç testtir (builder kararı).** Framework'e özgü işaretlemeler (`hx-*`, `x-*`, SvelteKit hidrasyon işaretleri, `<astro-island>` sarmalayıcıları) serbesttir; `markup-spec.md` görünen yapıyı ve `data-testid`'leri tanımlar, yapısal DOM diff'i yapılmaz. Anti-hedefteki "farklı markup" bu görünen yapıyı kasteder.

## 9. Ölçüm

### 9.1 Kullanıcı perspektifi (Playwright, Chromium)
- İki profil: **desktop** (kısıtsız) ve **mobile** (4× CPU yavaşlatma + ağ kısıtı: ~1.6 Mbps, 150 ms RTT).
- Sayfa yükleme (her sayfa, soğuk, yeni context, cache kapalı, N=10): TTFB, FCP, LCP, DOMContentLoaded, load, toplam aktarılan bayt, JS bayt, istek sayısı, long task toplamı (TBT benzeri), JS heap.
- Etkileşimler (senaryo akışındaki her adım, N=10): tıklama/yazma anından DOM'daki beklenen değişikliğe kadar geçen süre (ms).

### 9.2 Sunucu perspektifi (yük testi, `oha`)
- Her senaryonun ana sayfaları + 1–2 API ucu; ısınma sonrası 10 sn, eşzamanlılık 1/16/64.
- req/s, p50/p95/p99 gecikme, hata sayısı.
- Süreç kaynakları: RSS tepe/ortalama, CPU yüzdesi (backend ve varsa Bun süreci ayrı ayrı).
- **Kullanıcı kararı — Bun kombinasyonları (SvelteKit, Astro) iki seviyede ölçülür:**
  1. *Stack seviyesi:* kullanıcının vurduğu sayfa adresi (Bun portu) yük testine girer.
  2. *Backend seviyesi:* aynı anda C++ `/api` uçları doğrudan yük testine girer.
  Her iki testte de **her sürecin tüm kaynak kullanımı ayrı ayrı** kaydedilir (C++ süreci ve Bun süreci için RSS, CPU, thread sayısı, açık dosya/soket sayısı, örnekleme ile zaman serisi). Birleştirme (toplam bellek, toplam CPU) ham veride yapılmaz; raporlama/puanlama aşamasında yapılır. C++ render eden 25 kombinasyonda tek süreç vardır, aynı tablolara aynı şekilde yazılır.
- **Yazma işlemleri (builder kararı):** Kanban ve sepet yazma uçları oha yük testine girmez (SQLite tek yazıcı darboğazı backend'leri eşitlerdi); yalnızca Playwright akışlarında ölçülür. Akışlar idempotenttir: eklediği kartı siler, taşıdığını geri taşır, sepete ekleneni çıkarır. DB her kombinasyon başında temiz kopyadan sıfırlanır; iterasyonlar arası sıfırlama yapılmaz, akışların idempotent olduğu her iterasyon sonunda kayıt sayısı kontrolüyle doğrulanır.

### 9.3 Geliştirici deneyimi (statik)
- Kombinasyona özgü kod satırı (ortak kod hariç), bağımlılık sayısı, temiz derleme süresi, binary/bundle boyutu, soğuk başlangıç süresi (süreç başlatma → ilk 200 yanıt).

### 9.4 Koşu disiplini
- Release build; her kombinasyon öncesi DB sıfırlama; ısınma turu; kombinasyon sırası rastgele (seed kayıtlı); kombinasyonlar arası soğuma bekleme; arka plan sürecleri minimum.
- Ortam bilgisi (sürümler, makine, tarih, git benzeri hash) her koşuya kaydedilir.

## 10. Ham veri: `results/results.sqlite`

Ham örnekler (agregasyon değil) saklanır. Taslak şema:
- `env(run_id, started_at, machine, os, versions_json, seed, notes)`
- `combos(combo_id, backend, frontend, usecase)`
- `parity(run_id, combo_id, functional_ok, visual_diff_ratio, seo_ok, details_json)`
- `page_loads(run_id, combo_id, profile, page, iter, ttfb, fcp, lcp, dcl, load, bytes, js_bytes, requests, tbt, heap)`
- `interactions(run_id, combo_id, profile, step, iter, duration_ms)`
- `load_tests(run_id, combo_id, url, concurrency, duration_s, rps, p50, p95, p99, errors, raw_json)`
- `resources(run_id, combo_id, phase, process_role, pid, ts, rss_kb, cpu_pct, threads, fds)` — `process_role ∈ {cpp_backend, bun_ssr}`, `phase ∈ {idle, page_load, load_test_page, load_test_api}`
- `dx(run_id, combo_id, loc, deps, build_s, artifact_kb, cold_start_ms)`

## 11. Puanlama

Puanlama yalnızca `results.sqlite`'tan okur, kendi çıktısını ayrı tabloya (`scores`) ve markdown'a yazar; ağırlıklar bir config dosyasındadır, değiştirip yeniden çalıştırılabilir.

- Her metrik için örneklerin medyanı → 0–100'e normalize: düşük-iyi metriklerde `en_iyi / değer × 100`, yüksek-iyi metriklerde `değer / en_iyi × 100` (aynı senaryo içinde kıyaslanır).
- Kategoriler ve varsayılan ağırlıklar:
  - **Kullanıcı hızı %40** — LCP, TTFB, etkileşim gecikmesi, aktarılan bayt (desktop ve mobile ortalaması)
  - **Sunucu performansı %25** — req/s, p95, RSS
  - **Geliştirici deneyimi %20** — LOC, derleme süresi, bağımlılık, artefakt boyutu, soğuk başlangıç
  - **Kararlılık %15** — hata oranı, ölçüm varyansı (CV)
- **Uç durumlar (builder kararı):** Sıfırın en iyi olduğu metrikler oranla değil doğrudan puanlanır: hata → `100 × (1 − hata_oranı)`, varyans → `max(0, 100 − 5 × CV%)`. Kaynak kullanımında Bun kombinasyonları için puanlamada iki sürecin toplamı kullanılır (ham veride ayrı).
- **LOC atfı (builder kararı):** Backend LOC'u = o backend klasörünün tamamı (3 senaryo, 5 frontend'e hizmet eden yönlendirme dahil); frontend LOC'u = o frontend'in şablonları + istemci kodu (+ Bun uygulaması). Kombinasyon DX puanı = backend DX puanı ile frontend DX puanının ortalaması. Ortak kütüphaneler (`shared/`) kimseye atfedilmez, raporda ayrıca gösterilir.
- Çıktılar: kombinasyon sıralaması (senaryo bazında ve genel), backend ortalamaları, frontend ortalamaları; her hücrede 0–100 puan ve toplam.

## 12. Builder'a bırakılanlar

- Frontend başına şablon dosyalarının iç organizasyonu, Bun SSR uygulamalarının iskeleti.
- Mock veri içeriğinin detayları (isimler, açıklamalar) — deterministik olduğu sürece.
- Her framework'te "8 çekirdeği doğal yolla kullanma" yapılandırmasının ayrıntısı. **Risk:** macOS'ta `SO_REUSEPORT` yükü süreçler/loop'lar arasında dengelemez; uWebSockets'in çekirdek başına loop modeli için erken bir deneme (spike) yapılmalı, gerekirse tek accept thread'i + dağıtım modeli kullanılıp raporda not düşülmeli.
- Harness'ın Bun mu Node mu ile çalışacağı (Playwright uyumluluğuna göre); runtime sunucuları her durumda Bun.
- Görsel parite eşiği (makul bir başlangıç: %0.5 piksel farkı) ve ölçüm iterasyon sayılarının ince ayarı.
