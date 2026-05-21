# Проектирование Event-Driven архитектуры

## Состав проекта

| Файл или каталог | Назначение |
|---|---|
| `event_driven_design.md` | Описание Event-Driven архитектуры |
| `event_catalog.md` | Каталог событий |
| `src/producer.cpp` | Producer-сервис на userver |
| `src/consumer.cpp` | Consumer-сервис на userver |
| `docker-compose.yml` | Запуск RabbitMQ, producer и consumer |
| `configs/` | Конфигурация сервисов userver |
| `scripts/test.ps1` | Быстрая проверка публикации и обработки событий |

## Компоненты

| Компонент | Назначение | Адрес |
|---|---|---|
| `rabbitmq` | Брокер сообщений RabbitMQ | `localhost:5672`, UI `http://localhost:15672` |
| `producer` | Публикует события в RabbitMQ | `http://localhost:8080` |
| `consumer` | Читает события из RabbitMQ | `http://localhost:8081` |

## События

| HTTP endpoint producer | Routing key | Событие |
|---|---|---|
| `POST /users` | `user.created` | `UserCreatedEvent` |
| `POST /events` | `event.created` | `EventCreatedEvent` |
| `POST /events/register` | `registration.created` | `UserRegisteredForEventEvent` |
| `POST /events/cancel` | `registration.cancelled` | `EventRegistrationCancelledEvent` |

Consumer читает события из очереди `event-driven.events.queue`.

## Запуск

Собрать и запустить:

```bash
docker compose up --build
```

RabbitMQ Management UI:

```text
http://localhost:15672
login: guest
password: guest
```

## Быстрая проверка через скрипт

В отдельном терминале:

```powershell
.\scripts\test.ps1
```

Скрипт публикует события и затем запрашивает обработанные сообщения у consumer. В конце вывода должен быть список `events` с событиями:

- `UserCreatedEvent`;
- `EventCreatedEvent`;
- `UserRegisteredForEventEvent`;
- `EventRegistrationCancelledEvent`.

## Ручная проверка (у меня Windows, для powershell)

Создать пользователя:

```powershell
Invoke-RestMethod -Method Post -Uri "http://localhost:8080/users" -ContentType "application/json" -Body '{"user_id":"user-1","login":"ivanov","first_name":"Ivan","last_name":"Ivanov","email":"ivanov@example.com"}'
```

Создать событие:

```powershell
Invoke-RestMethod -Method Post -Uri "http://localhost:8080/events" -ContentType "application/json" -Body '{"event_id":"event-100","title":"Python Meetup","description":"Meeting for Python developers","event_date":"2026-06-10T18:00:00Z","location":"Moscow","owner_user_id":"user-1"}'
```

Зарегистрировать пользователя на событие:

```powershell
Invoke-RestMethod -Method Post -Uri "http://localhost:8080/events/register" -ContentType "application/json" -Body '{"user_id":"user-1","event_id":"event-100"}'
```

Отменить регистрацию:

```powershell
Invoke-RestMethod -Method Post -Uri "http://localhost:8080/events/cancel" -ContentType "application/json" -Body '{"user_id":"user-1","event_id":"event-100"}'
```

Посмотреть обработанные consumer события:

```powershell
Invoke-RestMethod -Method Get -Uri "http://localhost:8081/events/processed" | ConvertTo-Json -Depth 10
```

## Остановка

```bash
docker compose down
```
