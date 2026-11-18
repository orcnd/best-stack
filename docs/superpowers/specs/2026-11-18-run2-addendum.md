# Run 2 ek tasarımı — ölçüm düzeltmeleri + Rust / Go / Zig backend'leri

Tarih: 2026-11-18 · Ana tasarım: `2026-11-18-best-stack-design.md` (geçerliliğini korur; bu doküman ekler/değiştirir)

## Kullanıcı kararları

- Run 1'de bulunan üç ölçüm sorunu düzeltilir.
- Backend karşılaştırmasına **Rust, Go, Zig** eklenir; dil başına **2 framework**: Rust **Axum + Actix-web**, Go **net/http + Fiber**, Zig **http.zig + zap**.
- Yeni backend'ler **native**: her dil kendi SQLite sürücüsü, şablon/HTML üretimi ve handler'larıyla yazılır (C++ çekirdeğine FFI yok).
- Matris: 11 backend × 5 frontend × 3 senaryo = **165 kombinasyon**.

## 1. Ölçüm düzeltmeleri

| Sorun (run 1) | Düzeltme |
|---|---|
| Sıkıştırma eşit değil (Drogon gzip, httplib zstd) | Uygulama katmanında sıkıştırma **her yerde kapalı** (Drogon `enableGzip(false)`/brotli kapalı, httplib zstd/zlib/brotli olmadan). Sözleşme testi: `Accept-Encoding: gzip, deflate, br, zstd` ile hiçbir yanıtta `Content-Encoding` yok. Gerekçe: üretimde sıkıştırma tipik olarak reverse proxy'nin işi; framework farkını ölçmek için eşit olmalı. |
| RSS mmap yüzünden şişik | Örnekleyici macOS **phys_footprint** değerini `proc_pid_rusage` ile okuyan küçük bir C aracıyla (`bs-footprint`) alır; `resources.footprint_kb` sütunu eklenir. Puanlama bellek için `footprint_kb` kullanır (`rss_kb` ham veride kalır). |
| Kararlılık puanı tek sıçramaya duyarlı | Varyans **robust CV** ile: `1.4826 × MAD / medyan × 100`; puan `max(0, 100 − 5 × robustCV)`. |

## 2. Yeni backend'lerin yapısı (builder kararları)

Her dilde iki framework **o dilin ortak çekirdeğini** paylaşır (C++'taki ilkeyle aynı: aynı dil içinde yalnızca HTTP katmanı farklı).

| Dil | Çekirdek | SQLite | HTML | Framework A | Framework B |
|---|---|---|---|---|---|
| Rust | `backends/rust/core` crate | rusqlite (bundled amalgamation, r2d2 havuzu) | minijinja — **aynı Jinja şablon dosyaları** (`shared/cpp/templates`) | Axum (tokio, `spawn_blocking`) | Actix-web (`web::block`) |
| Go | `backends/go/core` paketi | mattn/go-sqlite3 (cgo) + `database/sql` havuzu | `html/template` (idiomatik Go; şablonlar Go'ya port edilir) | net/http (Go 1.22+ desen yönlendirme) | Fiber v2 |
| Zig | `backends/zig/core` modülü | SQLite amalgamation `@cImport` ile | elle yazılmış `writer.print` render fonksiyonları (Zig'de olgun şablon motoru yok) | http.zig | zap |

- Ortak C++ şablonları Jinja uyumlu alt kümeye çekilir (`length(x)` yerine view-model'de bool), böylece minijinja aynı dosyaları okur.
- SQLite ayarları tüm dillerde aynı: WAL, `busy_timeout=5000`, `synchronous=NORMAL`, `temp_store=MEMORY`, `mmap_size=256MB`.
- Her backend aynı CLI'yi (`--port --db --root --threads`), route tablosunu, JSON şekillerini ve markup'ı sağlar; **kabul kapısı** C++'takiyle aynı: `bench/contract` + `check-backend.sh` (fonksiyonel, görsel, SEO).
- Çıktı binary'leri `build/backends/<name>/bs-<name>` yoluna konur (harness değişmeden çalıştırır).
- Backend adları: `axum, actix, gohttp, fiber, httpzig, zap`.

## 3. DX atfı (değişiklik)

Backend LOC = framework adaptörü + **o dilin ortak çekirdeği** (şablon/HTML dosyaları hariç, kod satırı). Böylece diller arası karşılaştırmada çekirdeğin dildeki karmaşıklığı da görünür. C++ backend'leri için çekirdek `shared/cpp` (src+include). Bağımlılık sayısı `deps.txt`; build süresi dilin kendi aracıyla temiz derleme (cmake/ninja, `cargo build --release` sıfırdan hedef dizinle, `go build -a`, `zig build -Doptimize=ReleaseFast` temiz önbellekle).

## 4. Görev listesi

1. Ölçüm düzeltmeleri (sıkıştırma + test, footprint aracı + şema, robust varyans + test).
2. Şablonları Jinja-uyumlu alt kümeye çekme (C++ testleri yeşil kalır).
3. Rust çekirdek + Axum + Actix → contract + parite yeşil.
4. Go çekirdek + net/http + Fiber → contract + parite yeşil.
5. Zig çekirdek + http.zig + zap → contract + parite yeşil.
6. Harness/DX çok dilli hale + run 2 (165 kombinasyon) + rapor + inceleme + `main`'e birleştirme.
