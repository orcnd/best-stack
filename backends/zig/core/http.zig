//! Framework-agnostic request/response, parsing helpers, CLI, static files.
const std = @import("std");
const c = @import("c");
const A = std.mem.Allocator;

pub const KV = std.StringHashMapUnmanaged([]const u8);
pub const Header = struct { name: []const u8, value: []const u8 };

pub const Req = struct {
    a: A,
    method: []const u8,
    path: []const u8,
    params: KV = .empty,
    query: KV = .empty,
    form: KV = .empty,
    headers: KV = .empty, // lower-case keys
    cookies: KV = .empty,
    body: []const u8 = "",

    pub fn header(r: *const Req, k: []const u8) []const u8 {
        return r.headers.get(k) orelse "";
    }
    pub fn param(r: *const Req, k: []const u8) []const u8 {
        return r.params.get(k) orelse "";
    }
    /// Adapters call this after setting method, path, params, headers and body.
    pub fn finish(r: *Req, raw_query: []const u8) !void {
        r.query = try parseUrlencoded(r.a, raw_query);
        var it = std.mem.splitScalar(u8, r.header("cookie"), ';');
        while (it.next()) |part| {
            const p = std.mem.trim(u8, part, " ");
            if (std.mem.indexOfScalar(u8, p, '=')) |eq| try r.cookies.put(r.a, p[0..eq], p[eq + 1 ..]);
        }
        if (std.mem.startsWith(u8, r.header("content-type"), "application/x-www-form-urlencoded"))
            r.form = try parseUrlencoded(r.a, r.body);
    }
};

pub const Resp = struct {
    status: u16 = 200,
    content_type: []const u8 = "text/html; charset=utf-8",
    body: []const u8 = "",
    headers: []const Header = &.{},
};

pub fn get(m: *const KV, k: []const u8) []const u8 {
    return m.get(k) orelse "";
}

pub fn urlDecode(a: A, s: []const u8) ![]const u8 {
    var out: std.ArrayList(u8) = .empty;
    var i: usize = 0;
    while (i < s.len) : (i += 1) {
        if (s[i] == '+') {
            try out.append(a, ' ');
        } else if (s[i] == '%' and i + 2 < s.len) {
            if (std.fmt.parseInt(u8, s[i + 1 .. i + 3], 16)) |v| {
                try out.append(a, v);
                i += 2;
            } else |_| try out.append(a, '%');
        } else try out.append(a, s[i]);
    }
    return out.toOwnedSlice(a);
}

pub fn parseUrlencoded(a: A, s: []const u8) !KV {
    var m: KV = .empty;
    var it = std.mem.splitScalar(u8, s, '&');
    while (it.next()) |part| {
        if (part.len == 0) continue;
        if (std.mem.indexOfScalar(u8, part, '=')) |eq| {
            try m.put(a, try urlDecode(a, part[0..eq]), try urlDecode(a, part[eq + 1 ..]));
        } else try m.put(a, try urlDecode(a, part), "");
    }
    return m;
}

/// Route-pattern match: "{x}" matches one non-empty segment.
pub fn match(a: A, pattern: []const u8, path: []const u8, params: *KV) !bool {
    var pi = std.mem.splitScalar(u8, pattern, '/');
    var si = std.mem.splitScalar(u8, path, '/');
    while (true) {
        const p = pi.next();
        const s = si.next();
        if (p == null and s == null) return true;
        if (p == null or s == null) return false;
        if (p.?.len > 2 and p.?[0] == '{') {
            if (s.?.len == 0) return false;
            try params.put(a, p.?[1 .. p.?.len - 1], s.?);
        } else if (!std.mem.eql(u8, p.?, s.?)) return false;
    }
}

pub const Cli = struct { port: u16 = 18080, db: [:0]const u8 = "data/app.sqlite", root: []const u8 = ".", threads: u16 = 8 };

pub fn parseCli(args: []const [:0]const u8) Cli {
    var cli: Cli = .{};
    var i: usize = 1;
    while (i + 1 < args.len) : (i += 2) {
        const k = args[i];
        const v = args[i + 1];
        if (std.mem.eql(u8, k, "--port")) cli.port = std.fmt.parseInt(u16, v, 10) catch 18080;
        if (std.mem.eql(u8, k, "--db")) cli.db = v;
        if (std.mem.eql(u8, k, "--root")) cli.root = v;
        if (std.mem.eql(u8, k, "--threads")) cli.threads = std.fmt.parseInt(u16, v, 10) catch 8;
    }
    return cli;
}

// ---------- static files (read once with C stdio, cached) ----------
var static_dir: []const u8 = "shared/static";
var static_mu: std.c.pthread_mutex_t = .{}; // std.Thread.Mutex is gone in Zig 0.17
var static_cache: std.StringHashMapUnmanaged(Resp) = .empty;

pub fn initStatic(root: []const u8) void {
    static_dir = std.fmt.allocPrint(std.heap.c_allocator, "{s}/shared/static", .{root}) catch unreachable;
}

fn mime(rel: []const u8) []const u8 {
    const ext = std.fs.path.extension(rel);
    const table = [_][2][]const u8{
        .{ ".css", "text/css; charset=utf-8" },    .{ ".js", "text/javascript; charset=utf-8" }, .{ ".mjs", "text/javascript; charset=utf-8" },
        .{ ".svg", "image/svg+xml" },              .{ ".png", "image/png" },                     .{ ".webp", "image/webp" },
        .{ ".wasm", "application/wasm" },          .{ ".json", "application/json" },             .{ ".html", "text/html; charset=utf-8" },
        .{ ".ico", "image/x-icon" },
    };
    for (table) |t| if (std.mem.eql(u8, ext, t[0])) return t[1];
    return "application/octet-stream";
}

pub fn serveStatic(rel: []const u8) ?Resp {
    _ = std.c.pthread_mutex_lock(&static_mu);
    defer _ = std.c.pthread_mutex_unlock(&static_mu);
    if (static_cache.get(rel)) |r| return r;
    if (rel.len == 0 or rel[0] == '/' or std.mem.indexOf(u8, rel, "..") != null) return null;
    const ca = std.heap.c_allocator;
    const full = std.fmt.allocPrintSentinel(ca, "{s}/{s}", .{ static_dir, rel }, 0) catch return null;
    const f = c.fopen(full.ptr, "rb") orelse return null;
    defer _ = c.fclose(f);
    if (c.fseek(f, 0, c.SEEK_END) != 0) return null;
    const size = c.ftell(f);
    if (size < 0) return null;
    _ = c.fseek(f, 0, c.SEEK_SET);
    const buf = ca.alloc(u8, @intCast(size)) catch return null;
    if (c.fread(buf.ptr, 1, buf.len, f) != buf.len) return null;
    const hdrs = ca.alloc(Header, 1) catch return null;
    hdrs[0] = .{ .name = "Cache-Control", .value = "public, max-age=31536000" };
    const r: Resp = .{ .content_type = mime(rel), .body = buf, .headers = hdrs };
    static_cache.put(ca, ca.dupe(u8, rel) catch return null, r) catch return null;
    return r;
}

pub fn randomHex16(out: *[16]u8) void {
    var b: [8]u8 = undefined;
    c.arc4random_buf(&b, b.len);
    const hex = "0123456789abcdef";
    for (b, 0..) |x, i| {
        out[i * 2] = hex[x >> 4];
        out[i * 2 + 1] = hex[x & 15];
    }
}
