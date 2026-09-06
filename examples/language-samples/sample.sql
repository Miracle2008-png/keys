-- Real SQL: CTEs, window functions, joins.
WITH monthly AS (
    SELECT
        DATE_TRUNC('month', o.created_at) AS month,
        c.region,
        SUM(o.total_cents) / 100.0 AS revenue
    FROM orders o
    INNER JOIN customers c ON c.id = o.customer_id
    WHERE o.status <> 'cancelled'
    GROUP BY 1, 2
)
SELECT
    month,
    region,
    revenue,
    RANK() OVER (PARTITION BY month ORDER BY revenue DESC) AS rank
FROM monthly
ORDER BY month DESC, rank ASC
LIMIT 100;
