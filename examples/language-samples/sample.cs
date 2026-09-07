// Real C#: LINQ, records, pattern matching, async, nullable types.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace Example.Billing;

public record Invoice(int Id, string Customer, decimal Total, DateTime Issued);

public interface IRepository<T> where T : class
{
    Task<T?> FindAsync(int id);
}

public sealed class InvoiceRepository : IRepository<Invoice>
{
    private readonly Dictionary<int, Invoice> _cache = new();

    public async Task<Invoice?> FindAsync(int id)
    {
        if (_cache.TryGetValue(id, out var cached))
            return cached;

        await Task.Delay(10);
        return null;
    }

    public static string Describe(Invoice invoice) => invoice switch
    {
        { Total: > 10_000m } => "large",
        { Total: > 0m } => "standard",
        _ => "empty",
    };

    public IEnumerable<Invoice> Overdue(IEnumerable<Invoice> all) =>
        all.Where(i => i.Issued < DateTime.UtcNow.AddDays(-30))
           .OrderByDescending(i => i.Total);
}
