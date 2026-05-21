# Описание Event-Driven архитектуры

## Назначение системы

Система управления событиями позволяет создавать пользователей, публиковать события и регистрировать пользователей на события.

Основные сущности:

- **Пользователь** - зарегистрированный пользователь системы.
- **Событие** - мероприятие, созданное пользователем.
- **Участник** - пользователь, зарегистрированный на событие.

## Операции API

| Операция | Тип | Команда или запрос |
|---|---|---|
| Создание нового пользователя | Write | `CreateUserCommand` |
| Поиск пользователя по логину | Read | `FindUserByLoginQuery` |
| Поиск пользователя по имени и фамилии | Read | `SearchUsersByNameQuery` |
| Создание события | Write | `CreateEventCommand` |
| Получение списка событий | Read | `GetEventsQuery` |
| Поиск событий по дате | Read | `FindEventsByDateQuery` |
| Регистрация пользователя на событие | Write | `RegisterUserForEventCommand` |
| Получение участников события | Read | `GetEventParticipantsQuery` |
| Получение событий пользователя | Read | `GetUserEventsQuery` |
| Отмена регистрации на событие | Write | `CancelEventRegistrationCommand` |

## Компоненты системы

| Компонент | Назначение | Роль в обмене событиями |
|---|---|---|
| **User Service** | Работа с пользователями | Producer |
| **Event Service** | Работа с событиями | Producer, Consumer |
| **Registration Service** | Работа с регистрациями | Producer |
| **Notification Service** | Отправка уведомлений | Consumer |
| **Analytics Service** | Сбор статистики | Consumer |
| **RabbitMQ** | Брокер сообщений | Передача событий между сервисами |

## События и команды

| Команда | Событие | Producer | Consumers |
|---|---|---|---|
| `CreateUserCommand` | `UserCreatedEvent` | User Service | Notification Service, Analytics Service |
| `CreateEventCommand` | `EventCreatedEvent` | Event Service | Notification Service, Analytics Service |
| `RegisterUserForEventCommand` | `UserRegisteredForEventEvent` | Registration Service | Notification Service, Event Service, Analytics Service |
| `CancelEventRegistrationCommand` | `EventRegistrationCancelledEvent` | Registration Service | Notification Service, Event Service, Analytics Service |

Read-операции не публикуют события, так как они не изменяют состояние системы.

## RabbitMQ

Для обмена событиями выбран **RabbitMQ**.

| Элемент | Значение |
|---|---|
| Exchange | `events.exchange` |
| Тип exchange | `topic` |
| Назначение | Маршрутизация доменных событий по routing key |
| Гарантия доставки | `at-least-once` |

Тип `topic` подходит для системы, потому что события относятся к разным частям предметной области: пользователи, события и регистрации. Потребители могут подписываться на конкретные routing key или на группу событий.

## Routing

| Routing key | Событие |
|---|---|
| `user.created` | `UserCreatedEvent` |
| `event.created` | `EventCreatedEvent` |
| `registration.created` | `UserRegisteredForEventEvent` |
| `registration.cancelled` | `EventRegistrationCancelledEvent` |

## Очереди потребителей

| Очередь | Routing key | Потребитель |
|---|---|---|
| `notification.user.created.queue` | `user.created` | Notification Service |
| `notification.event.created.queue` | `event.created` | Notification Service |
| `notification.registration.created.queue` | `registration.created` | Notification Service |
| `notification.registration.cancelled.queue` | `registration.cancelled` | Notification Service |
| `event.registration.created.queue` | `registration.created` | Event Service |
| `event.registration.cancelled.queue` | `registration.cancelled` | Event Service |
| `analytics.all.queue` | `#` | Analytics Service |

В реализации используется один consumer-сервис с очередью `event-driven.events.queue`. Она привязана к exchange `events.exchange` по routing key `user.created`, `event.created`, `registration.created`, `registration.cancelled` и показывает обработку всех событий в одном месте.

## Формат сообщения

```json
{
  "event_id": "string",
  "event_type": "string",
  "occurred_at": "datetime",
  "producer": "string",
  "payload": {}
}
```

| Поле | Назначение |
|---|---|
| `event_id` | Уникальный идентификатор события |
| `event_type` | Название события |
| `occurred_at` | Время создания события |
| `producer` | Сервис, опубликовавший событие |
| `payload` | Данные конкретного события |

## Потоки событий

### Создание пользователя

| Шаг | Действие |
|---|---|
| 1 | Клиент отправляет `CreateUserCommand` в User Service |
| 2 | User Service создает пользователя |
| 3 | User Service публикует `UserCreatedEvent` с routing key `user.created` |
| 4 | Notification Service отправляет приветственное уведомление |
| 5 | Analytics Service обновляет статистику пользователей |

### Создание события

| Шаг | Действие |
|---|---|
| 1 | Клиент отправляет `CreateEventCommand` в Event Service |
| 2 | Event Service создает событие |
| 3 | Event Service публикует `EventCreatedEvent` с routing key `event.created` |
| 4 | Notification Service отправляет уведомление создателю события |
| 5 | Analytics Service обновляет статистику событий |

### Регистрация пользователя на событие

| Шаг | Действие |
|---|---|
| 1 | Клиент отправляет `RegisterUserForEventCommand` в Registration Service |
| 2 | Registration Service создает регистрацию |
| 3 | Registration Service публикует `UserRegisteredForEventEvent` с routing key `registration.created` |
| 4 | Notification Service отправляет подтверждение регистрации |
| 5 | Event Service обновляет количество участников |
| 6 | Analytics Service обновляет статистику регистраций |

### Отмена регистрации

| Шаг | Действие |
|---|---|
| 1 | Клиент отправляет `CancelEventRegistrationCommand` в Registration Service |
| 2 | Registration Service отменяет регистрацию |
| 3 | Registration Service публикует `EventRegistrationCancelledEvent` с routing key `registration.cancelled` |
| 4 | Notification Service отправляет подтверждение отмены |
| 5 | Event Service обновляет количество участников |
| 6 | Analytics Service обновляет статистику отмен |

## Гарантии доставки

Используется гарантия **at-least-once**.

| Механизм | Назначение |
|---|---|
| Durable exchange | Exchange сохраняется после перезапуска RabbitMQ |
| Durable queues | Очереди сохраняются после перезапуска RabbitMQ |
| Persistent messages | Сообщения сохраняются на диск |
| Manual ack | Сообщение подтверждается после успешной обработки |
| Retry | При ошибке сообщение может быть обработано повторно |

При `at-least-once` событие может быть доставлено повторно. Потребители должны учитывать `event_id` и обрабатывать события идемпотентно.

## CQRS

CQRS применим, потому что операции изменения данных и операции чтения имеют разные задачи.

### Write-модель

| Команда | Сервис | Результирующее событие |
|---|---|---|
| `CreateUserCommand` | User Service | `UserCreatedEvent` |
| `CreateEventCommand` | Event Service | `EventCreatedEvent` |
| `RegisterUserForEventCommand` | Registration Service | `UserRegisteredForEventEvent` |
| `CancelEventRegistrationCommand` | Registration Service | `EventRegistrationCancelledEvent` |

### Read-модель

| Запрос | Read-модель |
|---|---|
| `FindUserByLoginQuery` | `UserReadModel` |
| `SearchUsersByNameQuery` | `UserReadModel` |
| `GetEventsQuery` | `EventReadModel` |
| `FindEventsByDateQuery` | `EventReadModel` |
| `GetEventParticipantsQuery` | `EventParticipantsReadModel` |
| `GetUserEventsQuery` | `UserEventsReadModel` |

### Синхронизация моделей

| Событие | Обновление read-модели |
|---|---|
| `UserCreatedEvent` | Создается запись в `UserReadModel` |
| `EventCreatedEvent` | Создается запись в `EventReadModel` |
| `UserRegisteredForEventEvent` | Обновляются `EventReadModel`, `EventParticipantsReadModel`, `UserEventsReadModel` |
| `EventRegistrationCancelledEvent` | Обновляются `EventReadModel`, `EventParticipantsReadModel`, `UserEventsReadModel` |

Read-модель обновляется асинхронно обработчиками событий. Для данных чтения используется eventual consistency.
