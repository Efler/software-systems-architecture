# Warehouse Inventory API

REST API сервис для управления складом. Сервис хранит пользователей, товары, поступления, списания и рассчитывает текущие остатки товаров. Реализация выполнена на `userver-framework/userver`, данные хранятся в памяти процесса.

## Вариант 18: Система управления складом https://www.zoho.com/inventory/

## Состав проекта

| Путь | Назначение |
|---|---|
| `src/main.cpp` | Исходный код сервиса, DTO, in-memory хранилище и HTTP handlers |
| `configs/static_config.yaml` | Конфигурация компонентов userver |
| `openapi.yaml` | OpenAPI спецификация |
| `Dockerfile` | Сборка контейнера API |
| `docker-compose.yaml` | Запуск API и Swagger UI |
| `scripts/smoke-test.ps1` | Быстрая проверка основного сценария через PowerShell |
| `scripts/api-test.ps1` | Тесты API через PowerShell |
| `scripts/api-test.sh` | Тесты API для macOS/Linux |

## API

Базовый URL:

```text
http://localhost:8080/api/v1
```

Основные endpoints:

| Метод | URL | Назначение |
|---|---|---|
| `POST` | `/auth/register` | Регистрация пользователя |
| `POST` | `/auth/login` | Логин пользователя |
| `POST` | `/users` | Создание пользователя |
| `GET` | `/users?login={login}` | Поиск пользователя по логину |
| `GET` | `/users?firstNameMask={mask}&lastNameMask={mask}` | Поиск пользователей по маске имени и фамилии |
| `POST` | `/products` | Добавление товара |
| `GET` | `/products?name={name}` | Поиск товара по названию |
| `GET` | `/stock-balances` | Получение остатков |
| `POST` | `/receipts` | Создание поступления |
| `GET` | `/receipts` | История поступлений |
| `POST` | `/write-offs` | Списание товара |

Защищенные endpoints:

- `POST /products`
- `POST /receipts`
- `POST /write-offs`

Для них нужен заголовок:

```text
Authorization: Bearer <token>
```

Токен возвращается при регистрации или логине.

## Запуск

```bash
docker compose up --build
```

API будет доступен на:

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

## Примеры запросов

Регистрация:

```bash
curl -X POST http://localhost:8080/api/v1/auth/register \
  -H "Content-Type: application/json" \
  -d '{"login":"ivanov","firstName":"Ivan","lastName":"Ivanov","email":"ivanov@example.com","role":"warehouse_manager","password":"secret"}'
```

Логин:

```bash
curl -X POST http://localhost:8080/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"login":"ivanov","password":"secret"}'
```

Добавление товара:

```bash
curl -X POST http://localhost:8080/api/v1/products \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer token-1-2" \
  -d '{"name":"Office monitor 24","sku":"MON-24-001","unit":"pcs","description":"Office monitor"}'
```

Создание поступления:

```bash
curl -X POST http://localhost:8080/api/v1/receipts \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer token-1-2" \
  -d '{"productId":1,"quantity":10,"createdBy":1,"comment":"Initial delivery"}'
```

Списание товара:

```bash
curl -X POST http://localhost:8080/api/v1/write-offs \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer token-1-2" \
  -d '{"productId":1,"quantity":2,"createdBy":1,"reason":"Damaged package"}'
```

Получение остатков:

```bash
curl http://localhost:8080/api/v1/stock-balances
```

История поступлений:

```bash
curl http://localhost:8080/api/v1/receipts
```

## Тестирование

Поднять сервис:

```bash
docker compose up --build
```

PowerShell:

```powershell
.\scripts\api-test.ps1
```

macOS/Linux:

```bash
chmod +x scripts/api-test.sh
./scripts/api-test.sh
```

Быстрая проверка основного сценария:

```powershell
.\scripts\smoke-test.ps1
```

Тесты покрывают:

- регистрацию и логин;
- создание товара;
- создание поступления;
- списание товара;
- поиск пользователей и товаров;
- получение остатков;
- историю поступлений;
- ошибки валидации;
- отсутствие токена;
- отсутствующие ресурсы;
- конфликт уникальности;
- списание сверх остатка.

## OpenAPI

Спецификация находится в файле:

```text
openapi.yaml
```

В Swagger UI можно выполнить запросы интерактивно. Для защищенных endpoints нужно сначала получить токен через `POST /auth/register` или `POST /auth/login`, затем нажать `Authorize` и указать:

```text
Bearer <token>
```

В поле авторизации Swagger UI нужно вводить значение целиком вместе с префиксом `Bearer`, например:

```text
Bearer token-1-2
```

## Статусы ошибок

| Код | Когда возвращается |
|---|---|
| `400 Bad Request` | Некорректные query-параметры |
| `401 Unauthorized` | Не передан токен или неверные учетные данные |
| `404 Not Found` | Пользователь или товар не найден |
| `409 Conflict` | Дубликат логина, дубликат SKU или недостаточный остаток |
| `422 Unprocessable Entity` | Не заполнены обязательные поля |

Формат ошибки:

```json
{
  "error": {
    "code": "NOT_ENOUGH_STOCK",
    "message": "Not enough stock for write-off"
  }
}
```

## Хранение данных

Данные хранятся в in-memory структурах и сбрасываются при перезапуске контейнера.
