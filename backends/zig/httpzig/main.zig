//! http.zig backend: 1 event-loop worker + a handler thread pool of --threads; routes registered
//! natively (":param" syntax) from core.routes at comptime.
const std = @import("std");
const httpz = @import("httpz");
const core = @import("core");

fn toHttpz(comptime pattern: []const u8) []const u8 {
    comptime {
        var out: []const u8 = "";
        var i: usize = 0;
        while (i < pattern.len) : (i += 1) {
            if (pattern[i] == '{') {
                out = out ++ ":";
            } else if (pattern[i] != '}') out = out ++ pattern[i .. i + 1];
        }
        return out;
    }
}

fn paramNames(comptime pattern: []const u8) []const []const u8 {
    comptime {
        var names: []const []const u8 = &.{};
        var it = std.mem.splitScalar(u8, pattern, '/');
        while (it.next()) |seg| if (seg.len > 2 and seg[0] == '{') {
            names = names ++ [_][]const u8{seg[1 .. seg.len - 1]};
        };
        return names;
    }
}

fn write(res: *httpz.Response, out: core.Resp) void {
    res.status = out.status;
    res.header("Content-Type", out.content_type);
    for (out.headers) |h| res.header(h.name, h.value);
    res.body = out.body;
}

fn wrap(comptime rt: core.handlers.Route) httpz.Action(void) {
    return struct {
        fn handle(req: *httpz.Request, res: *httpz.Response) !void {
            var r: core.Req = .{ .a = req.arena, .method = @tagName(req.method), .path = req.url.path, .body = req.body() orelse "" };
            inline for (comptime paramNames(rt.pattern)) |n| try r.params.put(r.a, n, req.param(n) orelse "");
            for (req.headers.keys[0..req.headers.len], req.headers.values[0..req.headers.len]) |k, v| try r.headers.put(r.a, k, v);
            try r.finish(req.url.query);
            write(res, try rt.handler(&r));
        }
    }.handle;
}

fn static(req: *httpz.Request, res: *httpz.Response) !void {
    const rel = try core.http.urlDecode(req.arena, req.url.path["/static/".len..]);
    write(res, core.http.serveStatic(rel) orelse .{ .status = 404, .content_type = "text/plain", .body = "not found" });
}

fn notFound(_: *httpz.Request, res: *httpz.Response) !void {
    write(res, .{ .status = 404, .content_type = "text/plain", .body = "not found" });
}

pub fn main(init: std.process.Init) !void {
    const args = try init.minimal.args.toSlice(init.arena.allocator());
    const cli = core.http.parseCli(args);
    core.init(cli);
    var server = try httpz.Server(void).init(init.io, init.gpa, .{
        .address = .all(cli.port),
        .workers = .{ .count = 1 },
        .thread_pool = .{ .count = cli.threads },
    }, {});
    var router = try server.router(.{});
    inline for (core.routes) |rt| {
        const path = comptime toHttpz(rt.pattern);
        const action = comptime wrap(rt);
        if (comptime std.mem.eql(u8, rt.method, "GET")) router.get(path, action, .{});
        if (comptime std.mem.eql(u8, rt.method, "POST")) router.post(path, action, .{});
        if (comptime std.mem.eql(u8, rt.method, "PATCH")) router.patch(path, action, .{});
        if (comptime std.mem.eql(u8, rt.method, "DELETE")) router.delete(path, action, .{});
    }
    router.get("/static/*", static, .{});
    router.all("/*", notFound, .{});
    std.debug.print("bs-httpzig listening on :{d} (threads={d})\n", .{ cli.port, cli.threads });
    try server.listen();
}
