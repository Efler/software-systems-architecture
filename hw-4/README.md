# Проектирование и работа с MongoDB

## Вариант 5: Мессенджер https://slack.com/intl/en-gb/ 

## Описание проекта

Проект реализует API мессенджера с хранением данных в MongoDB. В системе есть пользователи, групповые чаты, сообщения групповых чатов, PtP-чаты и PtP-сообщения.

API написан на `userver-framework`. MongoDB подключена через компонент `components::Mongo`, работа с колекциями  выполняется через `storages::mongo::Pool`.

## Состав файлов

| Файл | Назначение |
|---|---|
| `schema_design.md` | Проектирование документной модели и обоснование embedded/references |
| `data.js` | Создание коллекций, индексов и тестовых данных |
| `queries.js` | CRUD-запросы MongoDB для операций мессенджера |
| `validation.js` | Проверка `$jsonSchema`-валидации  коллекции `users` |
| `README.md` | Описание проекта и инструкции по запуску |
| `Dockerfile` | Сборка userver API |
| `docker-compose.yml` | Запуск API, MongoDB и Swagger UI |
| `openapi.yaml` | Swagger/OpenAPI-описание HTTP API |
| `src/main.cpp` | Реализация API и MongoDB-операций |
| `configs/static_config.yaml` | Конфигурация userver-сервиса |
| `scripts/check_api.ps1` | Быстрая проверка API, MongoDB и валидации для PowerShell |
| `scripts/check_api.sh` | Быстрая проверка API, MongoDB и валидации для bash |

## Документная модель

В MongoDB используются коллекции:

| Коллекция | Назначение |
|---|---|
| `users` | Пользователи системы |
| `group_chats` | Групповые чаты и состав участников |
| `group_messages` | Сообщения групповых чатов |
| `ptp_chats` | Личные PtP-чаты между двумя пользователями |
| `ptp_messages` | Сообщения личных PtP-чатов |

Краткая логика модели:

- пользователи хранятся отдельно, потому что на них ссылаются чаты и сообщения;
- участники группового чата встроены в документ `group_chats.members`, так как состав нужен вместе с чатом;
- сообщения вынесены в отдельные коллекции, потому что история переписки постоянно растет;
- PtP-чат хранит массив из двух участников и `participant_key`, чтобы не создавать дубли диалогов для одной пары пользователей.

Подробное описание структуры документов и выбора embedded/references находится в `schema_design.md`.

## Запуск

```bash
docker compose -f docker-compose.yml up -d --build
```

После запуска доступны:

- API: `http://localhost:8080`
- Swagger UI: `http://localhost:8082`
- MongoDB: `mongodb://localhost:27018/messenger_api`

Проверка состояния API:

```bash
curl http://localhost:8080/api/v1/health
```

Ожидаемый ответ:

```json
{"status":"ok"}
```

## Инициализация данных

Скрипт `data.js` подключен в `docker-compose.yml` как init-скрипт MongoDB:

```yaml
./data.js:/docker-entrypoint-initdb.d/data.js:ro
```

При первом старте MongoDB создается база `messenger_api`, коллекции, индексы и тестовые данные:

```text
10 users
10 group_chats
10 group_messages
10 ptp_chats
10 ptp_messages
```

После запуска MongoDB отдельный контейнер `messenger_api_mongodb_validation` выполняет `validation.js`. API стартует только после успешного применения валидации схем.

Проверить количество документов:

```bash
docker compose -f docker-compose.yml exec -T mongodb mongosh messenger_api --quiet --eval "db.users.countDocuments() + ' users, ' + db.group_chats.countDocuments() + ' group_chats, ' + db.group_messages.countDocuments() + ' group_messages, ' + db.ptp_chats.countDocuments() + ' ptp_chats, ' + db.ptp_messages.countDocuments() + ' ptp_messages'"
```

## MongoDB-запросы

CRUD-запросы собраны в `queries.js`. В скрипте есть:

- создание пользователя, группового чата, сообщения группового чата, PtP-чата и PtP-сообщения;
- поиск пользователя по логину;
- поиск пользователя по маске имени и фамилии;
- загрузка сообщений группового чата;
- получение PtP-сообщений пользователя;
- обновление профиля, состава чата, реакции, статуса прочтения;
- удаление временных документов.

Запуск:

```bash
docker compose -f docker-compose.yml exec -T mongodb mongosh messenger_api --file /scripts/queries.js
```

В запросах используются операторы `$eq`, `$ne`, `$gt`, `$lt`, `$in`, `$and`, `$or`, `$regex`, `$set`, `$inc`, `$push`, `$pull`, `$addToSet`.

## Валидация схем

В `validation.js` настроена `$jsonSchema`-валидация коллекции `users`.

Проверяются:

- обязательные поля `login`, `first_name`, `last_name`, `display_name`, `email`, `is_active`, `profile`, `created_at`, `updated_at`;
- типы `objectId`, `string`, `bool`, `object`, `array`, `date`;
- ограничения `minLength`, `maxLength`, `minItems`, `maxItems`;
- `pattern` для `login`, `email` и `profile.timezone`;
- запрет лишних полей через `additionalProperties: false`.

Запуск:

```bash
docker compose -f docker-compose.yml exec -T mongodb mongosh messenger_api --file /scripts/validation.js
```

Этот же скрипт автоматически выполняется контейнером `mongodb-validation` при запуске `docker compose`. Ручной запуск нужен только для отдельной проверки. Скрипт вставляет корректного временного пользователя, проверяет отклонение некорректных документов и удаляет временные данные.

## HTTP API

| Метод | URL | Назначение |
|---|---|---|
| `GET` | `/api/v1/health` | Проверка состояния сервиса |
| `POST` | `/api/v1/users` | Создание пользователя |
| `GET` | `/api/v1/users?login=...` | Поиск пользователя по логину |
| `GET` | `/api/v1/users?firstNameMask=...&lastNameMask=...` | Поиск пользователя по маске имени и фамилии |
| `POST` | `/api/v1/group-chats` | Создание группового чата |
| `POST` | `/api/v1/group-chats/members` | Добавление пользователя в групповой чат |
| `POST` | `/api/v1/group-messages` | Добавление сообщения в групповой чат |
| `GET` | `/api/v1/group-messages?chatId=...` | Загрузка сообщений группового чата |
| `POST` | `/api/v1/ptp-messages` | Отправка PtP-сообщения пользователю |
| `GET` | `/api/v1/ptp-messages?userId=...` | Получение PtP-сообщений пользователя |

Swagger UI доступен по адресу:

```text
http://localhost:8082
```

## Примеры API-запросов

Поиск пользователя:

```bash
curl "http://localhost:8080/api/v1/users?login=ivan.petrov"
```

Создание пользователя:

```bash
curl -X POST http://localhost:8080/api/v1/users \
  -H "Content-Type: application/json" \
  -d "{\"login\":\"pavel.orlov\",\"firstName\":\"Павел\",\"lastName\":\"Орлов\",\"email\":\"pavel.orlov@example.com\"}"
```

Создание группового чата:

```bash
curl -X POST http://localhost:8080/api/v1/group-chats \
  -H "Content-Type: application/json" \
  -d "{\"name\":\"integrations\",\"description\":\"Интеграции\",\"createdBy\":\"665000000000000000000001\"}"
```

Добавление сообщения в групповой чат:

```bash
curl -X POST http://localhost:8080/api/v1/group-messages \
  -H "Content-Type: application/json" \
  -d "{\"chatId\":\"665000000000000000000101\",\"senderId\":\"665000000000000000000001\",\"text\":\"Сообщение из API\"}"
```

Отправка PtP-сообщения:

```bash
curl -X POST http://localhost:8080/api/v1/ptp-messages \
  -H "Content-Type: application/json" \
  -d "{\"senderId\":\"665000000000000000000001\",\"receiverId\":\"665000000000000000000002\",\"text\":\"Привет\"}"
```

## Быстрая проверка

PowerShell:

```powershell
.\scripts\check_api.ps1
```

bash:

```bash
./scripts/check_api.sh
```

Проверочный скрипт выполняет полный короткий сценарий:

- проверяет `GET /api/v1/health`;
- создает временного пользователя через API;
- находит пользователя по логину через API;
- создает групповой чат через API;
- добавляет сообщение в групповой чат через API;
- читает сообщение обратно через API;
- проверяет, что MongoDB отклоняет документ, нарушающий `$jsonSchema`;
- удаляет временные документы.

Успешный результат:

```text
OK: API, MongoDB connection and schema validation are working
```

## Остановка

```bash
docker compose -f docker-compose.yml down
```
