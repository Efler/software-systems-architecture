-- Создание нового пользователя
INSERT INTO users (login, first_name, last_name, email, role, password_hash)
VALUES ($1, $2, $3, $4, $5, $6)
RETURNING id, login, first_name, last_name, email, role, created_at;

-- Поиск пользователя по логину
SELECT id, login, first_name, last_name, email, role, created_at
FROM users
WHERE login = $1;

-- Поиск пользователя по маске имени и фамилии
SELECT id, login, first_name, last_name, email, role, created_at
FROM users
WHERE lower(first_name) LIKE '%' || lower($1) || '%'
  AND lower(last_name) LIKE '%' || lower($2) || '%'
ORDER BY last_name, first_name
LIMIT $3 OFFSET $4;

-- Добавление товара на склад
WITH new_product AS (
    INSERT INTO products (name, sku, unit, description)
    VALUES ($1, $2, $3, COALESCE($4, ''))
    RETURNING id, name, sku, unit, description, created_at
),
new_balance AS (
    INSERT INTO stock_balances (product_id, quantity)
    SELECT id, 0
    FROM new_product
    RETURNING product_id, quantity, updated_at
)
SELECT
    p.id,
    p.name,
    p.sku,
    p.unit,
    p.description,
    b.quantity,
    b.updated_at
FROM new_product p
JOIN new_balance b ON b.product_id = p.id;

-- Поиск товара по названию
SELECT id, name, sku, unit, description, created_at
FROM products
WHERE lower(name) LIKE '%' || lower($1) || '%'
ORDER BY name
LIMIT $2 OFFSET $3;

-- Получение остатков товаров
SELECT
    p.id AS product_id,
    p.name,
    p.sku,
    p.unit,
    b.quantity,
    b.updated_at
FROM stock_balances b
JOIN products p ON p.id = b.product_id
ORDER BY p.name
LIMIT $1 OFFSET $2;

-- Создание поступления товара
WITH new_receipt AS (
    INSERT INTO receipts (product_id, quantity, created_by, comment)
    VALUES ($1, $2, $3, COALESCE($4, ''))
    RETURNING id, product_id, quantity, received_at, created_by, comment, created_at
),
updated_balance AS (
    INSERT INTO stock_balances (product_id, quantity)
    VALUES ($1, $2)
    ON CONFLICT (product_id)
    DO UPDATE SET
        quantity = stock_balances.quantity + EXCLUDED.quantity,
        updated_at = now()
    RETURNING product_id, quantity, updated_at
)
SELECT
    r.id,
    r.product_id,
    r.quantity AS receipt_quantity,
    r.received_at,
    r.created_by,
    r.comment,
    b.quantity AS stock_quantity,
    b.updated_at AS stock_updated_at
FROM new_receipt r
JOIN updated_balance b ON b.product_id = r.product_id;

-- Получение истории поступлений
SELECT
    r.id,
    r.product_id,
    p.name AS product_name,
    p.sku,
    r.quantity,
    r.received_at,
    r.created_by,
    u.login AS created_by_login,
    r.comment
FROM receipts r
JOIN products p ON p.id = r.product_id
JOIN users u ON u.id = r.created_by
ORDER BY r.received_at DESC
LIMIT $1 OFFSET $2;

-- Списание товара со склада
WITH updated_balance AS (
    UPDATE stock_balances
    SET
        quantity = quantity - $2,
        updated_at = now()
    WHERE product_id = $1
      AND quantity >= $2
    RETURNING product_id, quantity, updated_at
),
new_write_off AS (
    INSERT INTO write_offs (product_id, quantity, created_by, reason)
    SELECT product_id, $2, $3, COALESCE($4, '')
    FROM updated_balance
    RETURNING id, product_id, quantity, written_off_at, created_by, reason, created_at
)
SELECT
    w.id,
    w.product_id,
    w.quantity AS written_off_quantity,
    w.written_off_at,
    w.created_by,
    w.reason,
    b.quantity AS stock_quantity,
    b.updated_at AS stock_updated_at
FROM new_write_off w
JOIN updated_balance b ON b.product_id = w.product_id;

-- Проверка токена аутентификации
SELECT
    t.id AS token_id,
    t.user_id,
    u.login,
    u.role
FROM auth_tokens t
JOIN users u ON u.id = t.user_id
WHERE t.token = $1
  AND t.revoked_at IS NULL
  AND (t.expires_at IS NULL OR t.expires_at > now());
