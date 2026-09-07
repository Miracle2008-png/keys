// Real Zig: comptime, error unions, allocators, slices.
const std = @import("std");
const Allocator = std.mem.Allocator;

const InvoiceError = error{
    NotFound,
    InvalidAmount,
};

pub const Status = enum { draft, sent, paid };

pub const Invoice = struct {
    id: u32,
    amount: f64 = 0.0,
    status: Status = .draft,

    pub fn describe(self: Invoice) []const u8 {
        return if (self.amount > 10_000) "large" else if (self.amount > 0) "standard" else "empty";
    }
};

pub fn total(invoices: []const Invoice) f64 {
    var sum: f64 = 0;
    for (invoices) |invoice| {
        sum += invoice.amount;
    }
    return sum;
}

pub fn find(list: []const Invoice, id: u32) InvoiceError!Invoice {
    for (list) |invoice| {
        if (invoice.id == id) return invoice;
    }
    return InvoiceError.NotFound;
}

test "total sums amounts" {
    const items = [_]Invoice{ .{ .id = 1, .amount = 10 }, .{ .id = 2, .amount = 5 } };
    try std.testing.expectEqual(@as(f64, 15), total(&items));
}
