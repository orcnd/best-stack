const std = @import("std");

/// Same SQLite options as the C++, Rust and Go backends.
const sqlite_flags = [_][]const u8{
    "-DSQLITE_THREADSAFE=2", "-DSQLITE_DEFAULT_MEMSTATUS=0", "-DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1", "-DSQLITE_DQS=0",
    "-DSQLITE_LIKE_DOESNT_MATCH_BLOBS", "-DSQLITE_OMIT_DEPRECATED", "-DSQLITE_OMIT_SHARED_CACHE", "-DSQLITE_USE_ALLOCA",
};

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const sqlite = b.dependency("sqlite", .{});
    const c_mod = b.addTranslateC(.{ .root_source_file = b.path("core/c.h"), .target = target, .optimize = optimize });
    c_mod.addIncludePath(sqlite.path("."));

    const core = b.createModule(.{ .root_source_file = b.path("core/root.zig"), .target = target, .optimize = optimize, .link_libc = true });
    core.addImport("c", c_mod.createModule());
    core.addCSourceFile(.{ .file = sqlite.path("sqlite3.c"), .flags = &sqlite_flags });

    const httpz = b.dependency("httpz", .{ .target = target, .optimize = optimize });
    const zap = b.dependency("zap", .{ .target = target, .optimize = optimize, .openssl = false });

    const exe_h = b.addExecutable(.{ .name = "bs-httpzig", .root_module = b.createModule(.{
        .root_source_file = b.path("httpzig/main.zig"), .target = target, .optimize = optimize,
        .imports = &.{ .{ .name = "httpz", .module = httpz.module("httpz") }, .{ .name = "core", .module = core } },
    }) });
    b.installArtifact(exe_h);

    const exe_z = b.addExecutable(.{ .name = "bs-zap", .root_module = b.createModule(.{
        .root_source_file = b.path("zap/main.zig"), .target = target, .optimize = optimize,
        .imports = &.{ .{ .name = "zap", .module = zap.module("zap") }, .{ .name = "core", .module = core } },
    }) });
    b.installArtifact(exe_z);

    const tests = b.addTest(.{ .root_module = core });
    b.step("test", "core unit tests").dependOn(&b.addRunArtifact(tests).step);
}
