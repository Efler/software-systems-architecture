use("messenger_api");

const tempValidUserId = ObjectId("665000000000000000009501");

function section(title) {
  print("\n=== " + title + " ===");
}

function tryInsert(title, document) {
  print(title);
  try {
    const result = db.users.insertOne(document);
    printjson({
      ok: true,
      insertedId: result.insertedId
    });
  } catch (error) {
    printjson({
      ok: false,
      error: error.message.substring(0, 180)
    });
  }
}

db.users.deleteOne({ _id: tempValidUserId });

section("Apply $jsonSchema validation");

printjson(db.runCommand({
  collMod: "users",
  validator: {
    $jsonSchema: {
      bsonType: "object",
      required: [
        "login",
        "first_name",
        "last_name",
        "display_name",
        "email",
        "is_active",
        "profile",
        "created_at",
        "updated_at"
      ],
      additionalProperties: false,
      properties: {
        _id: {
          bsonType: "objectId"
        },
        login: {
          bsonType: "string",
          minLength: 3,
          maxLength: 50,
          pattern: "^[a-z0-9]+([._-][a-z0-9]+)*$",
          description: "login must contain lowercase latin letters, digits, dots, underscores or hyphens"
        },
        first_name: {
          bsonType: "string",
          minLength: 1,
          maxLength: 100
        },
        last_name: {
          bsonType: "string",
          minLength: 1,
          maxLength: 100
        },
        display_name: {
          bsonType: "string",
          minLength: 1,
          maxLength: 150
        },
        email: {
          bsonType: "string",
          pattern: "^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$",
          description: "email must match a basic email pattern"
        },
        is_active: {
          bsonType: "bool"
        },
        profile: {
          bsonType: "object",
          required: ["position", "department", "timezone", "skills"],
          additionalProperties: false,
          properties: {
            position: {
              bsonType: "string",
              minLength: 1,
              maxLength: 100
            },
            department: {
              bsonType: "string",
              minLength: 1,
              maxLength: 100
            },
            timezone: {
              bsonType: "string",
              pattern: "^[A-Za-z]+/[A-Za-z_]+$"
            },
            skills: {
              bsonType: "array",
              minItems: 1,
              maxItems: 10,
              items: {
                bsonType: "string",
                minLength: 1,
                maxLength: 50
              }
            }
          }
        },
        created_at: {
          bsonType: "date"
        },
        updated_at: {
          bsonType: "date"
        }
      }
    }
  },
  validationLevel: "strict",
  validationAction: "error"
}));

section("Validation tests");

tryInsert("Valid user must be inserted", {
  _id: tempValidUserId,
  login: "valid.user",
  first_name: "Валидный",
  last_name: "Пользователь",
  display_name: "Валидный Пользователь",
  email: "valid.user@example.com",
  is_active: true,
  profile: {
    position: "QA Engineer",
    department: "Quality",
    timezone: "Europe/Moscow",
    skills: ["Testing", "MongoDB"]
  },
  created_at: new Date("2026-05-25T10:00:00Z"),
  updated_at: new Date("2026-05-25T10:00:00Z")
});

tryInsert("Invalid user without required email must be rejected", {
  login: "no.email",
  first_name: "Нет",
  last_name: "Почты",
  display_name: "Нет Почты",
  is_active: true,
  profile: {
    position: "Developer",
    department: "Platform",
    timezone: "Europe/Moscow",
    skills: ["API"]
  },
  created_at: new Date(),
  updated_at: new Date()
});

tryInsert("Invalid user with wrong login pattern must be rejected", {
  login: "Bad Login!",
  first_name: "Плохой",
  last_name: "Логин",
  display_name: "Плохой Логин",
  email: "bad.login@example.com",
  is_active: true,
  profile: {
    position: "Developer",
    department: "Platform",
    timezone: "Europe/Moscow",
    skills: ["API"]
  },
  created_at: new Date(),
  updated_at: new Date()
});

tryInsert("Invalid user with empty skills array must be rejected", {
  login: "empty.skills",
  first_name: "Нет",
  last_name: "Навыков",
  display_name: "Нет Навыков",
  email: "empty.skills@example.com",
  is_active: true,
  profile: {
    position: "Developer",
    department: "Platform",
    timezone: "Europe/Moscow",
    skills: []
  },
  created_at: new Date(),
  updated_at: new Date()
});

tryInsert("Invalid user with extra field must be rejected", {
  login: "extra.field",
  first_name: "Лишнее",
  last_name: "Поле",
  display_name: "Лишнее Поле",
  email: "extra.field@example.com",
  is_active: true,
  profile: {
    position: "Developer",
    department: "Platform",
    timezone: "Europe/Moscow",
    skills: ["API"]
  },
  created_at: new Date(),
  updated_at: new Date(),
  unexpected_field: "not allowed"
});

db.users.deleteOne({ _id: tempValidUserId });

section("Final count");
printjson({
  users: db.users.countDocuments()
});
