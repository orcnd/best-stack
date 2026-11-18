# Best Stack — sonuçlar (run 2)

**11 backend × 5 frontend × 3 senaryo = 165 kombinasyon**, Apple M2 (8 çekirdek, 8 GB), macOS 26.7.1, 2026-11-18 (~7 saat).
Ham veri: [`docs/data/results-run2.sqlite`](data/results-run2.sqlite) — `node scoring/score.ts --db docs/data/results-run2.sqlite --weights <senin ağırlıkların>` ile yeniden puanlanabilir.
Run 1 (yalnız C++, düzeltilmiş ölçüm sorunlarıyla): [results-run1.md](results-run1.md).

## Özet

| Dil | Ortalama toplam (Zig 30 · C++ 75 · Rust 30 · Go 30 kombinasyon) | En iyi kombinasyon |
|---|---|---|
| Zig | 65.4 | 88.6 (http.zig × vanilla × catalog) |
| C++ | 61.4 | 85.6 |
| Rust | 60.2 | 82.5 |
| Go | 57.8 | 78.0 |

Katalog sayfası, 64 eşzamanlı istemci, HTML'i backend üretiyor (htmx/Alpine/Vanilla ortalaması, req/s):
zap 40.240 · http.zig 34.699 · Axum 23.120 · Actix 20.413 · uWebSockets 14.080 · Oat++ 13.522 · Crow 12.444 · cpp-httplib 10.401 · Drogon 8.323 · Fiber 5.741 · net/http 5.597

**Okuma notları**
- **HTML'i backend üretince (vanilla/htmx/alpine) her backend, Bun SSR'li SvelteKit/Astro'nun belirgin önünde.** SSR'in Bun katmanı sayfa başına throughput'u sınırlıyor ve ikinci süreç bellek/DX maliyeti getiriyor; masaüstünde kullanıcı tarafı farkı küçük.
- **Zig (http.zig, zap) ham throughput'ta en hızlısı**, Rust (Axum/Actix) ikinci. C++ framework'leri arasında uWebSockets/Oat++/Crow önde; Drogon büyük HTML sayfalarında geride.
- **Go'da darboğaz `html/template`**: API uçlarında Go diğerlerine yakın, HTML üretilen sayfalarda en yavaş.
- **Dashboard (ağır SQL) senaryosunda backend farkı kayboluyor** — SQLite sorgusu sınırlayıcı (~180–230 req/s).
- **zap'in DX puanı soğuk başlangıçtan düşük**: upstream zap `listen()` içinde 500 ms bekliyor (bilinen hata için konmuş bir uyku).

## Sıralama dışı

- **Fiber × SvelteKit × Kanban** — yük testinde %11,9 hata. 64 eşzamanlı istemcide SvelteKit sayfası 792 kez 500 döndü (Bun → Fiber bağlantıları); ardından gelen API testindeki "Can't assign requested address" hataları macOS geçici port tükenmesinden (önceki aşamanın TIME_WAIT bağlantıları) — yani ilk kısım gerçek, ikinci kısım ölçüm ortamı etkisi. Kök neden tam teşhis edilmedi.

## Bilinen karıştırıcılar (run 2)

- **Bağlantı havuzu boyutları dile göre farklı**: C++/Zig thread başına bağlantı (8; httplib 64 thread), Rust/Go havuz 16. Go her sorguda statement'ı yeniden hazırlıyor (database/sql), diğerleri cache'liyor.
- **DX derleme süresi**: Drogon ve asio Homebrew'dan hazır geliyor; Oat++, Rust crate'leri, Go `-a` ve Zig (facil.io + SQLite) sıfırdan derleniyor.
- **LOC**: backend = adaptör + dil çekirdeği, şablonlar dahil (Rust C++ ile aynı Jinja dosyalarını kullanıyor; Zig HTML'i kodda yazıyor).
- **Ağırlıklar** (Kullanıcı 40 · Sunucu 25 · DX 20 · Kararlılık 15) bir tercih; ham veriden farklı ağırlıklarla yeniden puanlanabilir.

## Ölçüm ilkeleri (run 2'de düzeltilenler)

- Uygulama katmanında sıkıştırma yok (sözleşme testi zorluyor); bellek = macOS phys_footprint; varyans = robust CV (MAD).
- Tüm backend'ler SQLite 3.54.1, aynı derleme seçenekleri (global mutex'li MEMSTATUS kapalı) ve aynı PRAGMA'lar.
- Etkileşim adımları sayfa etkileşime hazır olunca başlıyor; istemci tarafı olması gereken bir adım tam sayfa yenilemeye düşerse başarısız sayılıyor; mobil zaman aşımları profile göre.
- req/s yalnız başarılı yanıtlar; yük testinde >%1 hata → sıralama dışı; eksik ölçüm 0 sayılır.
- Her koşu git commit'i ve araç zinciri sürümleriyle kaydedilir (`env.versions_json.provenance`).

---

# Best Stack — sonuç raporu

Run: `run-2026-11-18T13-28-27-975Z` · Apple M2, 8 cpu, 8 GB · macOS 26.7.1 · başlangıç 2026-11-18T13:28:27.978Z · bitiş 2026-11-18T20:28:50.375Z

Ağırlıklar: Kullanıcı hızı 40 · Sunucu 25 · DX 20 · Kararlılık 15. Her hücre 0–100; toplam ağırlıklı ortalama (/100).

## Genel sıralama (3 senaryo ortalaması)

| # | Backend | Frontend | Kullanıcı | Sunucu | DX | Kararlılık | **Toplam** |
|---|---|---|---|---|---|---|---|
| 1 | httpzig | vanilla | 98 | 69 | 78 | 100 | **87.2** |
| 2 | crow | vanilla | 94 | 46 | 65 | 100 | **77.0** |
| 3 | zap | vanilla | 93 | 56 | 46 | 100 | **75.2** |
| 4 | drogon | vanilla | 94 | 49 | 52 | 100 | **75.2** |
| 5 | uwebsockets | vanilla | 91 | 50 | 59 | 90 | **74.3** |
| 6 | httpzig | alpine | 80 | 67 | 51 | 100 | **73.9** |
| 7 | axum | vanilla | 94 | 40 | 50 | 97 | **72.3** |
| 8 | oatpp | vanilla | 93 | 35 | 59 | 92 | **71.6** |
| 9 | actix | vanilla | 93 | 39 | 48 | 100 | **71.6** |
| 10 | zap | htmx | 78 | 69 | 36 | 100 | **70.6** |
| 11 | httpzig | htmx | 75 | 64 | 56 | 89 | **70.5** |
| 12 | httplib | vanilla | 91 | 33 | 60 | 89 | **70.2** |
| 13 | uwebsockets | htmx | 75 | 57 | 54 | 100 | **69.9** |
| 14 | fiber | vanilla | 94 | 30 | 46 | 100 | **69.5** |
| 15 | uwebsockets | alpine | 76 | 53 | 51 | 100 | **68.7** |
| 16 | crow | htmx | 74 | 50 | 48 | 100 | **66.8** |
| 17 | gohttp | vanilla | 89 | 20 | 50 | 100 | **65.5** |
| 18 | zap | alpine | 74 | 56 | 34 | 98 | **65.1** |
| 19 | axum | htmx | 76 | 43 | 42 | 100 | **64.5** |
| 20 | crow | alpine | 76 | 44 | 48 | 89 | **64.4** |
| 21 | drogon | htmx | 72 | 46 | 45 | 100 | **64.3** |
| 22 | httplib | htmx | 73 | 38 | 51 | 100 | **64.0** |
| 23 | axum | alpine | 77 | 41 | 38 | 100 | **63.7** |
| 24 | actix | htmx | 76 | 41 | 42 | 97 | **63.4** |
| 25 | oatpp | htmx | 75 | 36 | 50 | 97 | **63.4** |
| 26 | drogon | alpine | 74 | 45 | 41 | 93 | **63.0** |
| 27 | oatpp | alpine | 75 | 33 | 49 | 92 | **62.1** |
| 28 | httplib | alpine | 71 | 32 | 57 | 92 | **61.6** |
| 29 | actix | alpine | 77 | 38 | 36 | 91 | **61.1** |
| 30 | gohttp | htmx | 74 | 30 | 42 | 100 | **60.4** |
| 31 | gohttp | alpine | 75 | 29 | 39 | 100 | **59.9** |
| 32 | fiber | htmx | 73 | 30 | 37 | 97 | **58.8** |
| 33 | fiber | alpine | 74 | 27 | 35 | 97 | **57.9** |
| 34 | drogon | astro | 73 | 21 | 27 | 100 | **54.9** |
| 35 | httpzig | astro | 74 | 19 | 24 | 100 | **54.1** |
| 36 | uwebsockets | astro | 73 | 21 | 27 | 93 | **54.0** |
| 37 | zap | astro | 73 | 21 | 22 | 100 | **53.9** |
| 38 | axum | astro | 73 | 21 | 20 | 100 | **53.6** |
| 39 | drogon | sveltekit | 68 | 23 | 27 | 100 | **53.2** |
| 40 | gohttp | sveltekit | 68 | 25 | 23 | 100 | **53.1** |
| 41 | fiber | astro | 73 | 19 | 20 | 97 | **52.7** |
| 42 | crow | astro | 73 | 18 | 26 | 94 | **52.7** |
| 43 | httplib | astro | 71 | 21 | 27 | 90 | **52.6** |
| 44 | zap | sveltekit | 67 | 28 | 22 | 95 | **52.6** |
| 45 | uwebsockets | sveltekit | 68 | 16 | 26 | 100 | **51.4** |
| 46 | actix | sveltekit | 68 | 26 | 19 | 91 | **51.0** |
| 47 | crow | sveltekit | 68 | 15 | 27 | 97 | **50.8** |
| 48 | oatpp | sveltekit | 67 | 20 | 24 | 95 | **50.8** |
| 49 | fiber | sveltekit | 65 | 25 | 19 | 96 | **50.6** |
| 50 | actix | astro | 71 | 17 | 19 | 92 | **50.4** |
| 51 | httpzig | sveltekit | 67 | 23 | 24 | 89 | **50.4** |
| 52 | httplib | sveltekit | 65 | 21 | 27 | 89 | **50.0** |
| 53 | axum | sveltekit | 66 | 21 | 20 | 94 | **49.8** |
| 54 | gohttp | astro | 69 | 13 | 23 | 96 | **49.7** |
| 55 | oatpp | astro | 68 | 10 | 22 | 93 | **48.3** |

## Backend ortalamaları

| Backend | Toplam |
|---|---|
| httpzig | 67.2 |
| uwebsockets | 63.6 |
| zap | 63.5 |
| crow | 62.3 |
| drogon | 62.1 |
| axum | 60.8 |
| httplib | 59.7 |
| actix | 59.5 |
| oatpp | 59.3 |
| fiber | 58.4 |
| gohttp | 57.7 |

## Frontend ortalamaları

| Frontend | Toplam |
|---|---|
| vanilla | 73.6 |
| htmx | 65.1 |
| alpine | 63.8 |
| astro | 52.4 |
| sveltekit | 51.3 |

## Senaryo: catalog

| # | Backend | Frontend | LCP ms (masaüstü) | Etkileşim ms (masaüstü) | KB | req/s @64 | p95 ms | Bellek MB | Kullanıcı | Sunucu | DX | Kararlılık | **Toplam** |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | httpzig | vanilla | 24 | 838 | 20 | 28253 | 9.1 | 7 | 99 | 63 | 79 | 100 | **85.7** |
| 2 | zap | vanilla | 24 | 842 | 20 | 42355 | 1.9 | 68 | 97 | 70 | 46 | 100 | **80.4** |
| 3 | httpzig | htmx | 25 | 832 | 65 | 41427 | 7.1 | 8 | 78 | 72 | 69 | 100 | **77.9** |
| 4 | crow | vanilla | 23 | 834 | 20 | 14237 | 6.7 | 12 | 96 | 42 | 60 | 100 | **76.0** |
| 5 | uwebsockets | vanilla | 25 | 854 | 20 | 13004 | 9.5 | 10 | 95 | 43 | 56 | 100 | **74.9** |
| 6 | httplib | vanilla | 25 | 834 | 20 | 13668 | 3.3 | 33 | 94 | 37 | 56 | 100 | **73.1** |
| 7 | oatpp | vanilla | 23 | 854 | 20 | 11636 | 5.1 | 31 | 95 | 30 | 62 | 100 | **73.0** |
| 8 | axum | vanilla | 23 | 857 | 20 | 23282 | 6.1 | 62 | 97 | 33 | 50 | 100 | **71.7** |
| 9 | httpzig | alpine | 25 | 888 | 62 | 34417 | 8.3 | 8 | 79 | 67 | 41 | 100 | **71.4** |
| 10 | drogon | vanilla | 25 | 849 | 20 | 9034 | 16.9 | 11 | 94 | 33 | 51 | 100 | **71.3** |
| 11 | zap | htmx | 25 | 834 | 66 | 39637 | 2.1 | 69 | 79 | 66 | 36 | 100 | **70.1** |
| 12 | actix | vanilla | 23 | 850 | 20 | 20890 | 10.5 | 36 | 96 | 29 | 46 | 100 | **70.0** |
| 13 | uwebsockets | htmx | 25 | 837 | 66 | 14720 | 8.6 | 10 | 76 | 45 | 59 | 100 | **68.4** |
| 14 | zap | alpine | 24 | 869 | 63 | 38727 | 2.5 | 68 | 78 | 60 | 34 | 100 | **68.0** |
| 15 | fiber | vanilla | 23 | 833 | 20 | 6392 | 35.8 | 25 | 96 | 17 | 47 | 100 | **67.1** |
| 16 | uwebsockets | alpine | 25 | 867 | 63 | 14516 | 8.6 | 10 | 77 | 44 | 51 | 100 | **67.1** |
| 17 | crow | htmx | 25 | 836 | 66 | 14135 | 6.8 | 13 | 76 | 40 | 57 | 100 | **66.8** |
| 18 | gohttp | vanilla | 25 | 833 | 20 | 5339 | 40.7 | 30 | 94 | 14 | 49 | 100 | **65.9** |
| 19 | oatpp | alpine | 24 | 849 | 63 | 14433 | 4.0 | 32 | 78 | 35 | 53 | 100 | **65.6** |
| 20 | httplib | htmx | 26 | 837 | 65 | 12280 | 3.3 | 33 | 74 | 37 | 54 | 100 | **64.6** |
| 21 | oatpp | htmx | 25 | 852 | 66 | 14496 | 4.3 | 31 | 77 | 34 | 42 | 100 | **62.7** |
| 22 | axum | alpine | 25 | 870 | 63 | 23301 | 6.1 | 56 | 77 | 33 | 40 | 100 | **62.3** |
| 23 | crow | alpine | 25 | 849 | 63 | 8961 | 14.2 | 12 | 77 | 32 | 40 | 100 | **61.7** |
| 24 | axum | htmx | 25 | 849 | 65 | 22776 | 5.9 | 56 | 77 | 33 | 38 | 100 | **61.6** |
| 25 | drogon | alpine | 27 | 1003 | 63 | 9831 | 13.0 | 11 | 74 | 35 | 42 | 97 | **61.2** |
| 26 | actix | htmx | 25 | 832 | 65 | 19139 | 11.7 | 40 | 76 | 27 | 41 | 100 | **60.5** |
| 27 | actix | alpine | 25 | 854 | 63 | 21209 | 10.6 | 40 | 77 | 29 | 37 | 100 | **60.4** |
| 28 | drogon | htmx | 36 | 867 | 66 | 6103 | 23.2 | 10 | 69 | 31 | 44 | 100 | **59.3** |
| 29 | gohttp | htmx | 26 | 836 | 65 | 5751 | 37.6 | 32 | 76 | 14 | 42 | 100 | **57.3** |
| 30 | gohttp | alpine | 25 | 867 | 63 | 5700 | 38.0 | 28 | 76 | 15 | 38 | 100 | **56.8** |
| 31 | fiber | alpine | 26 | 868 | 63 | 5394 | 38.6 | 31 | 76 | 14 | 35 | 100 | **56.0** |
| 32 | fiber | htmx | 25 | 852 | 65 | 5437 | 40.1 | 31 | 75 | 14 | 37 | 100 | **55.9** |
| 33 | httplib | alpine | 28 | 1945 | 63 | 5253 | 58.8 | 31 | 62 | 13 | 56 | 85 | **52.2** |
| 34 | drogon | astro | 29 | 854 | 46 | 1403 | 69.7 | 117 | 71 | 4 | 27 | 100 | **49.9** |
| 35 | uwebsockets | astro | 30 | 869 | 46 | 1706 | 41.7 | 114 | 70 | 5 | 27 | 100 | **49.9** |
| 36 | httpzig | astro | 29 | 880 | 46 | 1715 | 40.3 | 106 | 71 | 5 | 24 | 100 | **49.4** |
| 37 | crow | astro | 29 | 904 | 46 | 1609 | 49.2 | 117 | 70 | 5 | 26 | 100 | **49.3** |
| 38 | uwebsockets | sveltekit | 27 | 853 | 121 | 3119 | 26.9 | 53 | 65 | 10 | 28 | 100 | **48.8** |
| 39 | zap | astro | 29 | 874 | 46 | 1662 | 40.9 | 141 | 70 | 5 | 22 | 100 | **48.7** |
| 40 | gohttp | astro | 31 | 873 | 46 | 1654 | 46.0 | 126 | 70 | 5 | 23 | 100 | **48.6** |
| 41 | httplib | sveltekit | 27 | 884 | 121 | 2913 | 16.1 | 78 | 64 | 9 | 27 | 100 | **48.4** |
| 42 | axum | astro | 29 | 853 | 46 | 1627 | 45.1 | 129 | 71 | 5 | 19 | 100 | **48.4** |
| 43 | crow | sveltekit | 27 | 883 | 121 | 3052 | 25.7 | 73 | 64 | 8 | 27 | 100 | **48.3** |
| 44 | actix | astro | 29 | 872 | 46 | 1574 | 50.3 | 127 | 71 | 4 | 19 | 100 | **48.3** |
| 45 | fiber | astro | 29 | 875 | 46 | 1684 | 45.3 | 121 | 70 | 5 | 20 | 100 | **48.2** |
| 46 | oatpp | astro | 33 | 890 | 46 | 1237 | 87.6 | 131 | 69 | 4 | 23 | 100 | **48.2** |
| 47 | httpzig | sveltekit | 27 | 852 | 121 | 3105 | 23.5 | 53 | 65 | 10 | 24 | 100 | **48.1** |
| 48 | drogon | sveltekit | 29 | 898 | 121 | 2571 | 35.4 | 64 | 62 | 8 | 28 | 100 | **47.5** |
| 49 | axum | sveltekit | 27 | 849 | 121 | 3033 | 24.5 | 68 | 65 | 9 | 22 | 100 | **47.4** |
| 50 | oatpp | sveltekit | 27 | 853 | 121 | 2981 | 24.3 | 95 | 64 | 8 | 23 | 100 | **47.3** |
| 51 | gohttp | sveltekit | 27 | 866 | 121 | 2942 | 31.0 | 64 | 64 | 8 | 23 | 100 | **47.3** |
| 52 | zap | sveltekit | 27 | 867 | 121 | 3100 | 26.5 | 98 | 65 | 7 | 22 | 100 | **47.0** |
| 53 | fiber | sveltekit | 27 | 882 | 121 | 3030 | 29.9 | 61 | 64 | 9 | 19 | 100 | **46.7** |
| 54 | httplib | astro | 37 | 900 | 46 | 1619 | 47.0 | 134 | 67 | 4 | 27 | 84 | **45.9** |
| 55 | actix | sveltekit | 30 | 889 | 121 | 3068 | 24.1 | 66 | 62 | 9 | 19 | 74 | **42.1** |

## Senaryo: dashboard

| # | Backend | Frontend | LCP ms (masaüstü) | Etkileşim ms (masaüstü) | KB | req/s @64 | p95 ms | Bellek MB | Kullanıcı | Sunucu | DX | Kararlılık | **Toplam** |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | httpzig | vanilla | 52 | 216 | 14 | 174 | 977.6 | 11 | 99 | 73 | 79 | 100 | **88.6** |
| 2 | crow | vanilla | 56 | 234 | 14 | 137 | 579.9 | 11 | 96 | 76 | 66 | 100 | **85.6** |
| 3 | drogon | vanilla | 56 | 233 | 14 | 193 | 594.9 | 13 | 96 | 79 | 55 | 100 | **84.1** |
| 4 | uwebsockets | vanilla | 56 | 234 | 15 | 148 | 772.7 | 10 | 96 | 74 | 60 | 100 | **84.0** |
| 5 | actix | vanilla | 56 | 217 | 14 | 198 | 434.3 | 26 | 97 | 75 | 50 | 100 | **82.5** |
| 6 | oatpp | vanilla | 56 | 218 | 14 | 184 | 479.8 | 63 | 97 | 62 | 50 | 100 | **79.1** |
| 7 | httplib | vanilla | 56 | 219 | 14 | 138 | 606.8 | 43 | 97 | 51 | 61 | 100 | **78.6** |
| 8 | httpzig | htmx | 52 | 216 | 61 | 200 | 708.6 | 11 | 77 | 81 | 62 | 100 | **78.4** |
| 9 | crow | alpine | 52 | 216 | 59 | 174 | 429.1 | 12 | 77 | 85 | 55 | 100 | **78.2** |
| 10 | fiber | vanilla | 52 | 238 | 14 | 175 | 638.5 | 27 | 97 | 61 | 45 | 100 | **78.0** |
| 11 | uwebsockets | htmx | 52 | 216 | 61 | 199 | 585.1 | 12 | 76 | 83 | 56 | 100 | **77.5** |
| 12 | axum | vanilla | 64 | 232 | 14 | 161 | 546.4 | 25 | 94 | 63 | 46 | 92 | **76.3** |
| 13 | uwebsockets | alpine | 56 | 218 | 59 | 179 | 644.0 | 11 | 75 | 80 | 55 | 100 | **76.2** |
| 14 | httplib | alpine | 56 | 219 | 59 | 198 | 441.2 | 40 | 75 | 69 | 68 | 100 | **75.9** |
| 15 | crow | htmx | 56 | 234 | 61 | 181 | 418.2 | 11 | 75 | 90 | 39 | 100 | **75.2** |
| 16 | drogon | htmx | 56 | 232 | 61 | 182 | 628.5 | 13 | 75 | 76 | 49 | 100 | **73.8** |
| 17 | actix | htmx | 52 | 216 | 61 | 194 | 429.3 | 27 | 76 | 74 | 43 | 100 | **72.6** |
| 18 | httpzig | alpine | 52 | 216 | 58 | 186 | 894.4 | 14 | 77 | 70 | 46 | 100 | **72.4** |
| 19 | zap | htmx | 52 | 217 | 61 | 204 | 372.6 | 38 | 76 | 76 | 36 | 100 | **71.7** |
| 20 | axum | htmx | 52 | 216 | 61 | 184 | 464.4 | 25 | 76 | 71 | 41 | 100 | **71.4** |
| 21 | httplib | htmx | 56 | 218 | 61 | 185 | 455.4 | 42 | 75 | 66 | 47 | 100 | **70.9** |
| 22 | oatpp | htmx | 54 | 217 | 61 | 187 | 464.5 | 66 | 76 | 63 | 53 | 91 | **70.1** |
| 23 | axum | alpine | 56 | 217 | 59 | 178 | 470.5 | 24 | 76 | 70 | 36 | 100 | **70.0** |
| 24 | gohttp | htmx | 56 | 217 | 61 | 194 | 575.8 | 28 | 75 | 66 | 41 | 100 | **69.8** |
| 25 | actix | alpine | 54 | 220 | 59 | 174 | 467.5 | 26 | 77 | 68 | 37 | 91 | **68.9** |
| 26 | gohttp | alpine | 56 | 235 | 59 | 176 | 642.6 | 28 | 75 | 61 | 38 | 100 | **67.7** |
| 27 | gohttp | vanilla | 80 | 320 | 14 | 88 | 1298.0 | 28 | 84 | 36 | 49 | 100 | **67.4** |
| 28 | fiber | htmx | 54 | 234 | 61 | 186 | 584.5 | 26 | 75 | 65 | 36 | 91 | **67.2** |
| 29 | drogon | alpine | 60 | 235 | 59 | 147 | 809.5 | 13 | 74 | 67 | 41 | 84 | **67.0** |
| 30 | uwebsockets | astro | 56 | 219 | 45 | 192 | 624.1 | 65 | 77 | 57 | 27 | 100 | **65.3** |
| 31 | fiber | alpine | 60 | 237 | 58 | 155 | 728.1 | 27 | 73 | 55 | 36 | 100 | **65.1** |
| 32 | zap | vanilla | 84 | 319 | 15 | 76 | 1824.3 | 37 | 83 | 29 | 46 | 100 | **64.6** |
| 33 | drogon | astro | 60 | 233 | 45 | 192 | 667.8 | 63 | 75 | 55 | 27 | 100 | **64.2** |
| 34 | zap | astro | 60 | 234 | 45 | 189 | 584.6 | 94 | 75 | 56 | 22 | 100 | **63.4** |
| 35 | axum | astro | 60 | 234 | 45 | 189 | 574.3 | 78 | 75 | 57 | 20 | 100 | **63.0** |
| 36 | httpzig | astro | 56 | 217 | 45 | 181 | 903.1 | 67 | 77 | 48 | 24 | 100 | **62.7** |
| 37 | gohttp | sveltekit | 56 | 218 | 116 | 186 | 601.7 | 71 | 69 | 56 | 24 | 100 | **61.3** |
| 38 | httplib | astro | 66 | 240 | 45 | 175 | 513.0 | 101 | 72 | 56 | 27 | 85 | **61.2** |
| 39 | actix | sveltekit | 56 | 233 | 116 | 180 | 502.1 | 68 | 68 | 59 | 19 | 100 | **60.9** |
| 40 | drogon | sveltekit | 56 | 235 | 116 | 165 | 708.4 | 48 | 68 | 52 | 27 | 100 | **60.7** |
| 41 | fiber | astro | 58 | 220 | 45 | 170 | 661.5 | 78 | 76 | 51 | 20 | 91 | **60.6** |
| 42 | oatpp | alpine | 62 | 255 | 59 | 144 | 608.8 | 64 | 72 | 49 | 40 | 76 | **60.4** |
| 43 | crow | astro | 60 | 237 | 45 | 144 | 714.6 | 68 | 75 | 46 | 26 | 92 | **60.3** |
| 44 | zap | sveltekit | 64 | 252 | 116 | 185 | 374.2 | 77 | 65 | 68 | 22 | 85 | **60.0** |
| 45 | httpzig | sveltekit | 56 | 277 | 116 | 174 | 925.7 | 48 | 67 | 49 | 24 | 100 | **58.6** |
| 46 | actix | astro | 62 | 236 | 45 | 145 | 713.0 | 87 | 74 | 45 | 19 | 92 | **58.5** |
| 47 | axum | sveltekit | 58 | 238 | 116 | 130 | 628.7 | 71 | 68 | 46 | 19 | 91 | **56.1** |
| 48 | zap | alpine | 82 | 320 | 59 | 97 | 909.1 | 38 | 63 | 39 | 34 | 94 | **55.8** |
| 49 | oatpp | sveltekit | 64 | 250 | 116 | 153 | 666.5 | 102 | 65 | 47 | 23 | 85 | **55.1** |
| 50 | fiber | sveltekit | 60 | 236 | 116 | 131 | 838.4 | 66 | 66 | 41 | 19 | 92 | **54.5** |
| 51 | uwebsockets | sveltekit | 60 | 235 | 116 | 85 | 1684.2 | 47 | 67 | 29 | 26 | 100 | **54.1** |
| 52 | crow | sveltekit | 60 | 234 | 116 | 68 | 1535.6 | 47 | 67 | 27 | 26 | 92 | **52.6** |
| 53 | gohttp | astro | 90 | 345 | 45 | 98 | 1084.1 | 85 | 63 | 31 | 22 | 89 | **50.8** |
| 54 | httplib | sveltekit | 76 | 325 | 116 | 82 | 505.7 | 75 | 59 | 43 | 26 | 67 | **49.6** |
| 55 | oatpp | astro | 98 | 339 | 45 | 71 | 1293.9 | 127 | 61 | 24 | 22 | 80 | **47.0** |

## Senaryo: kanban

| # | Backend | Frontend | LCP ms (masaüstü) | Etkileşim ms (masaüstü) | KB | req/s @64 | p95 ms | Bellek MB | Kullanıcı | Sunucu | DX | Kararlılık | **Toplam** |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | httpzig | vanilla | 32 | 360 | 126 | 22807 | 13.4 | 15 | 97 | 72 | 77 | 100 | **87.3** |
| 2 | zap | vanilla | 32 | 375 | 127 | 21024 | 3.9 | 77 | 98 | 70 | 46 | 100 | **80.7** |
| 3 | httpzig | alpine | 48 | 391 | 190 | 22763 | 12.3 | 20 | 83 | 65 | 67 | 100 | **78.0** |
| 4 | zap | alpine | 48 | 392 | 190 | 20476 | 4.2 | 59 | 82 | 68 | 34 | 100 | **71.5** |
| 5 | drogon | vanilla | 36 | 361 | 127 | 3885 | 32.1 | 17 | 90 | 36 | 51 | 100 | **70.1** |
| 6 | zap | htmx | 48 | 393 | 185 | 19147 | 4.5 | 59 | 79 | 64 | 36 | 100 | **69.9** |
| 7 | crow | vanilla | 36 | 361 | 127 | 2302 | 47.2 | 34 | 90 | 19 | 68 | 100 | **69.4** |
| 8 | axum | vanilla | 36 | 376 | 126 | 7540 | 18.6 | 71 | 92 | 24 | 55 | 100 | **69.0** |
| 9 | uwebsockets | vanilla | 64 | 361 | 127 | 1934 | 69.7 | 15 | 82 | 33 | 62 | 69 | **64.1** |
| 10 | uwebsockets | htmx | 52 | 396 | 185 | 3663 | 34.0 | 13 | 71 | 43 | 48 | 100 | **63.8** |
| 11 | fiber | vanilla | 36 | 360 | 126 | 1548 | 118.9 | 43 | 89 | 13 | 46 | 100 | **63.2** |
| 12 | gohttp | vanilla | 36 | 377 | 126 | 1508 | 127.3 | 61 | 88 | 10 | 52 | 100 | **63.2** |
| 13 | uwebsockets | alpine | 48 | 409 | 190 | 3275 | 36.6 | 17 | 75 | 34 | 46 | 100 | **62.7** |
| 14 | oatpp | vanilla | 40 | 376 | 127 | 2449 | 113.1 | 52 | 88 | 13 | 65 | 75 | **62.7** |
| 15 | actix | vanilla | 48 | 411 | 126 | 1849 | 94.0 | 50 | 85 | 13 | 49 | 100 | **62.2** |
| 16 | drogon | alpine | 52 | 408 | 190 | 3019 | 41.3 | 17 | 74 | 33 | 40 | 100 | **60.8** |
| 17 | axum | htmx | 48 | 394 | 185 | 7747 | 17.1 | 79 | 75 | 24 | 47 | 100 | **60.4** |
| 18 | oatpp | alpine | 48 | 392 | 190 | 3861 | 68.4 | 52 | 76 | 16 | 55 | 100 | **60.2** |
| 19 | drogon | htmx | 52 | 395 | 185 | 1647 | 70.5 | 16 | 71 | 31 | 42 | 100 | **59.8** |
| 20 | axum | alpine | 48 | 391 | 190 | 5699 | 23.3 | 80 | 78 | 19 | 40 | 100 | **59.0** |
| 21 | httplib | vanilla | 48 | 396 | 126 | 2019 | 135.0 | 60 | 84 | 11 | 62 | 67 | **58.8** |
| 22 | crow | htmx | 52 | 395 | 185 | 3309 | 29.5 | 37 | 72 | 21 | 47 | 100 | **58.4** |
| 23 | oatpp | htmx | 52 | 396 | 185 | 1866 | 155.2 | 50 | 71 | 12 | 55 | 100 | **57.5** |
| 24 | actix | htmx | 50 | 394 | 185 | 7615 | 35.3 | 56 | 75 | 23 | 41 | 90 | **57.3** |
| 25 | httplib | alpine | 50 | 395 | 190 | 3626 | 88.2 | 55 | 75 | 15 | 48 | 90 | **56.7** |
| 26 | httplib | htmx | 52 | 397 | 184 | 2344 | 109.0 | 60 | 71 | 12 | 51 | 100 | **56.3** |
| 27 | gohttp | alpine | 52 | 394 | 190 | 1233 | 160.9 | 48 | 73 | 12 | 40 | 100 | **55.3** |
| 28 | httpzig | htmx | 64 | 437 | 184 | 8269 | 28.7 | 21 | 71 | 37 | 38 | 67 | **55.2** |
| 29 | gohttp | htmx | 52 | 400 | 184 | 1254 | 157.0 | 51 | 70 | 11 | 42 | 100 | **54.2** |
| 30 | actix | alpine | 52 | 392 | 190 | 4590 | 47.4 | 56 | 77 | 17 | 33 | 81 | **54.1** |
| 31 | crow | alpine | 58 | 393 | 190 | 1643 | 57.4 | 37 | 73 | 16 | 49 | 67 | **53.4** |
| 32 | fiber | htmx | 52 | 396 | 184 | 1132 | 173.0 | 51 | 70 | 11 | 38 | 100 | **53.2** |
| 33 | fiber | alpine | 52 | 394 | 190 | 1295 | 151.5 | 46 | 73 | 12 | 34 | 90 | **52.6** |
| 34 | httplib | sveltekit | 40 | 393 | 248 | 1105 | 27.2 | 83 | 72 | 12 | 27 | 100 | **52.1** |
| 35 | crow | sveltekit | 40 | 376 | 248 | 1127 | 74.5 | 73 | 72 | 9 | 26 | 100 | **51.6** |
| 36 | drogon | sveltekit | 40 | 377 | 248 | 1096 | 72.5 | 85 | 73 | 8 | 27 | 100 | **51.4** |
| 37 | uwebsockets | sveltekit | 40 | 392 | 248 | 1128 | 94.2 | 64 | 71 | 10 | 26 | 100 | **51.2** |
| 38 | httplib | astro | 44 | 396 | 192 | 337 | 112.6 | 223 | 73 | 4 | 27 | 100 | **50.7** |
| 39 | gohttp | sveltekit | 40 | 393 | 248 | 1096 | 89.8 | 67 | 72 | 10 | 23 | 100 | **50.7** |
| 40 | zap | sveltekit | 40 | 393 | 248 | 1213 | 38.6 | 91 | 72 | 10 | 22 | 100 | **50.7** |
| 41 | drogon | astro | 44 | 396 | 192 | 336 | 225.2 | 331 | 74 | 2 | 26 | 100 | **50.4** |
| 42 | httpzig | astro | 44 | 394 | 192 | 347 | 216.2 | 337 | 74 | 2 | 24 | 100 | **50.1** |
| 43 | oatpp | sveltekit | 40 | 394 | 248 | 975 | 98.7 | 112 | 71 | 7 | 24 | 100 | **50.0** |
| 44 | actix | sveltekit | 40 | 393 | 248 | 1196 | 74.6 | 81 | 72 | 9 | 19 | 100 | **50.0** |
| 45 | oatpp | astro | 44 | 378 | 192 | 343 | 201.4 | 329 | 75 | 2 | 22 | 100 | **49.9** |
| 46 | zap | astro | 44 | 395 | 192 | 359 | 193.5 | 321 | 74 | 3 | 22 | 100 | **49.7** |
| 47 | gohttp | astro | 44 | 397 | 192 | 353 | 268.5 | 325 | 74 | 2 | 23 | 100 | **49.7** |
| 48 | axum | astro | 44 | 379 | 192 | 315 | 273.1 | 348 | 75 | 2 | 20 | 100 | **49.4** |
| 49 | fiber | astro | 44 | 379 | 192 | 265 | 332.3 | 331 | 75 | 2 | 20 | 100 | **49.3** |
| 50 | crow | astro | 46 | 396 | 192 | 252 | 327.8 | 329 | 73 | 2 | 26 | 89 | **48.5** |
| 51 | uwebsockets | astro | 48 | 427 | 192 | 297 | 273.8 | 354 | 72 | 2 | 27 | 79 | **46.7** |
| 52 | axum | sveltekit | 58 | 428 | 248 | 662 | 138.2 | 75 | 66 | 8 | 19 | 91 | **45.9** |
| 53 | actix | astro | 68 | 406 | 192 | 208 | 424.1 | 338 | 69 | 2 | 18 | 85 | **44.5** |
| 54 | httpzig | sveltekit | 52 | 394 | 248 | 758 | 204.9 | 60 | 69 | 9 | 24 | 67 | **44.5** |

## Parite dışı kalanlar

- fiber × sveltekit × kanban: `load-test error rate 11.9% > 1%`
