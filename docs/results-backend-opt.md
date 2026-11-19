# Best Stack — optimize backend'ler (run 3) ve dashboard darboğazı (run 4)

Soru: C++, Rust ve Go, en popüler iki "darboğaz kaldıran" kütüphaneyle Zig'e ne kadar yaklaşır? Her dilin en hızlı iki yolu hangisi?
Apple M2 (8 çekirdek, 8 GB), macOS 26.7.1, prizde, Düşük Güç Modu kapalı, 2026-11-19. Yalnız sunucu ölçümü (htmx; sayfayı backend üretir), 64 eşzamanlı istemci.

| Dil | Framework'ler (aynı) | Optimizasyon kütüphaneleri |
|---|---|---|
| C++ | Crow, uWebSockets | **glaze** (struct ⇄ JSON, nlohmann yerine) + **mimalloc**; HTML `std::format_to` ile doğrudan tampona |
| Rust | Axum, Actix-web | **askama** (derleme zamanı şablon) + **mimalloc** |
| Go | net/http, Fiber | **templ** (derlenmiş şablon) + **goccy/go-json** |
| Zig | http.zig, zap | değişmedi (zaten HTML'i doğrudan yazıyor) |

Eşdeğerlik kapısı: her optimize backend orijinaliyle 152/152 aynı çıktı (golden), `check-backend` (sözleşme + fonksiyonel/görsel/SEO) PASS. Yük testlerinde 0 hata.

## Özet

- **Optimize C++ ve Rust, Zig'i yakaladı.** Üç senaryonun geometrik ortalamasında http.zig 16.669, Crow (opt) 15.922 (-%4), Actix (opt) 14.593, Axum (opt) 13.929, uWebSockets (opt) 13.863, zap 13.459 req/s. Fark artık ölçüm gürültüsü mertebesinde.
- **Kazancın kaynağı şablon/görünüm-modeli katmanı.** Optimize C++/Rust'ta HTML sayfası JSON API kadar hızlı (HTML/API ≈ %100–110); orijinallerde %47–68 idi. En büyük sıçrama ağır şablonlu Kanban'da: Crow 15×, uWebSockets 7,6×.
- **Go 2–3× hızlandı ama hâlâ yarı hızda** (Fiber opt 7.124, net/http opt 6.326). templ + go-json HTML'deki farkı kapattı (net/http %26 → %63), ama Go'nun API ucu da diğerlerinden yavaş.
- **Dashboard'daki SQLite darboğazı kaldırıldı: ~160 → ~4.000–6.700 req/s (~25–40×)**, tüm backend'lerde, aynı çıktıyla (bkz. run 4).
- Bellek: Zig ve C++ 8–12 MB; Rust (opt) mimalloc ile 88–229 MB (yük altında tepe); Go 20–24 MB.

## Her dilin en hızlı iki yolu

| Dil | 1. | 2. |
|---|---|---|
| Zig | http.zig — 16.669 | zap — 13.459 |
| C++ | Crow + glaze + mimalloc — 15.922 | uWebSockets + glaze + mimalloc — 13.863 |
| Rust | Actix-web + askama + mimalloc — 14.593 | Axum + askama + mimalloc — 13.929 |
| Go | Fiber + templ + go-json — 7.124 | net/http + templ + go-json — 6.326 |

(Katalog, Kanban ve Dashboard sayfa req/s geometrik ortalaması. Dashboard değerleri run 4b'den — yeni şemayla.)

## Run 4: dashboard darboğazı

Run 3'te 14 backend'in hepsi `/dashboard`'da ~130–170 req/s'de toplandı: dört sorgu 30 günlük penceredeki 16.809 olayı her istekte dört kez tarıyordu (~40 ms SQLite CPU/istek). Hep SQLite, önbellek yok, birebir aynı çıktı kuralıyla:

| Aşama | Değişiklik | Sorgu süresi / istek | Dashboard sayfa req/s |
|---|---|---|---|
| Run 3 | Ham `events` taraması | ~40 ms | ~130–170 |
| Run 4a | Trigger ile güncellenen günlük özet tablolar (`ev_day`, `ev_country_day`, `ev_item_day`, `ev_user_day`), WITHOUT ROWID + gün-önce anahtar (kapsayan kümelenmiş indeks) | ~8 ms | ~650–870 (hepsi aynı bantta; darboğaz `count(DISTINCT user_id)`) |
| Run 4b | `ev_user_day` yerine `ev_user(user_id, last_day, last_purchase_day)` + indeks: "son N gün" penceresi hep bugünde bittiği için aktif kullanıcı = `last_day >= başlangıç` sayımı | ~1 ms | **~4.000–6.700** |

- Doğrulama: seed testi her özet satırını ham `GROUP BY` ile, tekil kullanıcı sayılarını 7/13/30/90 gün için ham `count(DISTINCT)` ile karşılaştırıyor; eski/yeni binary golden 152/152 (4 dil); tüm birim testleri ve 14 backend için `check-backend` PASS.
- Varsayım: `events` yalnız ekleniyor (hiçbir uygulama güncellemiyor/silmiyor) ve pencere hep "şimdi"de bitiyor. Bunlar değişirse `ev_user` yaklaşımı geçerli olmaz.
- Reddedilenler: hazır hesaplanmış KPI (cevabı önbelleklemek olur), DuckDB ("yalnız SQLite" kuralı + 4 dilde yeni bağımlılık).
- Şimdi kalan: backend süreçleri ortalama %420–490 CPU kullanıyor (8 çekirdeğin geri kalanını yük üreticisi alıyor); sorgu başına ~1 ms SQLite hâlâ en büyük kalem (en pahalısı top-10 ürün). Dil farkı artık görünüyor ama dar: C++/Rust/Zig ~5.300–6.700, Go ~4.000–5.000.

## Ölçüm notları

- Her hücre turların **en iyisi**: run 3 için 2 tam tur + 4 kombinasyonluk tekrar, run 4b için 2 tur. Arka plan yükü ölçümü yalnız yavaşlatabildiğinden en iyi değer gerçeğe en yakın olanı. Turlar arası fark bazı hücrelerde büyük (ikinci turda sistem yükü 15–18'di; örn. Fiber katalog 4.375 → 31). Tek tur sonuçlarına güvenmeyin.
- İlk run 3 denemesi pilde ve Düşük Güç Modu'nda yapıldı, geçersiz sayıldı (`results/run3-throttled.sqlite`, yayımlanmadı). Ölçüm aracı artık pilde/Düşük Güç Modu'nda çalışmayı reddediyor (`--allow-throttled` ile geçilir).
- Run 2 ile mutlak sayılar doğrudan karşılaştırılmamalı (farklı gün/makine durumu); karşılaştırma aynı oturumdaki orijinallerle.
- Go: templ, paylaşılan Go dolaylı bağımlılıklarını yükseltti (brotli 1.3.0, x/sys 0.50, go-isatty 0.0.24, go-colorable 0.1.16); orijinaller de bunlarla yeniden derlendi.
- Optimize binary'ler kullanılmayan minijinja/nlohmann başlıklarını hâlâ bağlıyor (istek yolunda çağrılmıyor). Crow-opt aynı `main.cpp`'yi kullandığı için açılış logu "bs-crow listening" der.

Ham veri: `docs/data/results-run3-p1.sqlite`, `-p2`, `-recheck`; `results-run4a-dashboard-p{1,2}.sqlite`; `results-run4b-dashboard-p{1,2}.sqlite`.
Tablolar: `python3 scripts/run3_report.py docs/data/results-run3-p1.sqlite docs/data/results-run3-p2.sqlite docs/data/results-run3-recheck.sqlite docs/data/results-run4b-dashboard-p1.sqlite docs/data/results-run4b-dashboard-p2.sqlite`
Tasarım: [run 3](superpowers/specs/2026-11-19-run3-optimized-backends.md), [run 4](superpowers/specs/2026-11-19-run4-dashboard-rollups.md).

# Tablolar (run 3 + run 4b dashboard, turların en iyisi)


## Sayfa (HTML) req/s, 64 eşzamanlı istemci

| Backend | Dil | Katalog | Kanban | Dashboard | Katalog API | HTML/API |
|---|---|---|---|---|---|---|
| http.zig | Zig | 38.992 | 18.905 | 6.283 | 35.490 | %110 |
| Crow (opt) | C++ | 33.109 | 20.332 | 5.996 | 32.402 | %102 |
| Actix-web (opt) | Rust | 32.229 | 16.009 | 6.023 | 30.811 | %105 |
| zap | Zig | 31.534 | 14.671 | 5.270 | 30.826 | %102 |
| Axum (opt) | Rust | 30.225 | 14.717 | 6.076 | 30.366 | %100 |
| Actix-web | Rust | 18.187 | 5.811 | 5.840 | 26.866 | %68 |
| uWebSockets (opt) | C++ | 17.669 | 22.466 | 6.711 | 27.828 | %63 |
| Fiber (opt) | Go | 15.506 | 4.708 | 4.953 | 19.794 | %78 |
| Axum | Rust | 13.285 | 6.399 | 5.785 | 28.158 | %47 |
| uWebSockets | C++ | 12.539 | 2.957 | 5.349 | 23.923 | %52 |
| Crow | C++ | 11.187 | 1.346 | 5.235 | 21.318 | %52 |
| net/http (opt) | Go | 9.972 | 5.141 | 4.939 | 15.850 | %63 |
| Fiber | Go | 4.375 | 920 | 4.251 | 12.514 | %35 |
| net/http | Go | 4.367 | 1.045 | 4.074 | 16.753 | %26 |

## Optimizasyon kazancı (opt / orijinal, sayfa req/s @64)

| Framework | Kütüphaneler | Katalog | Kanban | Dashboard | Katalog API |
|---|---|---|---|---|---|
| Actix-web | askama + mimalloc | 1,77× | 2,75× | 1,03× | 1,15× |
| Axum | askama + mimalloc | 2,28× | 2,30× | 1,05× | 1,08× |
| Crow | glaze + mimalloc | 2,96× | 15,10× | 1,15× | 1,52× |
| Fiber | templ + go-json | 3,54× | 5,12× | 1,17× | 1,58× |
| net/http | templ + go-json | 2,28× | 4,92× | 1,21× | 0,95× |
| uWebSockets | glaze + mimalloc | 1,41× | 7,60× | 1,25× | 1,16× |

## Gecikme ve kaynak (Katalog sayfası)

| Backend | p50 @1 ms | p95 @64 ms | Bellek MB (yük altında, tepe) | CPU % (ortalama) | req/s başına CPU-µs |
|---|---|---|---|---|---|
| Crow (opt) | 0,12 | 3,21 | 11,6 | 391 | 118,0 |
| http.zig | 0,12 | 7,96 | 8,0 | 362 | 92,9 |
| zap | 0,13 | 2,66 | 68,7 | 413 | 131,0 |
| Actix-web (opt) | 0,13 | 3,89 | 87,7 | 411 | 127,6 |
| Axum (opt) | 0,13 | 5,51 | 228,8 | 349 | 115,5 |
| Fiber (opt) | 0,17 | 9,92 | 20,4 | 414 | 267,2 |
| net/http (opt) | 0,21 | 15,22 | 23,7 | 343 | 344,4 |
| Actix-web | 0,21 | 11,77 | 37,5 | 449 | 246,8 |
| uWebSockets (opt) | 0,26 | 11,21 | 9,2 | 259 | 146,7 |
| Axum | 0,32 | 10,12 | 50,8 | 383 | 288,0 |
| uWebSockets | 0,38 | 9,67 | 10,7 | 500 | 398,6 |
| Crow | 0,38 | 7,95 | 15,0 | 449 | 401,1 |
| Fiber | 0,51 | 47,35 | 33,5 | 420 | 959,2 |
| net/http | 0,57 | 48,76 | 30,5 | 413 | 945,3 |

## Her dilin en hızlı iki yolu (üç senaryonun sayfa req/s @64 geometrik ortalaması)

| Dil | 1. | 2. |
|---|---|---|
| Zig | http.zig — 16.669 | zap — 13.459 |
| C++ | Crow (opt) — 15.922 | uWebSockets (opt) — 13.863 |
| Rust | Actix-web (opt) — 14.593 | Axum (opt) — 13.929 |
| Go | Fiber (opt) — 7.124 | net/http (opt) — 6.326 |

| Sıra | Backend | Geo. ort. sayfa req/s |
|---|---|---|
| 1 | http.zig | 16.669 |
| 2 | Crow (opt) | 15.922 |
| 3 | Actix-web (opt) | 14.593 |
| 4 | Axum (opt) | 13.929 |
| 5 | uWebSockets (opt) | 13.863 |
| 6 | zap | 13.459 |
| 7 | Actix-web | 8.514 |
| 8 | Axum | 7.894 |
| 9 | Fiber (opt) | 7.124 |
| 10 | net/http (opt) | 6.326 |
| 11 | uWebSockets | 5.832 |
| 12 | Crow | 4.288 |
| 13 | net/http | 2.649 |
| 14 | Fiber | 2.576 |

## Hatalar ve eşdeğerlik

- Yük testi hatası olan ölçümler: 0
- Eşdeğerlik (fonksiyonel/görsel/SEO) başarısız kombinasyon: 0 
