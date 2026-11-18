// best-stack: trimmed build script for zap 0.10.6 (zig-master branch, MIT) vendored for Zig 0.17.
// Only the library module is built; upstream examples/tests/docs are omitted.
const std = @import("std");
const build_facilio = @import("facil.io/build.zig").build_facilio;

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const use_openssl = b.option(bool, "openssl", "Use system-installed openssl for TLS support in zap") orelse false;
    const facilio = try build_facilio("facil.io", b, target, optimize, use_openssl);
    const zap_module = b.addModule("zap", .{
        .root_source_file = b.path("src/zap.zig"),
        .target = target,
        .optimize = optimize,
    });
    zap_module.linkLibrary(facilio);
}
