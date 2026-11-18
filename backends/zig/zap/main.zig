//! zap (facil.io) backend: --threads facil.io threads, 1 worker process; one on_request callback
//! dispatches with the core's route table (zap's idiomatic single-handler style).
const std = @import("std");
const zap = @import("zap");
const core = @import("core");

const used_headers = [_][]const u8{ "cookie", "content-type", "hx-request", "referer", "x-session-id" };

fn onRequest(zr: zap.Request) !void {
    var arena = std.heap.ArenaAllocator.init(std.heap.c_allocator);
    defer arena.deinit();
    const a = arena.allocator();
    var r: core.Req = .{ .a = a, .method = zr.method orelse "GET", .path = zr.path orelse "/", .body = zr.body orelse "" };
    for (used_headers) |h| if (zr.getHeader(h)) |v| try r.headers.put(a, h, v);
    try r.finish(zr.query orelse "");
    const out = core.handlers.dispatch(&r) catch |e| blk: {
        std.debug.print("handler error: {s}\n", .{@errorName(e)});
        break :blk core.Resp{ .status = 500, .content_type = "text/plain", .body = "internal error" };
    };
    zr.setStatusNumeric(out.status);
    try zr.setHeader("Content-Type", out.content_type);
    for (out.headers) |h| try zr.setHeader(h.name, h.value);
    try zr.sendBody(out.body);
}

pub fn main(init: std.process.Init) !void {
    const args = try init.minimal.args.toSlice(init.arena.allocator());
    const cli = core.http.parseCli(args);
    core.init(cli);
    var listener = zap.HttpListener.init(.{ .port = cli.port, .on_request = onRequest, .log = false, .max_clients = 4096 });
    try listener.listen();
    std.debug.print("bs-zap listening on :{d} (threads={d})\n", .{ cli.port, cli.threads });
    zap.start(.{ .threads = @intCast(cli.threads), .workers = 1 });
}
