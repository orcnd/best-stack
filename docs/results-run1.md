# Best Stack — sonuç raporu

Run: `run-2026-11-18T07-53-53-993Z` · Apple M2, 8 cpu, 8 GB · macOS 26.7.1 · başlangıç 2026-11-18T07:53:53.995Z · bitiş 2026-11-18T11:16:46.408Z

Ağırlıklar: Kullanıcı hızı 40 · Sunucu 25 · DX 20 · Kararlılık 15. Her hücre 0–100; toplam ağırlıklı ortalama (/100).

## Genel sıralama (3 senaryo ortalaması)

| # | Backend | Frontend | Kullanıcı | Sunucu | DX | Kararlılık | **Toplam** |
|---|---|---|---|---|---|---|---|
| 1 | httplib | vanilla | 96 | 52 | 86 | 90 | **81.9** |
| 2 | drogon | vanilla | 97 | 56 | 68 | 87 | **79.4** |
| 3 | uwebsockets | vanilla | 74 | 75 | 79 | 86 | **77.0** |
| 4 | crow | vanilla | 77 | 74 | 67 | 82 | **75.0** |
| 5 | uwebsockets | alpine | 70 | 77 | 68 | 81 | **73.3** |
| 6 | httplib | htmx | 74 | 57 | 77 | 86 | **72.2** |
| 7 | crow | alpine | 70 | 79 | 67 | 73 | **72.1** |
| 8 | drogon | alpine | 78 | 64 | 50 | 89 | **70.6** |
| 9 | uwebsockets | htmx | 69 | 73 | 62 | 81 | **70.3** |
| 10 | httplib | alpine | 75 | 56 | 65 | 80 | **69.1** |
| 11 | crow | htmx | 69 | 68 | 60 | 83 | **69.0** |
| 12 | drogon | htmx | 76 | 62 | 55 | 80 | **68.8** |
| 13 | oatpp | vanilla | 74 | 49 | 71 | 78 | **67.5** |
| 14 | oatpp | alpine | 68 | 53 | 67 | 86 | **66.9** |
| 15 | oatpp | htmx | 66 | 48 | 66 | 87 | **64.9** |
| 16 | httplib | sveltekit | 68 | 43 | 36 | 91 | **58.8** |
| 17 | uwebsockets | sveltekit | 69 | 34 | 30 | 87 | **55.0** |
| 18 | oatpp | sveltekit | 69 | 30 | 31 | 87 | **54.4** |
| 19 | httplib | astro | 69 | 25 | 36 | 82 | **53.3** |
| 20 | crow | sveltekit | 69 | 34 | 30 | 74 | **53.2** |
| 21 | drogon | sveltekit | 68 | 34 | 31 | 76 | **53.2** |
| 22 | crow | astro | 68 | 21 | 30 | 85 | **51.4** |
| 23 | drogon | astro | 68 | 23 | 30 | 78 | **50.8** |
| 24 | oatpp | astro | 69 | 23 | 29 | 78 | **50.7** |
| 25 | uwebsockets | astro | 69 | 23 | 30 | 76 | **50.7** |

## Backend ortalamaları

| Backend | Toplam |
|---|---|
| httplib | 67.1 |
| uwebsockets | 65.3 |
| drogon | 64.6 |
| crow | 64.1 |
| oatpp | 60.9 |

## Frontend ortalamaları

| Frontend | Toplam |
|---|---|
| vanilla | 76.2 |
| alpine | 70.4 |
| htmx | 69.0 |
| sveltekit | 54.9 |
| astro | 51.4 |

## Senaryo: catalog

| # | Backend | Frontend | LCP ms (masaüstü) | Etkileşim ms (masaüstü) | KB | req/s @64 | p95 ms | RSS MB | Kullanıcı | Sunucu | DX | Kararlılık | **Toplam** |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | httplib | vanilla | 25 | 705 | 7 | 12527 | 4.5 | 33 | 95 | 68 | 84 | 83 | **84.2** |
| 2 | uwebsockets | vanilla | 24 | 704 | 20 | 14358 | 8.6 | 11 | 82 | 80 | 82 | 84 | **81.8** |
| 3 | drogon | vanilla | 24 | 704 | 7 | 11749 | 10.6 | 16 | 98 | 61 | 66 | 83 | **80.0** |
| 4 | crow | vanilla | 25 | 704 | 20 | 13562 | 7.3 | 12 | 81 | 78 | 73 | 77 | **77.7** |
| 5 | httplib | alpine | 24 | 668 | 24 | 13721 | 3.7 | 36 | 79 | 75 | 77 | 76 | **77.1** |
| 6 | crow | alpine | 25 | 667 | 63 | 13452 | 7.3 | 13 | 75 | 76 | 70 | 79 | **74.9** |
| 7 | uwebsockets | htmx | 25 | 700 | 66 | 11755 | 10.2 | 11 | 73 | 72 | 75 | 79 | **74.1** |
| 8 | crow | htmx | 25 | 727 | 66 | 13149 | 7.9 | 13 | 72 | 74 | 69 | 85 | **73.9** |
| 9 | httplib | htmx | 27 | 682 | 23 | 12213 | 5.3 | 32 | 76 | 63 | 76 | 77 | **73.0** |
| 10 | uwebsockets | alpine | 25 | 665 | 63 | 13919 | 9.1 | 11 | 75 | 79 | 61 | 68 | **72.3** |
| 11 | drogon | htmx | 25 | 699 | 21 | 10814 | 11.8 | 15 | 79 | 59 | 53 | 100 | **72.0** |
| 12 | oatpp | htmx | 25 | 701 | 66 | 13024 | 4.4 | 41 | 70 | 67 | 70 | 76 | **70.1** |
| 13 | oatpp | vanilla | 27 | 706 | 20 | 11495 | 5.6 | 38 | 77 | 58 | 81 | 50 | **68.8** |
| 14 | oatpp | alpine | 27 | 666 | 63 | 11156 | 6.6 | 40 | 71 | 54 | 66 | 84 | **67.6** |
| 15 | drogon | alpine | 25 | 699 | 23 | 10242 | 12.5 | 17 | 79 | 54 | 52 | 76 | **67.0** |
| 16 | httplib | sveltekit | 27 | 718 | 121 | 2871 | 16.8 | 126 | 66 | 17 | 37 | 100 | **53.2** |
| 17 | crow | sveltekit | 27 | 719 | 121 | 3011 | 26.5 | 96 | 66 | 15 | 28 | 90 | **49.4** |
| 18 | httplib | astro | 29 | 704 | 46 | 1648 | 42.1 | 153 | 67 | 9 | 35 | 81 | **48.2** |
| 19 | oatpp | sveltekit | 27 | 724 | 121 | 2787 | 27.9 | 146 | 66 | 13 | 30 | 81 | **47.8** |
| 20 | uwebsockets | sveltekit | 27 | 750 | 121 | 3088 | 26.8 | 93 | 65 | 16 | 28 | 80 | **47.4** |
| 21 | drogon | astro | 30 | 706 | 46 | 1702 | 43.1 | 145 | 67 | 9 | 31 | 80 | **47.2** |
| 22 | crow | astro | 30 | 722 | 46 | 1719 | 43.0 | 136 | 66 | 9 | 31 | 80 | **47.1** |
| 23 | drogon | sveltekit | 28 | 751 | 121 | 2727 | 29.4 | 101 | 63 | 14 | 30 | 81 | **47.0** |
| 24 | uwebsockets | astro | 33 | 701 | 46 | 1693 | 42.9 | 140 | 66 | 9 | 30 | 50 | **42.0** |
| 25 | oatpp | astro | 30 | 725 | 46 | 1478 | 63.6 | 149 | 66 | 8 | 29 | 50 | **41.6** |

## Senaryo: dashboard

| # | Backend | Frontend | LCP ms (masaüstü) | Etkileşim ms (masaüstü) | KB | req/s @64 | p95 ms | RSS MB | Kullanıcı | Sunucu | DX | Kararlılık | **Toplam** |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | drogon | vanilla | 56 | 252 | 5 | 148 | 905.2 | 58 | 99 | 63 | 67 | 90 | **82.1** |
| 2 | crow | alpine | 54 | 248 | 59 | 185 | 431.2 | 51 | 73 | 85 | 70 | 90 | **77.6** |
| 3 | httplib | vanilla | 56 | 249 | 5 | 139 | 1435.2 | 389 | 97 | 34 | 84 | 86 | **76.9** |
| 4 | uwebsockets | alpine | 52 | 249 | 59 | 176 | 660.4 | 56 | 73 | 73 | 72 | 87 | **74.9** |
| 5 | drogon | alpine | 56 | 249 | 21 | 176 | 637.4 | 58 | 79 | 72 | 59 | 91 | **74.9** |
| 6 | crow | vanilla | 54 | 249 | 14 | 105 | 786.7 | 50 | 82 | 62 | 67 | 87 | **74.7** |
| 7 | httplib | htmx | 56 | 249 | 22 | 202 | 429.8 | 395 | 78 | 59 | 74 | 90 | **74.3** |
| 8 | drogon | htmx | 56 | 269 | 20 | 186 | 619.1 | 58 | 78 | 74 | 54 | 90 | **73.9** |
| 9 | crow | htmx | 52 | 248 | 61 | 144 | 599.5 | 50 | 73 | 72 | 62 | 90 | **73.2** |
| 10 | httplib | alpine | 58 | 250 | 23 | 179 | 499.1 | 390 | 77 | 52 | 72 | 82 | **70.6** |
| 11 | uwebsockets | htmx | 56 | 265 | 61 | 177 | 653.2 | 54 | 71 | 74 | 56 | 79 | **69.8** |
| 12 | oatpp | vanilla | 56 | 321 | 14 | 138 | 1086.7 | 742 | 78 | 34 | 81 | 92 | **69.5** |
| 13 | oatpp | alpine | 56 | 250 | 59 | 187 | 503.7 | 705 | 71 | 52 | 67 | 84 | **67.4** |
| 14 | uwebsockets | vanilla | 62 | 486 | 15 | 78 | 1620.1 | 54 | 70 | 49 | 72 | 73 | **65.7** |
| 15 | httplib | sveltekit | 60 | 253 | 116 | 171 | 275.7 | 447 | 72 | 65 | 36 | 87 | **65.1** |
| 16 | httplib | astro | 60 | 249 | 45 | 191 | 490.9 | 503 | 75 | 54 | 38 | 88 | **64.0** |
| 17 | drogon | sveltekit | 56 | 250 | 116 | 190 | 611.0 | 126 | 73 | 60 | 31 | 92 | **64.0** |
| 18 | uwebsockets | sveltekit | 56 | 248 | 116 | 185 | 695.1 | 115 | 73 | 58 | 31 | 91 | **63.4** |
| 19 | oatpp | astro | 60 | 250 | 45 | 190 | 511.3 | 757 | 75 | 52 | 29 | 92 | **62.3** |
| 20 | oatpp | htmx | 56 | 339 | 61 | 99 | 917.5 | 719 | 68 | 29 | 68 | 90 | **61.5** |
| 21 | uwebsockets | astro | 60 | 250 | 45 | 155 | 764.3 | 142 | 75 | 49 | 30 | 87 | **61.4** |
| 22 | oatpp | sveltekit | 56 | 265 | 116 | 192 | 524.6 | 782 | 71 | 51 | 31 | 89 | **60.9** |
| 23 | crow | astro | 60 | 267 | 45 | 145 | 982.8 | 135 | 73 | 46 | 30 | 91 | **60.1** |
| 24 | crow | sveltekit | 56 | 248 | 116 | 162 | 538.7 | 110 | 73 | 59 | 31 | 50 | **57.5** |
| 25 | drogon | astro | 62 | 269 | 45 | 165 | 753.2 | 147 | 72 | 51 | 30 | 61 | **56.5** |

## Senaryo: kanban

| # | Backend | Frontend | LCP ms (masaüstü) | Etkileşim ms (masaüstü) | KB | req/s @64 | p95 ms | RSS MB | Kullanıcı | Sunucu | DX | Kararlılık | **Toplam** |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | httplib | vanilla | 36 | 356 | 8 | 3601 | 89.4 | 49 | 95 | 54 | 89 | 100 | **84.5** |
| 2 | uwebsockets | vanilla | 36 | 357 | 127 | 3719 | 32.9 | 17 | 70 | 95 | 84 | 100 | **83.4** |
| 3 | drogon | vanilla | 36 | 356 | 8 | 1166 | 126.4 | 21 | 95 | 43 | 71 | 89 | **76.1** |
| 4 | uwebsockets | alpine | 52 | 323 | 190 | 2646 | 43.8 | 16 | 63 | 80 | 72 | 88 | **72.8** |
| 5 | crow | vanilla | 36 | 355 | 127 | 3640 | 30.1 | 35 | 69 | 81 | 63 | 81 | **72.5** |
| 6 | drogon | alpine | 36 | 324 | 25 | 2411 | 48.3 | 24 | 77 | 65 | 40 | 100 | **69.9** |
| 7 | httplib | htmx | 52 | 392 | 26 | 3285 | 97.7 | 61 | 69 | 48 | 80 | 91 | **69.4** |
| 8 | uwebsockets | htmx | 56 | 394 | 185 | 2303 | 52.9 | 16 | 63 | 73 | 55 | 86 | **67.0** |
| 9 | oatpp | alpine | 52 | 324 | 190 | 3596 | 79.6 | 58 | 62 | 54 | 69 | 90 | **65.7** |
| 10 | oatpp | vanilla | 36 | 357 | 127 | 3612 | 82.3 | 55 | 67 | 54 | 51 | 91 | **64.3** |
| 11 | crow | alpine | 52 | 323 | 190 | 3455 | 31.7 | 40 | 62 | 76 | 61 | 50 | **63.7** |
| 12 | oatpp | htmx | 52 | 392 | 185 | 3275 | 91.8 | 70 | 62 | 48 | 62 | 94 | **63.0** |
| 13 | drogon | htmx | 44 | 392 | 24 | 1589 | 75.8 | 21 | 71 | 53 | 57 | 51 | **60.6** |
| 14 | crow | htmx | 52 | 392 | 185 | 2340 | 43.6 | 38 | 63 | 58 | 48 | 72 | **60.0** |
| 15 | httplib | alpine | 60 | 324 | 27 | 2804 | 126.2 | 60 | 70 | 42 | 45 | 81 | **59.8** |
| 16 | httplib | sveltekit | 42 | 342 | 248 | 1038 | 31.4 | 118 | 66 | 46 | 36 | 87 | **58.1** |
| 17 | oatpp | sveltekit | 40 | 305 | 248 | 1068 | 80.6 | 153 | 70 | 25 | 32 | 92 | **54.5** |
| 18 | uwebsockets | sveltekit | 40 | 304 | 248 | 1169 | 83.7 | 108 | 70 | 27 | 31 | 89 | **54.3** |
| 19 | crow | sveltekit | 40 | 305 | 248 | 1069 | 79.7 | 101 | 68 | 27 | 30 | 83 | **52.8** |
| 20 | drogon | astro | 44 | 292 | 192 | 355 | 196.6 | 425 | 66 | 10 | 30 | 93 | **48.7** |
| 21 | uwebsockets | astro | 44 | 293 | 192 | 366 | 192.4 | 446 | 66 | 10 | 30 | 93 | **48.6** |
| 22 | drogon | sveltekit | 40 | 306 | 248 | 1104 | 79.5 | 117 | 68 | 27 | 31 | 57 | **48.6** |
| 23 | oatpp | astro | 44 | 294 | 192 | 348 | 225.4 | 453 | 66 | 9 | 29 | 93 | **48.3** |
| 24 | httplib | astro | 44 | 308 | 192 | 333 | 112.8 | 312 | 65 | 14 | 36 | 75 | **47.7** |
| 25 | crow | astro | 44 | 292 | 192 | 348 | 209.4 | 422 | 65 | 9 | 30 | 85 | **47.1** |

## Parite dışı kalanlar

Yok — tüm kombinasyonlar fonksiyonel, görsel ve SEO kontrollerini geçti.

## Bilinen karıştırıcı etkenler (run 1)

Bu koşu olduğu gibi yayımlandı; aşağıdaki üç etken sıralamayı etkiliyor ve bir sonraki koşuda düzeltilmesi önerildi.

1. **Sıkıştırma eşit değil.** Drogon yanıtları varsayılan olarak gzip'liyor, cpp-httplib ise Homebrew `zstd` bulunduğu için zstd ile sıkıştırıyor (Chrome `zstd` kabul ediyor). Crow, uWebSockets, Oat++ ve Bun sunucuları sıkıştırmıyor. Etkisi: httplib/Drogon kombinasyonlarında aktarılan bayt ~15–25× düşük (ör. kanban/vanilla 8 KB vs 127 KB) → "bytes" ve mobil metriklerinde avantaj; Drogon'da büyük sayfalarda CPU maliyeti (kanban/vanilla 1.166 req/s vs ~3.600).
2. **RSS, mmap yüzünden şişik.** SQLite bağlantıları DB'yi `mmap` ile eşliyor; macOS `ps` RSS'i her eşlemeyi ayrı sayıyor. Bağlantı başına thread kullanan backend'lerde (httplib 64 thread, Oat++ bağlantı başına thread) dashboard RSS'i 390–780 MB görünüyor; gerçek fiziksel ayak izi çok daha düşük.
3. **Kararlılık puanı gürültüye duyarlı.** Varyans, masaüstü LCP'nin değişim katsayısından (CV) hesaplanıyor; ~25 ms'lik LCP'de tek bir sıçrama CV'yi %20'nin üstüne itip varyans puanını 0'a düşürebiliyor (tabloda Kararlılık = 50 görünen satırlar).

Ham veri: [`docs/data/results-run1.sqlite`](data/results-run1.sqlite) — `scoring/score.ts --db docs/data/results-run1.sqlite` ile farklı ağırlıklarla yeniden puanlanabilir.
