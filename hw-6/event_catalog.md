# Каталог событий

## Общий формат события

```json
{
  "event_id": "string",
  "event_type": "string",
  "occurred_at": "datetime",
  "producer": "string",
  "payload": {}
}
```

| Поле | Тип | Назначение |
|---|---|---|
| `event_id` | `string` | Уникальный идентификатор события |
| `event_type` | `string` | Название события |
| `occurred_at` | `datetime` | Время формирования события |
| `producer` | `string` | Сервис, который опубликовал событие |
| `payload` | `object` | Данные события |

## `UserCreatedEvent`

| Параметр | Значение |
|---|---|
| Название события | `UserCreatedEvent` |
| Routing key | `user.created` |
| Производитель | `User Service` |
| Потребители | `Notification Service`, `Analytics Service` |
| Гарантия доставки | `at-least-once` |

### Payload

| Поле | Тип | Назначение |
|---|---|---|
| `user_id` | `string` | Идентификатор пользователя |
| `login` | `string` | Логин пользователя |
| `first_name` | `string` | Имя пользователя |
| `last_name` | `string` | Фамилия пользователя |
| `email` | `string` | Email пользователя |
| `created_at` | `datetime` | Время создания пользователя |

```json
{
  "user_id": "string",
  "login": "string",
  "first_name": "string",
  "last_name": "string",
  "email": "string",
  "created_at": "datetime"
}
```

## `EventCreatedEvent`

| Параметр | Значение |
|---|---|
| Название события | `EventCreatedEvent` |
| Routing key | `event.created` |
| Производитель | `Event Service` |
| Потребители | `Notification Service`, `Analytics Service` |
| Гарантия доставки | `at-least-once` |

### Payload

| Поле | Тип | Назначение |
|---|---|---|
| `event_id` | `string` | Идентификатор события |
| `title` | `string` | Название события |
| `description` | `string` | Описание события |
| `event_date` | `datetime` | Дата и время проведения |
| `location` | `string` | Место проведения |
| `owner_user_id` | `string` | Идентификатор создателя события |
| `created_at` | `datetime` | Время создания события |

```json
{
  "event_id": "string",
  "title": "string",
  "description": "string",
  "event_date": "datetime",
  "location": "string",
  "owner_user_id": "string",
  "created_at": "datetime"
}
```

## `UserRegisteredForEventEvent`

| Параметр | Значение |
|---|---|
| Название события | `UserRegisteredForEventEvent` |
| Routing key | `registration.created` |
| Производитель | `Registration Service` |
| Потребители | `Notification Service`, `Event Service`, `Analytics Service` |
| Гарантия доставки | `at-least-once` |

### Payload

| Поле | Тип | Назначение |
|---|---|---|
| `registration_id` | `string` | Идентификатор регистрации |
| `event_id` | `string` | Идентификатор события |
| `user_id` | `string` | Идентификатор пользователя |
| `registered_at` | `datetime` | Время регистрации |
| `status` | `string` | Статус регистрации, значение `registered` |

```json
{
  "registration_id": "string",
  "event_id": "string",
  "user_id": "string",
  "registered_at": "datetime",
  "status": "registered"
}
```

## `EventRegistrationCancelledEvent`

| Параметр | Значение |
|---|---|
| Название события | `EventRegistrationCancelledEvent` |
| Routing key | `registration.cancelled` |
| Производитель | `Registration Service` |
| Потребители | `Notification Service`, `Event Service`, `Analytics Service` |
| Гарантия доставки | `at-least-once` |

### Payload

| Поле | Тип | Назначение |
|---|---|---|
| `registration_id` | `string` | Идентификатор регистрации |
| `event_id` | `string` | Идентификатор события |
| `user_id` | `string` | Идентификатор пользователя |
| `cancelled_at` | `datetime` | Время отмены регистрации |
| `status` | `string` | Статус регистрации, значение `cancelled` |

```json
{
  "registration_id": "string",
  "event_id": "string",
  "user_id": "string",
  "cancelled_at": "datetime",
  "status": "cancelled"
}
```
