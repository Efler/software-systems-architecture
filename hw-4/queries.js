use("messenger_api");

const u1 = ObjectId("665000000000000000000001");
const u2 = ObjectId("665000000000000000000002");
const u3 = ObjectId("665000000000000000000003");
const u4 = ObjectId("665000000000000000000004");
const u6 = ObjectId("665000000000000000000006");

const g1 = ObjectId("665000000000000000000101");
const g2 = ObjectId("665000000000000000000102");
const g3 = ObjectId("665000000000000000000103");

const tempUserId = ObjectId("665000000000000000009001");
const tempGroupChatId = ObjectId("665000000000000000009101");
const tempGroupMessageId = ObjectId("665000000000000000009201");
const tempPtpChatId = ObjectId("665000000000000000009301");
const tempPtpMessageId = ObjectId("665000000000000000009401");
const tempParticipantKey = "665000000000000000000001:665000000000000000009001";

function section(title) {
  print("\n=== " + title + " ===");
}

function cleanupTempData() {
  db.group_messages.deleteMany({ _id: { $in: [tempGroupMessageId] } });
  db.ptp_messages.deleteMany({ _id: { $in: [tempPtpMessageId] } });
  db.group_chats.deleteMany({ _id: tempGroupChatId });
  db.ptp_chats.deleteMany({ _id: tempPtpChatId });
  db.users.deleteMany({ _id: tempUserId });
  db.group_chats.updateOne(
    { _id: g1 },
    {
      $pull: { members: { user_id: tempUserId } },
      $set: { member_count: NumberInt(3) }
    }
  );
}

cleanupTempData();

section("Create");

print("Create user");
printjson(db.users.insertOne({
  _id: tempUserId,
  login: "pavel.orlov",
  first_name: "Павел",
  last_name: "Орлов",
  display_name: "Павел Орлов",
  email: "pavel.orlov@example.com",
  is_active: true,
  profile: {
    position: "Integration Engineer",
    department: "Platform",
    timezone: "Europe/Moscow",
    skills: ["MongoDB", "REST", "Docker"]
  },
  created_at: new Date("2026-05-25T09:00:00Z"),
  updated_at: new Date("2026-05-25T09:00:00Z")
}));

print("Create group chat");
printjson(db.group_chats.insertOne({
  _id: tempGroupChatId,
  name: "integrations",
  description: "Интеграции с внешними сервисами",
  created_by: tempUserId,
  members: [
    {
      user_id: tempUserId,
      role: "owner",
      joined_at: new Date("2026-05-25T09:05:00Z"),
      muted: false
    },
    {
      user_id: u1,
      role: "member",
      joined_at: new Date("2026-05-25T09:07:00Z"),
      muted: false
    }
  ],
  member_count: NumberInt(2),
  settings: {
    is_private: false,
    allow_threads: true,
    retention_days: NumberInt(180)
  },
  created_at: new Date("2026-05-25T09:05:00Z"),
  updated_at: new Date("2026-05-25T09:05:00Z")
}));

print("Create group message");
printjson(db.group_messages.insertOne({
  _id: tempGroupMessageId,
  chat_id: tempGroupChatId,
  sender_id: tempUserId,
  text: "Создал чат для обсуждения интеграций.",
  sequence_number: NumberInt(1),
  reactions: [],
  attachments: [
    {
      type: "file",
      name: "integration-plan.md",
      size_kb: NumberInt(12)
    }
  ],
  created_at: new Date("2026-05-25T09:10:00Z"),
  edited_at: null
}));

print("Create PtP chat");
printjson(db.ptp_chats.insertOne({
  _id: tempPtpChatId,
  participant_ids: [u1, tempUserId],
  participant_key: tempParticipantKey,
  last_message_at: new Date("2026-05-25T09:20:00Z"),
  unread_counts: {
    "665000000000000000000001": NumberInt(1),
    "665000000000000000009001": NumberInt(0)
  },
  created_at: new Date("2026-05-25T09:15:00Z"),
  updated_at: new Date("2026-05-25T09:20:00Z")
}));

print("Create PtP message");
printjson(db.ptp_messages.insertOne({
  _id: tempPtpMessageId,
  chat_id: tempPtpChatId,
  sender_id: tempUserId,
  receiver_id: u1,
  text: "Иван, добавил первую версию интеграционного чата.",
  delivery: {
    status: "delivered",
    attempts: NumberInt(1)
  },
  created_at: new Date("2026-05-25T09:20:00Z"),
  read_at: null
}));

section("Read");

print("Find user by login with $eq");
printjson(db.users.findOne(
  { login: { $eq: "pavel.orlov" } },
  { login: 1, first_name: 1, last_name: 1, email: 1 }
));

print("Find users by first name and last name mask with $and");
printjson(db.users.find({
  $and: [
    { first_name: { $regex: "^Па", $options: "i" } },
    { last_name: { $regex: "ов$", $options: "i" } }
  ]
}, { login: 1, first_name: 1, last_name: 1 }).toArray());

print("Find active users except one user with $ne");
printjson(db.users.find({
  is_active: true,
  login: { $ne: "pavel.orlov" }
}, { login: 1, is_active: 1 }).limit(5).toArray());

print("Find group messages by chat list with $in and date range with $gt/$lt");
printjson(db.group_messages.find({
  chat_id: { $in: [g1, g2, g3, tempGroupChatId] },
  created_at: {
    $gt: new Date("2026-05-20T08:00:00Z"),
    $lt: new Date("2026-05-26T00:00:00Z")
  }
}, { chat_id: 1, sender_id: 1, text: 1, created_at: 1 }).sort({ created_at: -1 }).toArray());

print("Load messages of one group chat");
printjson(db.group_messages.find(
  { chat_id: tempGroupChatId },
  { sender_id: 1, text: 1, attachments: 1, created_at: 1 }
).sort({ created_at: 1 }).limit(20).toArray());

print("Get PtP messages for user with $or");
printjson(db.ptp_messages.find({
  $or: [
    { sender_id: u1 },
    { receiver_id: u1 }
  ]
}, { sender_id: 1, receiver_id: 1, text: 1, created_at: 1 }).sort({ created_at: -1 }).limit(10).toArray());

print("Find PtP chats for user with array condition");
printjson(db.ptp_chats.find(
  { participant_ids: u1 },
  { participant_ids: 1, participant_key: 1, last_message_at: 1 }
).sort({ last_message_at: -1 }).toArray());

section("Update");

print("Update user profile with $set");
printjson(db.users.updateOne(
  { _id: tempUserId },
  {
    $set: {
      "profile.position": "Senior Integration Engineer",
      updated_at: new Date("2026-05-25T09:30:00Z")
    }
  }
));

print("Add user to group chat with $addToSet");
const addMemberResult = db.group_chats.updateOne(
  { _id: g1, "members.user_id": { $ne: tempUserId } },
  {
    $addToSet: {
      members: {
        user_id: tempUserId,
        role: "member",
        joined_at: new Date("2026-05-25T09:35:00Z"),
        muted: false
      }
    },
    $inc: { member_count: NumberInt(1) },
    $set: { updated_at: new Date("2026-05-25T09:35:00Z") }
  }
);
printjson(addMemberResult);

print("Add reaction to group message with $push");
printjson(db.group_messages.updateOne(
  { _id: tempGroupMessageId },
  {
    $push: {
      reactions: {
        emoji: "+1",
        user_ids: [u1, u2]
      }
    },
    $set: { edited_at: new Date("2026-05-25T09:40:00Z") }
  }
));

print("Update group chat settings with $and");
printjson(db.group_chats.updateOne(
  {
    $and: [
      { _id: tempGroupChatId },
      { "settings.retention_days": { $lt: NumberInt(365) } }
    ]
  },
  {
    $set: {
      "settings.retention_days": NumberInt(365),
      updated_at: new Date("2026-05-25T09:45:00Z")
    }
  }
));

print("Mark PtP message as read");
printjson(db.ptp_messages.updateOne(
  {
    _id: tempPtpMessageId,
    receiver_id: u1,
    read_at: null
  },
  {
    $set: {
      "delivery.status": "read",
      read_at: new Date("2026-05-25T09:50:00Z")
    }
  }
));

print("Update unread counter in PtP chat");
printjson(db.ptp_chats.updateOne(
  { _id: tempPtpChatId },
  {
    $set: {
      "unread_counts.665000000000000000000001": NumberInt(0),
      updated_at: new Date("2026-05-25T09:50:00Z")
    }
  }
));

section("Delete");

print("Remove user from group chat with $pull");
printjson(db.group_chats.updateOne(
  { _id: g1, "members.user_id": tempUserId },
  {
    $pull: { members: { user_id: tempUserId } },
    $inc: { member_count: NumberInt(-1) },
    $set: { updated_at: new Date("2026-05-25T10:00:00Z") }
  }
));

print("Delete PtP message");
printjson(db.ptp_messages.deleteOne({ _id: tempPtpMessageId }));

print("Delete group message");
printjson(db.group_messages.deleteOne({ _id: tempGroupMessageId }));

print("Delete PtP chat");
printjson(db.ptp_chats.deleteOne({ _id: tempPtpChatId }));

print("Delete group chat");
printjson(db.group_chats.deleteOne({ _id: tempGroupChatId }));

print("Delete user");
printjson(db.users.deleteOne({ _id: tempUserId }));

section("Final counts");
printjson({
  users: db.users.countDocuments(),
  group_chats: db.group_chats.countDocuments(),
  group_messages: db.group_messages.countDocuments(),
  ptp_chats: db.ptp_chats.countDocuments(),
  ptp_messages: db.ptp_messages.countDocuments()
});
