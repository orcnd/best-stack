//! Thin wrapper over the SQLite C API: one connection per thread, cached prepared statements.
const std = @import("std");
const c = @import("c");

pub const Error = error{SqliteError};

var db_path: [:0]const u8 = "data/app.sqlite";
pub fn setPath(p: [:0]const u8) void {
    db_path = p;
}

pub const Conn = struct {
    db: *c.sqlite3,
    cache: std.StringHashMapUnmanaged(*c.sqlite3_stmt) = .empty,

    pub fn prepare(self: *Conn, sql: []const u8) Error!Stmt {
        if (self.cache.get(sql)) |s| return .{ .s = s };
        var s: ?*c.sqlite3_stmt = null;
        if (c.sqlite3_prepare_v3(self.db, sql.ptr, @intCast(sql.len), c.SQLITE_PREPARE_PERSISTENT, &s, null) != c.SQLITE_OK) {
            std.debug.print("sqlite prepare: {s} :: {s}\n", .{ c.sqlite3_errmsg(self.db), sql });
            return error.SqliteError;
        }
        const key = std.heap.c_allocator.dupe(u8, sql) catch return error.SqliteError;
        self.cache.put(std.heap.c_allocator, key, s.?) catch return error.SqliteError;
        return .{ .s = s.? };
    }

    pub fn changes(self: *Conn) i64 {
        return c.sqlite3_changes64(self.db);
    }
    pub fn lastInsertRowid(self: *Conn) i64 {
        return c.sqlite3_last_insert_rowid(self.db);
    }
};

threadlocal var tl_conn: ?Conn = null;

/// The calling thread's connection (opened lazily with the shared pragmas).
pub fn conn() Error!*Conn {
    if (tl_conn == null) {
        var db: ?*c.sqlite3 = null;
        if (c.sqlite3_open_v2(db_path.ptr, &db, c.SQLITE_OPEN_READWRITE | c.SQLITE_OPEN_NOMUTEX, null) != c.SQLITE_OK) return error.SqliteError;
        _ = c.sqlite3_busy_timeout(db, 5000);
        _ = c.sqlite3_exec(db, "PRAGMA journal_mode = WAL; PRAGMA synchronous = NORMAL; PRAGMA temp_store = MEMORY; PRAGMA mmap_size = 268435456;", null, null, null);
        tl_conn = .{ .db = db.? };
    }
    return &tl_conn.?;
}

/// A cached statement; call `done()` (reset + clear bindings) when finished.
pub const Stmt = struct {
    s: *c.sqlite3_stmt,

    pub fn done(self: Stmt) void {
        _ = c.sqlite3_reset(self.s);
        _ = c.sqlite3_clear_bindings(self.s);
    }
    pub fn bindInt(self: Stmt, i: c_int, v: i64) void {
        _ = c.sqlite3_bind_int64(self.s, i, v);
    }
    /// SQLITE_STATIC (null destructor): callers bind request-arena memory and step before it is freed.
    /// (SQLITE_TRANSIENT is the pointer -1, which Zig's translate-c cannot represent as a fn pointer.)
    pub fn bindText(self: Stmt, i: c_int, v: []const u8) void {
        _ = c.sqlite3_bind_text(self.s, i, v.ptr, @intCast(v.len), null);
    }
    /// true = row available, false = done
    pub fn step(self: Stmt) Error!bool {
        const rc = c.sqlite3_step(self.s);
        if (rc == c.SQLITE_ROW) return true;
        if (rc == c.SQLITE_DONE) return false;
        std.debug.print("sqlite step: {s}\n", .{c.sqlite3_errmsg(c.sqlite3_db_handle(self.s))});
        return error.SqliteError;
    }
    pub fn int(self: Stmt, col: c_int) i64 {
        return c.sqlite3_column_int64(self.s, col);
    }
    pub fn float(self: Stmt, col: c_int) f64 {
        return c.sqlite3_column_double(self.s, col);
    }
    /// Copies the column text into `a` (the statement buffer is reused on the next step).
    pub fn text(self: Stmt, a: std.mem.Allocator, col: c_int) ![]const u8 {
        const p = c.sqlite3_column_text(self.s, col);
        if (p == null) return "";
        const n: usize = @intCast(c.sqlite3_column_bytes(self.s, col));
        return a.dupe(u8, p[0..n]);
    }
};
