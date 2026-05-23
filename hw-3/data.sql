INSERT INTO users (id, login, first_name, last_name, email, role, password_hash, created_at) VALUES
    (1, 'ivanov', 'Ivan', 'Ivanov', 'ivanov@example.com', 'warehouse_manager', 'hash_ivanov', now() - interval '10 days'),
    (2, 'petrov', 'Petr', 'Petrov', 'petrov@example.com', 'warehouse_manager', 'hash_petrov', now() - interval '9 days'),
    (3, 'sidorov', 'Sidor', 'Sidorov', 'sidorov@example.com', 'warehouse_manager', 'hash_sidorov', now() - interval '8 days'),
    (4, 'smirnova', 'Anna', 'Smirnova', 'smirnova@example.com', 'warehouse_manager', 'hash_smirnova', now() - interval '7 days'),
    (5, 'kuznetsov', 'Alexey', 'Kuznetsov', 'kuznetsov@example.com', 'admin', 'hash_kuznetsov', now() - interval '6 days'),
    (6, 'popova', 'Maria', 'Popova', 'popova@example.com', 'warehouse_manager', 'hash_popova', now() - interval '5 days'),
    (7, 'volkov', 'Dmitry', 'Volkov', 'volkov@example.com', 'warehouse_manager', 'hash_volkov', now() - interval '4 days'),
    (8, 'fedorova', 'Elena', 'Fedorova', 'fedorova@example.com', 'warehouse_manager', 'hash_fedorova', now() - interval '3 days'),
    (9, 'morozov', 'Nikolay', 'Morozov', 'morozov@example.com', 'warehouse_manager', 'hash_morozov', now() - interval '2 days'),
    (10, 'novikova', 'Olga', 'Novikova', 'novikova@example.com', 'admin', 'hash_novikova', now() - interval '1 day');

INSERT INTO auth_tokens (id, user_id, token, created_at, expires_at, revoked_at) VALUES
    (1, 1, 'token-ivanov-001', now() - interval '10 days', now() + interval '20 days', NULL),
    (2, 2, 'token-petrov-001', now() - interval '9 days', now() + interval '21 days', NULL),
    (3, 3, 'token-sidorov-001', now() - interval '8 days', now() + interval '22 days', NULL),
    (4, 4, 'token-smirnova-001', now() - interval '7 days', now() + interval '23 days', NULL),
    (5, 5, 'token-kuznetsov-001', now() - interval '6 days', now() + interval '24 days', NULL),
    (6, 6, 'token-popova-001', now() - interval '5 days', now() + interval '25 days', NULL),
    (7, 7, 'token-volkov-001', now() - interval '4 days', now() + interval '26 days', NULL),
    (8, 8, 'token-fedorova-001', now() - interval '3 days', now() + interval '27 days', NULL),
    (9, 9, 'token-morozov-001', now() - interval '2 days', now() + interval '28 days', NULL),
    (10, 10, 'token-novikova-001', now() - interval '1 day', now() + interval '29 days', NULL);

INSERT INTO products (id, name, sku, unit, description, created_at) VALUES
    (1, 'Office monitor 24', 'MON-24-001', 'pcs', 'Office monitor 24 inches', now() - interval '10 days'),
    (2, 'Wireless keyboard', 'KEY-WL-001', 'pcs', 'Wireless keyboard with USB receiver', now() - interval '9 days'),
    (3, 'Optical mouse', 'MOU-OP-001', 'pcs', 'Optical mouse for office workstations', now() - interval '8 days'),
    (4, 'Printer paper A4', 'PAPER-A4-001', 'pcs', 'A4 paper pack', now() - interval '7 days'),
    (5, 'Ethernet cable 3m', 'CAB-ETH-003', 'pcs', 'Network cable 3 meters', now() - interval '6 days'),
    (6, 'Laptop stand', 'STAND-LAP-001', 'pcs', 'Adjustable laptop stand', now() - interval '5 days'),
    (7, 'USB-C hub', 'HUB-USBC-001', 'pcs', 'USB-C hub with HDMI and USB ports', now() - interval '4 days'),
    (8, 'Barcode labels', 'LBL-BAR-001', 'pcs', 'Labels for barcode printer', now() - interval '3 days'),
    (9, 'Packing film', 'FILM-PACK-001', 'm', 'Stretch packing film', now() - interval '2 days'),
    (10, 'Thermal printer', 'PRINT-THERM-001', 'pcs', 'Thermal printer for warehouse labels', now() - interval '1 day');

INSERT INTO stock_balances (product_id, quantity, updated_at) VALUES
    (1, 45, now()),
    (2, 37, now()),
    (3, 65, now()),
    (4, 80, now()),
    (5, 28, now()),
    (6, 21, now()),
    (7, 53, now()),
    (8, 39, now()),
    (9, 65, now()),
    (10, 30, now());

INSERT INTO receipts (id, product_id, quantity, received_at, created_by, comment, created_at) VALUES
    (1, 1, 50, now() - interval '10 days', 1, 'Initial delivery', now() - interval '10 days'),
    (2, 2, 40, now() - interval '9 days', 2, 'Initial delivery', now() - interval '9 days'),
    (3, 3, 75, now() - interval '8 days', 3, 'Initial delivery', now() - interval '8 days'),
    (4, 4, 100, now() - interval '7 days', 4, 'Initial delivery', now() - interval '7 days'),
    (5, 5, 30, now() - interval '6 days', 5, 'Initial delivery', now() - interval '6 days'),
    (6, 6, 25, now() - interval '5 days', 6, 'Initial delivery', now() - interval '5 days'),
    (7, 7, 60, now() - interval '4 days', 7, 'Initial delivery', now() - interval '4 days'),
    (8, 8, 45, now() - interval '3 days', 8, 'Initial delivery', now() - interval '3 days'),
    (9, 9, 80, now() - interval '2 days', 9, 'Initial delivery', now() - interval '2 days'),
    (10, 10, 35, now() - interval '1 day', 10, 'Initial delivery', now() - interval '1 day');

INSERT INTO write_offs (id, product_id, quantity, written_off_at, created_by, reason, created_at) VALUES
    (1, 1, 5, now() - interval '9 days', 1, 'Damaged package', now() - interval '9 days'),
    (2, 2, 3, now() - interval '8 days', 2, 'Internal usage', now() - interval '8 days'),
    (3, 3, 10, now() - interval '7 days', 3, 'Inventory adjustment', now() - interval '7 days'),
    (4, 4, 20, now() - interval '6 days', 4, 'Warehouse consumption', now() - interval '6 days'),
    (5, 5, 2, now() - interval '5 days', 5, 'Damaged item', now() - interval '5 days'),
    (6, 6, 4, now() - interval '4 days', 6, 'Internal usage', now() - interval '4 days'),
    (7, 7, 7, now() - interval '3 days', 7, 'Inventory adjustment', now() - interval '3 days'),
    (8, 8, 6, now() - interval '2 days', 8, 'Damaged labels', now() - interval '2 days'),
    (9, 9, 15, now() - interval '1 day', 9, 'Packing usage', now() - interval '1 day'),
    (10, 10, 5, now(), 10, 'Printer maintenance replacement', now());

SELECT setval('users_id_seq', 10, true);
SELECT setval('auth_tokens_id_seq', 10, true);
SELECT setval('products_id_seq', 10, true);
SELECT setval('receipts_id_seq', 10, true);
SELECT setval('write_offs_id_seq', 10, true);
