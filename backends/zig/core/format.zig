//! tr-TR formatting and the daily chart; byte-identical to the C++ core.
const std = @import("std");
const db = @import("db.zig");
const A = std.mem.Allocator;

fn group(a: A, v: i64) ![]const u8 {
    var buf: [32]u8 = undefined;
    const digits = try std.fmt.bufPrint(&buf, "{d}", .{@abs(v)});
    var out: std.ArrayList(u8) = .empty;
    if (v < 0) try out.append(a, '-');
    for (digits, 0..) |ch, i| {
        if (i > 0 and (digits.len - i) % 3 == 0) try out.append(a, '.');
        try out.append(a, ch);
    }
    return out.toOwnedSlice(a);
}

pub fn int(a: A, v: i64) ![]const u8 {
    return group(a, v);
}
pub fn money(a: A, cents: i64) ![]const u8 {
    return std.fmt.allocPrint(a, "₺{s},{d:0>2}", .{ try group(a, @divTrunc(cents, 100)), @abs(@rem(cents, 100)) });
}
fn oneDecimalComma(a: A, v: f64) ![]const u8 {
    const s = try std.fmt.allocPrint(a, "{d:.1}", .{v});
    std.mem.replaceScalar(u8, s, '.', ',');
    return s;
}
pub fn pct(a: A, ratio: f64) ![]const u8 {
    return std.fmt.allocPrint(a, "%{s}", .{try oneDecimalComma(a, @round(ratio * 1000.0) / 10.0)});
}
pub fn rating(a: A, r: f64) ![]const u8 {
    return std.fmt.allocPrint(a, "★ {s}", .{try oneDecimalComma(a, r)});
}

fn chartNum(a: A, v: f64) ![]const u8 {
    var s = try std.fmt.allocPrint(a, "{d:.2}", .{@round(v * 100.0) / 100.0});
    while (s.len > 0 and s[s.len - 1] == '0') s = s[0 .. s.len - 1];
    if (s.len > 0 and s[s.len - 1] == '.') s = s[0 .. s.len - 1];
    return if (std.mem.eql(u8, s, "-0")) "0" else s;
}

pub fn dailyChartSvg(a: A, days: []const db.DayPoint) ![]const u8 {
    var max: i64 = 0;
    for (days) |d| max = @max(max, d.events);
    if (max == 0) max = 1;
    const bar_w: f64 = 960.0 / @as(f64, @floatFromInt(@max(days.len, 1)));
    var out: std.ArrayList(u8) = .empty;
    try out.appendSlice(a, "<svg class=\"chart-svg\" viewBox=\"0 0 960 240\" width=\"960\" height=\"240\" role=\"img\" aria-label=\"Günlük olaylar\">");
    for (days, 0..) |d, i| {
        const h: i64 = @intFromFloat(@round(@as(f64, @floatFromInt(d.events)) / @as(f64, @floatFromInt(max)) * 220.0));
        try out.appendSlice(a, try std.fmt.allocPrint(a, "<rect x=\"{s}\" y=\"{d}\" width=\"{s}\" height=\"{d}\" fill=\"#4f6bed\"/>", .{
            try chartNum(a, @as(f64, @floatFromInt(i)) * bar_w + 1.0), 240 - h, try chartNum(a, bar_w - 2.0), h,
        }));
    }
    try out.appendSlice(a, "</svg>");
    return out.toOwnedSlice(a);
}

test "formats like the C++ core" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const a = arena.allocator();
    try std.testing.expectEqualStrings("₺1.234,50", try money(a, 123450));
    try std.testing.expectEqualStrings("₺0,05", try money(a, 5));
    try std.testing.expectEqualStrings("₺1.234.567,89", try money(a, 123456789));
    try std.testing.expectEqualStrings("12.345", try int(a, 12345));
    try std.testing.expectEqualStrings("999", try int(a, 999));
    try std.testing.expectEqualStrings("%12,3", try pct(a, 0.1234));
    try std.testing.expectEqualStrings("★ 4,5", try rating(a, 4.5));
}

test "chart is byte-identical to the C++ core" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const a = arena.allocator();
    const d = [_]db.DayPoint{ .{ .day = "", .events = 10, .revenue_cents = 0 }, .{ .day = "", .events = 20, .revenue_cents = 0 }, .{ .day = "", .events = 5, .revenue_cents = 0 } };
    try std.testing.expectEqualStrings("<svg class=\"chart-svg\" viewBox=\"0 0 960 240\" width=\"960\" height=\"240\" role=\"img\" aria-label=\"Günlük olaylar\"><rect x=\"1\" y=\"130\" width=\"318\" height=\"110\" fill=\"#4f6bed\"/><rect x=\"321\" y=\"20\" width=\"318\" height=\"220\" fill=\"#4f6bed\"/><rect x=\"641\" y=\"185\" width=\"318\" height=\"55\" fill=\"#4f6bed\"/></svg>", try dailyChartSvg(a, &d));
    var seven: [7]db.DayPoint = undefined;
    for (&seven) |*x| x.* = .{ .day = "", .events = 0, .revenue_cents = 0 };
    try std.testing.expect(std.mem.indexOf(u8, try dailyChartSvg(a, &seven), "<rect x=\"138.14\" y=\"240\" width=\"135.14\" height=\"0\"") != null);
    try std.testing.expectEqualStrings("2026-11-01", try db.isoDay(a, db.anchor_ts));
}
