# Warehouse Inventory PostgreSQL

## Вариант 18: Система управления складом https://www.zoho.com/inventory/

## Назначение

Реляционная база данных для сервиса управления складом. Схема хранит пользователей, товары, текущие остатки, поступления, списания  и токены аутентификаци API.

## Состав файлов

| Файл | Назначение |
|---|---|
| `schema.sql` | Создание таблиц, ограничений и индексов |
| `data.sql` | Заполнение таблиц тестовыми данными |
| `queries.sql` | SQL-запросы для операций складской системы |
| `optimization.md` | Описание индексов, EXPLAIN и выводы по оптимизации |
| `Dockerfile` | Сборка REST API сервиса |
| `docker-compose.yaml` | Запуск API, Swagger UI и PostgreSQL |

## Схема БД

Основные таблицы:

| Таблица | Назначение |
|---|---|
| `users` | Пользователи системы |
| `auth_tokens` | Токены аутентификации пользователей |
| `products` | Товары склада |
| `stock_balances` | Текущие остатки товаров |
| `receipts` | История поступлений |
| `write_offs` | История списаний |

Связи:

| Связь | Тип |
|---|---|
| `users -> auth_tokens` | `1:N` |
| `products -> stock_balances` | `1:1` |
| `products -> receipts` | `1:N` |
| `products -> write_offs` | `1:N` |
| `users -> receipts` | `1:N` |
| `users -> write_offs` | `1:N` |

## Ограничения

В схеме используются:

- `PRIMARY KEY` для идентификаторов таблиц;
- `FOREIGN KEY` для связей между сущностями;
- `NOT NULL` для обязательных полей;
- `UNIQUE` для `users.login`, `users.email`, `auth_tokens.token`, `products.sku`;
- `CHECK` для ролей пользователей, единиц измерения и положительного количества товаров.

Остаток товара не может быть отрицательным:

```sql
quantity BIGINT NOT NULL DEFAULT 0 CHECK (quantity >= 0)
```

Поступления и списания принимают только положительное количество:

```sql
quantity BIGINT NOT NULL CHECK (quantity > 0)
```

## Индексы

Автоматические индексы создаются PostgreSQL для `PRIMARY KEY` и `UNIQUE`.

Дополнительно в `schema.sql` созданы:

| Индекс | Назначение |
|---|---|
| `idx_auth_tokens_user_id` | Поиск токенов пользователя и обслуживание FK |
| `idx_users_full_name_lower_pattern` | Поиск пользователей по имени и фамилии |
| `idx_products_name_lower_pattern` | Поиск товаров по названию |
| `idx_receipts_product_id_received_at` | История поступлений по товару |
| `idx_write_offs_product_id_written_off_at` | История списаний по товару |

## Тестовые данные

`data.sql` добавляет по 10 записей в каждую таблицу:

| Таблица | Количество |
|---|---:|
| `users` | 10 |
| `auth_tokens` | 10 |
| `products` | 10 |
| `stock_balances` | 10 |
| `receipts` | 10 |
| `write_offs` | 10 |

Остатки согласованы с поступлениями  и списаниями.

## Запуск

Запуск API и PostgreSQL:

```bash
docker compose up --build
```

PostgreSQL:

```text
host: localhost
port: 5432
database: warehouse_inventory
user: warehouse_user
password: warehouse_password
```

API:

```text
http://localhost:8080
```

Swagger UI:

```text
http://localhost:8082
```

Остановка:

```bash
docker compose down
```

## Проверка БД

Список таблиц:

```bash
docker compose exec postgres psql -U warehouse_user -d warehouse_inventory -c "\dt"
```

Количество записей:

```bash
docker compose exec postgres psql -U warehouse_user -d warehouse_inventory -c "SELECT 'users' AS table_name, count(*) FROM users UNION ALL SELECT 'auth_tokens', count(*) FROM auth_tokens UNION ALL SELECT 'products', count(*) FROM products UNION ALL SELECT 'stock_balances', count(*) FROM stock_balances UNION ALL SELECT 'receipts', count(*) FROM receipts UNION ALL SELECT 'write_offs', count(*) FROM write_offs ORDER BY table_name;"
```

Проверка индексов:

```bash
docker compose exec postgres psql -U warehouse_user -d warehouse_inventory -c "SELECT indexname, tablename FROM pg_indexes WHERE schemaname = 'public' ORDER BY tablename, indexname;"
```

## Проверка API

PowerShell:

```powershell
.\scripts\api-test.ps1
```

macOS/Linux:

```bash
chmod +x scripts/api-test.sh
./scripts/api-test.sh
```

## SQL запросы

`queries.sql` содержит запросы для всех операций складской системы:

- создание нового пользователя;
- поиск пользователя по логину;
- поиск пользователя по маске имени и фамилии;
- добавление товара на склад;
- поиск товара по названию;
- получение остатков товаров;
- создание поступления товара;
- получение истории поступлений;
- списание товара со склада;
- проверка токена аутентификации.

## Оптимизация

Описание оптимизаций находится в `optimization.md`.

В документе привдены:

- частые запросы системы;
- список индексов и их назначение;
- примеры `EXPLAIN`;
- сравнение плана поиска товара до и после добавления индекса.
