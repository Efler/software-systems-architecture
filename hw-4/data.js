use("messenger_api");

db.users.drop();
db.group_chats.drop();
db.group_messages.drop();
db.ptp_chats.drop();
db.ptp_messages.drop();

function createCollection(name, validator) {
  db.createCollection(name, {
    validator: { $jsonSchema: validator },
    validationLevel: "strict",
    validationAction: "error"
  });
}

createCollection("users", {
  bsonType: "object",
  required: ["login", "first_name", "last_name", "display_name", "email", "is_active", "profile", "created_at", "updated_at"],
  properties: {
    _id: { bsonType: "objectId" },
    login: { bsonType: "string", minLength: 3, maxLength: 50 },
    first_name: { bsonType: "string", minLength: 1, maxLength: 100 },
    last_name: { bsonType: "string", minLength: 1, maxLength: 100 },
    display_name: { bsonType: "string", minLength: 1, maxLength: 150 },
    email: { bsonType: "string" },
    is_active: { bsonType: "bool" },
    profile: {
      bsonType: "object",
      required: ["position", "department", "timezone", "skills"],
      properties: {
        position: { bsonType: "string" },
        department: { bsonType: "string" },
        timezone: { bsonType: "string" },
        skills: {
          bsonType: "array",
          items: { bsonType: "string" }
        }
      }
    },
    created_at: { bsonType: "date" },
    updated_at: { bsonType: "date" }
  }
});

createCollection("group_chats", {
  bsonType: "object",
  required: ["name", "description", "created_by", "members", "member_count", "settings", "created_at", "updated_at"],
  properties: {
    _id: { bsonType: "objectId" },
    name: { bsonType: "string", minLength: 1, maxLength: 150 },
    description: { bsonType: "string" },
    created_by: { bsonType: "objectId" },
    members: {
      bsonType: "array",
      minItems: 1,
      items: {
        bsonType: "object",
        required: ["user_id", "role", "joined_at", "muted"],
        properties: {
          user_id: { bsonType: "objectId" },
          role: { enum: ["owner", "admin", "member"] },
          joined_at: { bsonType: "date" },
          muted: { bsonType: "bool" }
        }
      }
    },
    member_count: { bsonType: "int", minimum: 1 },
    settings: {
      bsonType: "object",
      required: ["is_private", "allow_threads", "retention_days"],
      properties: {
        is_private: { bsonType: "bool" },
        allow_threads: { bsonType: "bool" },
        retention_days: { bsonType: "int" }
      }
    },
    created_at: { bsonType: "date" },
    updated_at: { bsonType: "date" }
  }
});

createCollection("group_messages", {
  bsonType: "object",
  required: ["chat_id", "sender_id", "text", "sequence_number", "reactions", "attachments", "created_at", "edited_at"],
  properties: {
    _id: { bsonType: "objectId" },
    chat_id: { bsonType: "objectId" },
    sender_id: { bsonType: "objectId" },
    text: { bsonType: "string", minLength: 1 },
    sequence_number: { bsonType: "int", minimum: 1 },
    reactions: {
      bsonType: "array",
      items: {
        bsonType: "object",
        required: ["emoji", "user_ids"],
        properties: {
          emoji: { bsonType: "string" },
          user_ids: {
            bsonType: "array",
            items: { bsonType: "objectId" }
          }
        }
      }
    },
    attachments: {
      bsonType: "array",
      items: {
        bsonType: "object",
        required: ["type", "name", "size_kb"],
        properties: {
          type: { bsonType: "string" },
          name: { bsonType: "string" },
          size_kb: { bsonType: "int" }
        }
      }
    },
    created_at: { bsonType: "date" },
    edited_at: { bsonType: ["date", "null"] }
  }
});

createCollection("ptp_chats", {
  bsonType: "object",
  required: ["participant_ids", "participant_key", "last_message_at", "unread_counts", "created_at", "updated_at"],
  properties: {
    _id: { bsonType: "objectId" },
    participant_ids: {
      bsonType: "array",
      minItems: 2,
      maxItems: 2,
      items: { bsonType: "objectId" }
    },
    participant_key: { bsonType: "string" },
    last_message_at: { bsonType: "date" },
    unread_counts: {
      bsonType: "object",
      additionalProperties: { bsonType: "int" }
    },
    created_at: { bsonType: "date" },
    updated_at: { bsonType: "date" }
  }
});

createCollection("ptp_messages", {
  bsonType: "object",
  required: ["chat_id", "sender_id", "receiver_id", "text", "delivery", "created_at", "read_at"],
  properties: {
    _id: { bsonType: "objectId" },
    chat_id: { bsonType: "objectId" },
    sender_id: { bsonType: "objectId" },
    receiver_id: { bsonType: "objectId" },
    text: { bsonType: "string", minLength: 1 },
    delivery: {
      bsonType: "object",
      required: ["status", "attempts"],
      properties: {
        status: { enum: ["sent", "delivered", "read"] },
        attempts: { bsonType: "int", minimum: 1 }
      }
    },
    created_at: { bsonType: "date" },
    read_at: { bsonType: ["date", "null"] }
  }
});

const u1 = ObjectId("665000000000000000000001");
const u2 = ObjectId("665000000000000000000002");
const u3 = ObjectId("665000000000000000000003");
const u4 = ObjectId("665000000000000000000004");
const u5 = ObjectId("665000000000000000000005");
const u6 = ObjectId("665000000000000000000006");
const u7 = ObjectId("665000000000000000000007");
const u8 = ObjectId("665000000000000000000008");
const u9 = ObjectId("665000000000000000000009");
const u10 = ObjectId("665000000000000000000010");

const g1 = ObjectId("665000000000000000000101");
const g2 = ObjectId("665000000000000000000102");
const g3 = ObjectId("665000000000000000000103");
const g4 = ObjectId("665000000000000000000104");
const g5 = ObjectId("665000000000000000000105");
const g6 = ObjectId("665000000000000000000106");
const g7 = ObjectId("665000000000000000000107");
const g8 = ObjectId("665000000000000000000108");
const g9 = ObjectId("665000000000000000000109");
const g10 = ObjectId("665000000000000000000110");

const p1 = ObjectId("665000000000000000000301");
const p2 = ObjectId("665000000000000000000302");
const p3 = ObjectId("665000000000000000000303");
const p4 = ObjectId("665000000000000000000304");
const p5 = ObjectId("665000000000000000000305");
const p6 = ObjectId("665000000000000000000306");
const p7 = ObjectId("665000000000000000000307");
const p8 = ObjectId("665000000000000000000308");
const p9 = ObjectId("665000000000000000000309");
const p10 = ObjectId("665000000000000000000310");

db.users.insertMany([
  { _id: u1, login: "ivan.petrov", first_name: "Иван", last_name: "Петров", display_name: "Иван Петров", email: "ivan.petrov@example.com", is_active: true, profile: { position: "Backend Developer", department: "Platform", timezone: "Europe/Moscow", skills: ["Python", "MongoDB", "API"] }, created_at: new Date("2026-05-01T08:00:00Z"), updated_at: new Date("2026-05-20T08:00:00Z") },
  { _id: u2, login: "maria.ivanova", first_name: "Мария", last_name: "Иванова", display_name: "Мария Иванова", email: "maria.ivanova@example.com", is_active: true, profile: { position: "Product Manager", department: "Product", timezone: "Europe/Moscow", skills: ["Roadmap", "Analytics"] }, created_at: new Date("2026-05-02T08:00:00Z"), updated_at: new Date("2026-05-20T08:10:00Z") },
  { _id: u3, login: "alex.smirnov", first_name: "Алексей", last_name: "Смирнов", display_name: "Алексей Смирнов", email: "alex.smirnov@example.com", is_active: true, profile: { position: "Frontend Developer", department: "Client Apps", timezone: "Europe/Moscow", skills: ["TypeScript", "React"] }, created_at: new Date("2026-05-03T08:00:00Z"), updated_at: new Date("2026-05-20T08:20:00Z") },
  { _id: u4, login: "olga.novikova", first_name: "Ольга", last_name: "Новикова", display_name: "Ольга Новикова", email: "olga.novikova@example.com", is_active: true, profile: { position: "QA Engineer", department: "Quality", timezone: "Europe/Moscow", skills: ["Testing", "Postman"] }, created_at: new Date("2026-05-04T08:00:00Z"), updated_at: new Date("2026-05-20T08:30:00Z") },
  { _id: u5, login: "dmitry.kozlov", first_name: "Дмитрий", last_name: "Козлов", display_name: "Дмитрий Козлов", email: "dmitry.kozlov@example.com", is_active: true, profile: { position: "DevOps Engineer", department: "Infrastructure", timezone: "Europe/Moscow", skills: ["Docker", "CI/CD", "Linux"] }, created_at: new Date("2026-05-05T08:00:00Z"), updated_at: new Date("2026-05-20T08:40:00Z") },
  { _id: u6, login: "elena.morozova", first_name: "Елена", last_name: "Морозова", display_name: "Елена Морозова", email: "elena.morozova@example.com", is_active: true, profile: { position: "UX Designer", department: "Design", timezone: "Europe/Moscow", skills: ["Figma", "Research"] }, created_at: new Date("2026-05-06T08:00:00Z"), updated_at: new Date("2026-05-20T08:50:00Z") },
  { _id: u7, login: "nikita.volkov", first_name: "Никита", last_name: "Волков", display_name: "Никита Волков", email: "nikita.volkov@example.com", is_active: true, profile: { position: "Data Analyst", department: "Analytics", timezone: "Europe/Moscow", skills: ["SQL", "BI"] }, created_at: new Date("2026-05-07T08:00:00Z"), updated_at: new Date("2026-05-20T09:00:00Z") },
  { _id: u8, login: "anna.sokolova", first_name: "Анна", last_name: "Соколова", display_name: "Анна Соколова", email: "anna.sokolova@example.com", is_active: true, profile: { position: "Support Lead", department: "Support", timezone: "Europe/Moscow", skills: ["Support", "Docs"] }, created_at: new Date("2026-05-08T08:00:00Z"), updated_at: new Date("2026-05-20T09:10:00Z") },
  { _id: u9, login: "sergey.popov", first_name: "Сергей", last_name: "Попов", display_name: "Сергей Попов", email: "sergey.popov@example.com", is_active: false, profile: { position: "Security Engineer", department: "Security", timezone: "Europe/Moscow", skills: ["Audit", "IAM"] }, created_at: new Date("2026-05-09T08:00:00Z"), updated_at: new Date("2026-05-20T09:20:00Z") },
  { _id: u10, login: "tatyana.lebedeva", first_name: "Татьяна", last_name: "Лебедева", display_name: "Татьяна Лебедева", email: "tatyana.lebedeva@example.com", is_active: true, profile: { position: "Team Lead", department: "Platform", timezone: "Europe/Moscow", skills: ["Architecture", "Mentoring"] }, created_at: new Date("2026-05-10T08:00:00Z"), updated_at: new Date("2026-05-20T09:30:00Z") }
]);

db.group_chats.insertMany([
  { _id: g1, name: "platform", description: "Разработка серверной платформы", created_by: u1, members: [{ user_id: u1, role: "owner", joined_at: new Date("2026-05-11T09:00:00Z"), muted: false }, { user_id: u5, role: "admin", joined_at: new Date("2026-05-11T09:05:00Z"), muted: false }, { user_id: u10, role: "member", joined_at: new Date("2026-05-11T09:10:00Z"), muted: false }], member_count: NumberInt(3), settings: { is_private: false, allow_threads: true, retention_days: NumberInt(365) }, created_at: new Date("2026-05-11T09:00:00Z"), updated_at: new Date("2026-05-20T09:00:00Z") },
  { _id: g2, name: "product", description: "Вопросы продукта и дорожной карты", created_by: u2, members: [{ user_id: u2, role: "owner", joined_at: new Date("2026-05-11T10:00:00Z"), muted: false }, { user_id: u6, role: "member", joined_at: new Date("2026-05-11T10:15:00Z"), muted: false }, { user_id: u7, role: "member", joined_at: new Date("2026-05-11T10:20:00Z"), muted: true }], member_count: NumberInt(3), settings: { is_private: false, allow_threads: true, retention_days: NumberInt(180) }, created_at: new Date("2026-05-11T10:00:00Z"), updated_at: new Date("2026-05-20T10:00:00Z") },
  { _id: g3, name: "frontend", description: "Клиентские приложения", created_by: u3, members: [{ user_id: u3, role: "owner", joined_at: new Date("2026-05-12T09:00:00Z"), muted: false }, { user_id: u6, role: "member", joined_at: new Date("2026-05-12T09:30:00Z"), muted: false }], member_count: NumberInt(2), settings: { is_private: false, allow_threads: true, retention_days: NumberInt(365) }, created_at: new Date("2026-05-12T09:00:00Z"), updated_at: new Date("2026-05-20T11:00:00Z") },
  { _id: g4, name: "qa", description: "Тестирование и качество", created_by: u4, members: [{ user_id: u4, role: "owner", joined_at: new Date("2026-05-12T10:00:00Z"), muted: false }, { user_id: u1, role: "member", joined_at: new Date("2026-05-12T10:20:00Z"), muted: false }], member_count: NumberInt(2), settings: { is_private: false, allow_threads: true, retention_days: NumberInt(180) }, created_at: new Date("2026-05-12T10:00:00Z"), updated_at: new Date("2026-05-20T12:00:00Z") },
  { _id: g5, name: "devops", description: "Инфраструктура и релизы", created_by: u5, members: [{ user_id: u5, role: "owner", joined_at: new Date("2026-05-13T09:00:00Z"), muted: false }, { user_id: u1, role: "member", joined_at: new Date("2026-05-13T09:15:00Z"), muted: false }, { user_id: u9, role: "member", joined_at: new Date("2026-05-13T09:20:00Z"), muted: true }], member_count: NumberInt(3), settings: { is_private: true, allow_threads: true, retention_days: NumberInt(90) }, created_at: new Date("2026-05-13T09:00:00Z"), updated_at: new Date("2026-05-20T13:00:00Z") },
  { _id: g6, name: "design", description: "Дизайн интерфейсов", created_by: u6, members: [{ user_id: u6, role: "owner", joined_at: new Date("2026-05-13T10:00:00Z"), muted: false }, { user_id: u2, role: "member", joined_at: new Date("2026-05-13T10:10:00Z"), muted: false }, { user_id: u3, role: "member", joined_at: new Date("2026-05-13T10:15:00Z"), muted: false }], member_count: NumberInt(3), settings: { is_private: false, allow_threads: true, retention_days: NumberInt(180) }, created_at: new Date("2026-05-13T10:00:00Z"), updated_at: new Date("2026-05-20T14:00:00Z") },
  { _id: g7, name: "analytics", description: "Отчеты и продуктовые метрики", created_by: u7, members: [{ user_id: u7, role: "owner", joined_at: new Date("2026-05-14T09:00:00Z"), muted: false }, { user_id: u2, role: "member", joined_at: new Date("2026-05-14T09:10:00Z"), muted: false }], member_count: NumberInt(2), settings: { is_private: true, allow_threads: false, retention_days: NumberInt(365) }, created_at: new Date("2026-05-14T09:00:00Z"), updated_at: new Date("2026-05-20T15:00:00Z") },
  { _id: g8, name: "support", description: "Поддержка пользователей", created_by: u8, members: [{ user_id: u8, role: "owner", joined_at: new Date("2026-05-14T10:00:00Z"), muted: false }, { user_id: u4, role: "member", joined_at: new Date("2026-05-14T10:15:00Z"), muted: false }], member_count: NumberInt(2), settings: { is_private: false, allow_threads: true, retention_days: NumberInt(120) }, created_at: new Date("2026-05-14T10:00:00Z"), updated_at: new Date("2026-05-20T16:00:00Z") },
  { _id: g9, name: "security", description: "Безопасность и доступы", created_by: u9, members: [{ user_id: u9, role: "owner", joined_at: new Date("2026-05-15T09:00:00Z"), muted: false }, { user_id: u5, role: "member", joined_at: new Date("2026-05-15T09:15:00Z"), muted: false }, { user_id: u10, role: "member", joined_at: new Date("2026-05-15T09:20:00Z"), muted: false }], member_count: NumberInt(3), settings: { is_private: true, allow_threads: true, retention_days: NumberInt(730) }, created_at: new Date("2026-05-15T09:00:00Z"), updated_at: new Date("2026-05-20T17:00:00Z") },
  { _id: g10, name: "general", description: "Общие объявления", created_by: u10, members: [{ user_id: u1, role: "member", joined_at: new Date("2026-05-15T10:00:00Z"), muted: false }, { user_id: u2, role: "member", joined_at: new Date("2026-05-15T10:00:00Z"), muted: false }, { user_id: u3, role: "member", joined_at: new Date("2026-05-15T10:00:00Z"), muted: false }, { user_id: u10, role: "owner", joined_at: new Date("2026-05-15T10:00:00Z"), muted: false }], member_count: NumberInt(4), settings: { is_private: false, allow_threads: false, retention_days: NumberInt(365) }, created_at: new Date("2026-05-15T10:00:00Z"), updated_at: new Date("2026-05-20T18:00:00Z") }
]);

db.group_messages.insertMany([
  { _id: ObjectId("665000000000000000000201"), chat_id: g1, sender_id: u1, text: "Подготовил черновик API для сообщений.", sequence_number: NumberInt(1), reactions: [{ emoji: "+1", user_ids: [u5, u10] }], attachments: [], created_at: new Date("2026-05-20T09:00:00Z"), edited_at: null },
  { _id: ObjectId("665000000000000000000202"), chat_id: g2, sender_id: u2, text: "Сегодня обновляем приоритеты по спринту.", sequence_number: NumberInt(1), reactions: [], attachments: [], created_at: new Date("2026-05-20T10:00:00Z"), edited_at: null },
  { _id: ObjectId("665000000000000000000203"), chat_id: g3, sender_id: u3, text: "Добавил обработку пустого состояния в списке чатов.", sequence_number: NumberInt(1), reactions: [{ emoji: "ok", user_ids: [u6] }], attachments: [{ type: "image", name: "empty-state.png", size_kb: NumberInt(248) }], created_at: new Date("2026-05-20T11:00:00Z"), edited_at: null },
  { _id: ObjectId("665000000000000000000204"), chat_id: g4, sender_id: u4, text: "Проверила сценарий добавления пользователя в чат.", sequence_number: NumberInt(1), reactions: [], attachments: [], created_at: new Date("2026-05-20T12:00:00Z"), edited_at: null },
  { _id: ObjectId("665000000000000000000205"), chat_id: g5, sender_id: u5, text: "MongoDB контейнер поднят на отдельном порту.", sequence_number: NumberInt(1), reactions: [{ emoji: "rocket", user_ids: [u1] }], attachments: [], created_at: new Date("2026-05-20T13:00:00Z"), edited_at: null },
  { _id: ObjectId("665000000000000000000206"), chat_id: g6, sender_id: u6, text: "Макет формы поиска пользователя готов.", sequence_number: NumberInt(1), reactions: [], attachments: [{ type: "file", name: "search-form.fig", size_kb: NumberInt(1024) }], created_at: new Date("2026-05-20T14:00:00Z"), edited_at: null },
  { _id: ObjectId("665000000000000000000207"), chat_id: g7, sender_id: u7, text: "Выгрузил метрики активности по чатам.", sequence_number: NumberInt(1), reactions: [], attachments: [{ type: "file", name: "chat-metrics.xlsx", size_kb: NumberInt(87) }], created_at: new Date("2026-05-20T15:00:00Z"), edited_at: null },
  { _id: ObjectId("665000000000000000000208"), chat_id: g8, sender_id: u8, text: "В поддержку пришел вопрос по личным сообщениям.", sequence_number: NumberInt(1), reactions: [], attachments: [], created_at: new Date("2026-05-20T16:00:00Z"), edited_at: null },
  { _id: ObjectId("665000000000000000000209"), chat_id: g9, sender_id: u9, text: "Проверяем доступы к приватным каналам.", sequence_number: NumberInt(1), reactions: [{ emoji: "lock", user_ids: [u5, u10] }], attachments: [], created_at: new Date("2026-05-20T17:00:00Z"), edited_at: null },
  { _id: ObjectId("665000000000000000000210"), chat_id: g10, sender_id: u10, text: "В пятницу короткая синхронизация команды.", sequence_number: NumberInt(1), reactions: [{ emoji: "calendar", user_ids: [u1, u2, u3] }], attachments: [], created_at: new Date("2026-05-20T18:00:00Z"), edited_at: null }
]);

db.ptp_chats.insertMany([
  { _id: p1, participant_ids: [u1, u2], participant_key: "665000000000000000000001:665000000000000000000002", last_message_at: new Date("2026-05-21T09:00:00Z"), unread_counts: { "665000000000000000000001": NumberInt(0), "665000000000000000000002": NumberInt(1) }, created_at: new Date("2026-05-21T08:55:00Z"), updated_at: new Date("2026-05-21T09:00:00Z") },
  { _id: p2, participant_ids: [u1, u3], participant_key: "665000000000000000000001:665000000000000000000003", last_message_at: new Date("2026-05-21T09:10:00Z"), unread_counts: { "665000000000000000000001": NumberInt(1), "665000000000000000000003": NumberInt(0) }, created_at: new Date("2026-05-21T09:05:00Z"), updated_at: new Date("2026-05-21T09:10:00Z") },
  { _id: p3, participant_ids: [u2, u4], participant_key: "665000000000000000000002:665000000000000000000004", last_message_at: new Date("2026-05-21T09:20:00Z"), unread_counts: { "665000000000000000000002": NumberInt(0), "665000000000000000000004": NumberInt(0) }, created_at: new Date("2026-05-21T09:15:00Z"), updated_at: new Date("2026-05-21T09:20:00Z") },
  { _id: p4, participant_ids: [u3, u6], participant_key: "665000000000000000000003:665000000000000000000006", last_message_at: new Date("2026-05-21T09:30:00Z"), unread_counts: { "665000000000000000000003": NumberInt(0), "665000000000000000000006": NumberInt(1) }, created_at: new Date("2026-05-21T09:25:00Z"), updated_at: new Date("2026-05-21T09:30:00Z") },
  { _id: p5, participant_ids: [u4, u8], participant_key: "665000000000000000000004:665000000000000000000008", last_message_at: new Date("2026-05-21T09:40:00Z"), unread_counts: { "665000000000000000000004": NumberInt(0), "665000000000000000000008": NumberInt(1) }, created_at: new Date("2026-05-21T09:35:00Z"), updated_at: new Date("2026-05-21T09:40:00Z") },
  { _id: p6, participant_ids: [u5, u9], participant_key: "665000000000000000000005:665000000000000000000009", last_message_at: new Date("2026-05-21T09:50:00Z"), unread_counts: { "665000000000000000000005": NumberInt(0), "665000000000000000000009": NumberInt(0) }, created_at: new Date("2026-05-21T09:45:00Z"), updated_at: new Date("2026-05-21T09:50:00Z") },
  { _id: p7, participant_ids: [u5, u10], participant_key: "665000000000000000000005:665000000000000000000010", last_message_at: new Date("2026-05-21T10:00:00Z"), unread_counts: { "665000000000000000000005": NumberInt(1), "665000000000000000000010": NumberInt(0) }, created_at: new Date("2026-05-21T09:55:00Z"), updated_at: new Date("2026-05-21T10:00:00Z") },
  { _id: p8, participant_ids: [u6, u7], participant_key: "665000000000000000000006:665000000000000000000007", last_message_at: new Date("2026-05-21T10:10:00Z"), unread_counts: { "665000000000000000000006": NumberInt(0), "665000000000000000000007": NumberInt(1) }, created_at: new Date("2026-05-21T10:05:00Z"), updated_at: new Date("2026-05-21T10:10:00Z") },
  { _id: p9, participant_ids: [u7, u8], participant_key: "665000000000000000000007:665000000000000000000008", last_message_at: new Date("2026-05-21T10:20:00Z"), unread_counts: { "665000000000000000000007": NumberInt(0), "665000000000000000000008": NumberInt(0) }, created_at: new Date("2026-05-21T10:15:00Z"), updated_at: new Date("2026-05-21T10:20:00Z") },
  { _id: p10, participant_ids: [u9, u10], participant_key: "665000000000000000000009:665000000000000000000010", last_message_at: new Date("2026-05-21T10:30:00Z"), unread_counts: { "665000000000000000000009": NumberInt(1), "665000000000000000000010": NumberInt(0) }, created_at: new Date("2026-05-21T10:25:00Z"), updated_at: new Date("2026-05-21T10:30:00Z") }
]);

db.ptp_messages.insertMany([
  { _id: ObjectId("665000000000000000000401"), chat_id: p1, sender_id: u1, receiver_id: u2, text: "Мария, посмотри требования к групповым чатам.", delivery: { status: "delivered", attempts: NumberInt(1) }, created_at: new Date("2026-05-21T09:00:00Z"), read_at: null },
  { _id: ObjectId("665000000000000000000402"), chat_id: p2, sender_id: u3, receiver_id: u1, text: "Иван, нужна структура ответа для поиска.", delivery: { status: "sent", attempts: NumberInt(1) }, created_at: new Date("2026-05-21T09:10:00Z"), read_at: null },
  { _id: ObjectId("665000000000000000000403"), chat_id: p3, sender_id: u2, receiver_id: u4, text: "Ольга, проверь сценарии по PtP.", delivery: { status: "read", attempts: NumberInt(1) }, created_at: new Date("2026-05-21T09:20:00Z"), read_at: new Date("2026-05-21T09:25:00Z") },
  { _id: ObjectId("665000000000000000000404"), chat_id: p4, sender_id: u3, receiver_id: u6, text: "Елена, отправил экран личного чата.", delivery: { status: "delivered", attempts: NumberInt(1) }, created_at: new Date("2026-05-21T09:30:00Z"), read_at: null },
  { _id: ObjectId("665000000000000000000405"), chat_id: p5, sender_id: u4, receiver_id: u8, text: "Анна, нужен пример обращения в поддержку.", delivery: { status: "delivered", attempts: NumberInt(2) }, created_at: new Date("2026-05-21T09:40:00Z"), read_at: null },
  { _id: ObjectId("665000000000000000000406"), chat_id: p6, sender_id: u5, receiver_id: u9, text: "Сергей, проверь доступ к приватному каналу.", delivery: { status: "read", attempts: NumberInt(1) }, created_at: new Date("2026-05-21T09:50:00Z"), read_at: new Date("2026-05-21T09:55:00Z") },
  { _id: ObjectId("665000000000000000000407"), chat_id: p7, sender_id: u10, receiver_id: u5, text: "Дмитрий, контейнер MongoDB оставляем на 27018.", delivery: { status: "sent", attempts: NumberInt(1) }, created_at: new Date("2026-05-21T10:00:00Z"), read_at: null },
  { _id: ObjectId("665000000000000000000408"), chat_id: p8, sender_id: u6, receiver_id: u7, text: "Никита, добавь в отчет активность по каналам.", delivery: { status: "delivered", attempts: NumberInt(1) }, created_at: new Date("2026-05-21T10:10:00Z"), read_at: null },
  { _id: ObjectId("665000000000000000000409"), chat_id: p9, sender_id: u7, receiver_id: u8, text: "Анна, выгрузка по обращениям готова.", delivery: { status: "read", attempts: NumberInt(1) }, created_at: new Date("2026-05-21T10:20:00Z"), read_at: new Date("2026-05-21T10:22:00Z") },
  { _id: ObjectId("665000000000000000000410"), chat_id: p10, sender_id: u10, receiver_id: u9, text: "Сергей, согласуй список ролей для чатов.", delivery: { status: "sent", attempts: NumberInt(1) }, created_at: new Date("2026-05-21T10:30:00Z"), read_at: null }
]);

db.users.createIndex({ login: 1 }, { unique: true });
db.users.createIndex({ email: 1 }, { unique: true });
db.users.createIndex({ first_name: 1, last_name: 1 });

db.group_chats.createIndex({ "members.user_id": 1 });
db.group_messages.createIndex({ chat_id: 1, created_at: -1 });
db.group_messages.createIndex({ sender_id: 1 });

db.ptp_chats.createIndex({ participant_ids: 1 });
db.ptp_chats.createIndex({ participant_key: 1 }, { unique: true });
db.ptp_messages.createIndex({ chat_id: 1, created_at: -1 });
db.ptp_messages.createIndex({ sender_id: 1, created_at: -1 });
db.ptp_messages.createIndex({ receiver_id: 1, created_at: -1 });

print("Database messenger_api initialized");
print("users: " + db.users.countDocuments());
print("group_chats: " + db.group_chats.countDocuments());
print("group_messages: " + db.group_messages.countDocuments());
print("ptp_chats: " + db.ptp_chats.countDocuments());
print("ptp_messages: " + db.ptp_messages.countDocuments());
