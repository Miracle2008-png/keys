<?php

declare(strict_types=1);

namespace App\Billing;

use App\Contracts\RepositoryInterface;
use InvalidArgumentException;

final class InvoiceRepository implements RepositoryInterface
{
    /** @var array<int, Invoice> */
    private array $cache = [];

    public function __construct(
        private readonly \PDO $db,
        private string $table = 'invoices',
    ) {
    }

    public function find(int $id): ?Invoice
    {
        if (isset($this->cache[$id])) {
            return $this->cache[$id];
        }

        $stmt = $this->db->prepare("SELECT * FROM {$this->table} WHERE id = :id");
        $stmt->execute(['id' => $id]);

        $row = $stmt->fetch(\PDO::FETCH_ASSOC);
        if ($row === false) {
            return null;
        }

        return $this->cache[$id] = new Invoice(
            id: (int) $row['id'],
            total: (float) $row['total'],
        );
    }

    public function overdue(array $invoices): array
    {
        return array_filter($invoices, static fn(Invoice $i): bool => $i->isOverdue());
    }
}
