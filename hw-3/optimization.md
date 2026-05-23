# Оптимизация запросов

## Частые операции

Для системы управления складом чаще всего  выполняются:

| Операция | Основные таблицы | Условия |
|---|---|---|
| Логин пользователя | `users`, `auth_tokens` | `login`, `token` |
| Поиск пользователя | `users` | `login`, `first_name`, `last_name` |
| Поиск товара | `products` | `name`, `sku` |
| Получение остатков | `stock_balances`, `products` | `product_id` |
| История поступлений | `receipts`, `products`, `users` | `product_id`, `received_at` |
| История списаний | `write_offs`, `products`, `users` | `product_id`, `written_off_at` |

## Индексы

Индексы первичных ключей и уникальных полей PostgreSQL создает автоматически:

- `users.id`, `users.login`, `users.email`;
- `auth_tokens.id`, `auth_tokens.token`;
- `products.id`, `products.sku`;
- `stock_balances.product_id`;
- `receipts.id`;
- `write_offs.id`.

Дополнительно созданы индексы:

| Индекс | Назначение |
|---|---|
| `idx_auth_tokens_user_id` | Поиск токенов пользователя, обслуживание FK `auth_tokens.user_id` |
| `idx_users_full_name_lower_pattern` | Поиск пользователя по имени и фамилии без учета регистра |
| `idx_products_name_lower_pattern` | Поиск товара по названию без учета регистра |
| `idx_receipts_product_id_received_at` | История поступлений конкретного товара в порядке новых записей |
| `idx_write_offs_product_id_written_off_at` | История списаний конкретного товара в порядке новых записей |

Набор индексов оставлен небольшим. Отдельные индексы только по дате или только по `created_by` не добавлены, так как в текущем API нет частых выборок только по этим полям .

## EXPLAIN

Проверка поиска товара:

```sql
EXPLAIN (ANALYZE, BUFFERS)
SELECT id, name, sku, unit, description, created_at
FROM products
WHERE lower(name) LIKE 'office%'
ORDER BY name
LIMIT 20;
```

На тестовых 10 строках PostgreSQL выбирает последовательное чтение:

```text
Seq Scan on products
Filter: (lower((name)::text) ~~ 'office%'::text)
Rows Removed by Filter: 9
Execution Time: 0.185 ms
```

Такой план нормален для маленькой таблицы: прочитать 10 строк дешевле, чем обращаться к индексу.

Проверка пригодности индекса:

```sql
SET enable_seqscan = off;

EXPLAIN
SELECT id, name, sku, unit, description, created_at
FROM products
WHERE lower(name) LIKE 'office%'
ORDER BY name
LIMIT 20;

RESET enable_seqscan;
```

План с индексом:

```text
Index Scan using idx_products_name_lower_pattern on products
Index Cond: ((lower((name)::text) ~>=~ 'office'::text)
             AND (lower((name)::text) ~<~ 'officf'::text))
```

Для истории поступлений:

```sql
SET enable_seqscan = off;

EXPLAIN
SELECT r.id, r.product_id, p.name, r.quantity, r.received_at
FROM receipts r
JOIN products p ON p.id = r.product_id
WHERE r.product_id = 1
ORDER BY r.received_at DESC
LIMIT 20;

RESET enable_seqscan;
```

План:

```text
Index Scan using idx_receipts_product_id_received_at on receipts r
Index Cond: (product_id = 1)

Index Scan using products_pkey on products p
Index Cond: (id = 1)
```

## Сравнение до и после

На примере поиска товара по названию:

До индекса:

```text
Seq Scan on products
Filter: (lower((name)::text) ~~ 'office%'::text)
```

После индекса:

```text
Index Scan using idx_products_name_lower_pattern on products
Index Cond: ((lower((name)::text) ~>=~ 'office'::text)
             AND (lower((name)::text) ~<~ 'officf'::text))
```

Для маленького набора данных фактическое время почти не отличается. При росте таблицы индекс снижает количество просматриваемых строк для поиска по началу названия.

## Итог

Текущая схема покрывает основные операции API. Дополнительная оптимизация запросов не требуется: JOIN выполняются по первичным и внешним ключам, поиск по логину/SKU/токену покрыт уникальными индексами, истории движений покрыты составными индексами по товару и дате.
