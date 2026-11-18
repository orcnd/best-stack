//! best-stack Zig core: SQLite data layer, HTML rendering, framework-agnostic handlers.
pub const db = @import("db.zig");
pub const format = @import("format.zig");
pub const handlers = @import("handlers.zig");
pub const http = @import("http.zig");
pub const render = @import("render.zig");
pub const sqlite = @import("sqlite.zig");

pub const Req = http.Req;
pub const Resp = http.Resp;
pub const routes = handlers.routes;

/// Must be called once before serving.
pub fn init(cli: http.Cli) void {
    sqlite.setPath(cli.db);
    http.initStatic(cli.root);
}

test {
    _ = format;
}
