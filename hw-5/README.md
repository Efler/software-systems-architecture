# Profi Services API

## Вариант 4: Сайт заказа услуг https://profi.ru/ 


Profi API - REST API для сервса заказа услуг: пользователи, услуги и заказы.  Проект реализован на `userver-framework/userver`.

В проекте используются:

- PostgreSQL - основное хранилище данных;
- Redis - кеширование и счетчики rate limiting;
- Redis Sentinel - подключение Redis через стандартный компонент  userver;
- Docker Compose - запуск всей инфраструктуры.

## Реализовано

| Возможность | Статус |
|---|---|
| Создание пользователя | Реализовано |
| Поиск пользователя по логину | Реализовано, есть Redis-кеш |
| Поиск пользователя по маске имени и фамилии | Реализовано |
| Создание услуги | Реализовано, инвалидирует кеш списка услуг |
| Получение списка услуг | Реализовано, есть Redis-кеш и Token Bucket rate limiting |
| Добавление услуг в заказ | Реализовано, инвалидирует кеш заказа и ограничивается Sliding Window |
| Получение заказа пользователя | Реализовано, есть Redis-кеш |

## Запуск

Из каталога с `docker-compose.yaml`:

```powershell
docker compose up --build
```

API будет доступен по адресу:

```text
http://localhost:8080/api/v1
```

Контейнеры:

- `profi-services-api` - userver API;
- `profi-services-postgres` - PostgreSQL;
- `profi-services-redis` - Redis;
- `profi-services-redis-sentinel` - Redis Sentinel.

## Быстрая проверка

```powershell
.\scripts\smoke-test.ps1
```
или для bash:
```bash
chmod +x scripts/smoke-test.sh
./scripts/smoke-test.sh
```

Скрипт создает пользователя, проверяет поиск по логину, получает список услуг, создает новую услугу, добавляет ее в заказ и получает заказ пользвателя.

## Проверка оптимизаций

Минимальная проверка кеширования и rate limiting:

```powershell
.\scripts\check-optimizations.ps1
```

или для bash:
```bash
chmod +x scripts/check-optimizations.sh
./scripts/check-optimizations.sh
```

Скрипт проверяет, что `GET /api/v1/services` создает ключ кеша в Redis, создание услуги увеличивает версию кеша каталога, а серия регистраций с одним `X-Forwarded-For` получает `429 Too Many Requests` вместе с заголовками `X-RateLimit-*`.

## Ручные запросы

Получить список услуг:

```powershell
Invoke-RestMethod "http://localhost:8080/api/v1/services"
```

```bash
curl "http://localhost:8080/api/v1/services"
```

Найти пользователя по логину:

```powershell
Invoke-RestMethod "http://localhost:8080/api/v1/users?login=ivanov"
```

```bash
curl "http://localhost:8080/api/v1/users?login=ivanov"
```

Создать пользователя:

```powershell
Invoke-RestMethod -Method Post "http://localhost:8080/api/v1/users" `
  -ContentType "application/json" `
  -Body '{"login":"api-user","firstName":"Иван","lastName":"Иванов","email":"api-user@example.com"}'
```

```bash
curl -X POST "http://localhost:8080/api/v1/users" \
  -H "Content-Type: application/json" \
  -d '{"login":"api-user","firstName":"Иван","lastName":"Иванов","email":"api-user@example.com"}'
```

Создать услугу:

```powershell
Invoke-RestMethod -Method Post "http://localhost:8080/api/v1/services" `
  -ContentType "application/json" `
  -Body '{"name":"Химчистка дивана","description":"Выездная химчистка","price":4500,"durationMinutes":180}'
```

```bash
curl -X POST "http://localhost:8080/api/v1/services" \
  -H "Content-Type: application/json" \
  -d '{"name":"Химчистка дивана","description":"Выездная химчистка","price":4500,"durationMinutes":180}'
```

Добавить услуги в заказ:

```powershell
Invoke-RestMethod -Method Post "http://localhost:8080/api/v1/orders/services" `
  -ContentType "application/json" `
  -Body '{"userId":1,"serviceIds":[1,2]}'
```

```bash
curl -X POST "http://localhost:8080/api/v1/orders/services" \
  -H "Content-Type: application/json" \
  -d '{"userId":1,"serviceIds":[1,2]}'
```

Получить заказ:

```powershell
Invoke-RestMethod "http://localhost:8080/api/v1/orders?userId=1"
```

```bash
curl "http://localhost:8080/api/v1/orders?userId=1"
```

## Кеширование

Кеширование реализовано по схеме Cache-Aside.

| Endpoint | Ключ Redis | TTL | Инвалидация |
|---|---|---:|---|
| `GET /api/v1/services` | `services:list:v:{version}:limit:{limit}:offset:{offset}` | 10 минут | `POST /api/v1/services` увеличивает `cache:services:list:version` |
| `GET /api/v1/users?login={login}` | `user:login:{login}` | 10 минут | `POST /api/v1/users` удаляет ключ логина |
| `GET /api/v1/orders?userId={userId}` | `order:user:{userId}` | 60 секунд | `POST /api/v1/orders/services` удаляет ключ заказа |

Проверить ключи Redis:

```powershell
docker compose exec redis redis-cli --scan --pattern "*"
```

## Rate limiting

Rate limiting хранит состояние в Redis.

| Endpoint | Алгоритм | Лимит |
|---|---|---:|
| `POST /api/v1/users` | Sliding Window Counter | 10 запросов в минуту |
| `GET /api/v1/services` | Token Bucket | 300 запросов в минуту, burst 100 |
| `POST /api/v1/orders/services` | Sliding Window Counter | 60 запросов в минуту |

В ответ добавляются заголовки:

```text
X-RateLimit-Limit
X-RateLimit-Remaining
X-RateLimit-Reset
```

При превышении лимита API возвращает:

```text
429 Too Many Requests
```

Проверка заголовков:

```powershell
Invoke-WebRequest "http://localhost:8080/api/v1/services" `
  -Headers @{ "X-Forwarded-For" = "manual-check" }
```

```bash
curl -i "http://localhost:8080/api/v1/services" \
  -H "X-Forwarded-For: manual-check"
```

## Проверка сборки

```powershell
docker compose build
docker compose up -d
.\scripts\smoke-test.ps1
```

Дополнительно проверяется по:

- наличие ключей кеша и rate limiting в Redis;
- заголовки `X-RateLimit-*`;
- ответ `429 Too Many Requests` при превышении лимита регистрации.
