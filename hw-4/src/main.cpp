#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_base.hpp>
#include <userver/components/component_context.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/formats/bson/inline.hpp>
#include <userver/formats/bson/types.hpp>
#include <userver/formats/json.hpp>
#include <userver/server/handlers/http_handler_json_base.hpp>
#include <userver/server/handlers/implicit_options.hpp>
#include <userver/server/http/http_status.hpp>
#include <userver/storages/mongo/collection.hpp>
#include <userver/storages/mongo/component.hpp>
#include <userver/storages/mongo/pool.hpp>
#include <userver/utils/daemon_run.hpp>

using namespace userver;

namespace messenger {

namespace bson = formats::bson;
namespace json = formats::json;
namespace mongo = storages::mongo;

std::chrono::system_clock::time_point Now() {
  return std::chrono::system_clock::now();
}

std::string ToIsoString( std::chrono::system_clock::time_point value) {
  const auto time = std::chrono::system_clock::to_time_t(value);
  std::tm utc{};
  gmtime_r(&time, &utc);

  std::ostringstream result;
  result  << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
  return result.str();
}

std::string DateField(const bson::Document& doc, const std::string& field) {
  try {
    return ToIsoString(doc[field].As<std::chrono::system_clock::time_point>());
  } catch (...) {
    return {};
  }
}

std::optional<bson::Oid> ParseOid(const std::string& value) {
  if (value.size() != 24) return std::nullopt;
  try {
    return bson::Oid(value);
  } catch (...) {
    return std::nullopt;
  }
}

std::string ParticipantKey(const bson::Oid& first, const bson::Oid& second) {
  auto left = first.ToString();
  auto right = second.ToString();
  if (right < left) std::swap(left, right);
  return left + ":" + right;
}

std::size_t QueryLimit(const server::http::HttpRequest& request) {
  const auto value = request.GetArg("limit");
  if (value.empty()) return 50;
  try {
    return std::min<std::size_t>(std::stoull(value), 100);
  } catch (...) {
    return 50;
  }
}

void AddCorsHeaders(const server::http::HttpRequest& request) {
  auto& response = request.GetHttpResponse();
  response.SetHeader(std::string{"Access-Control-Allow-Origin"}, "*");
  response.SetHeader(std::string{"Access-Control-Allow-Methods"}, "GET, POST, OPTIONS");
  response.SetHeader(std::string{"Access-Control-Allow-Headers"}, "Content-Type");
}

json::Value ErrorJson(
    const std::string& code, const std::string& message
) {
  json::ValueBuilder result;
  result["error"]["code"] = code;
  result["error"]["message"] = message;
  return result.ExtractValue();
}

json::Value ItemsResponse(const std::vector<json::Value>& items) {
  json::ValueBuilder result;
  json::ValueBuilder array(formats::common::Type::kArray);
  for (const auto& item : items) {
    array.PushBack(item);
  }
  result["items"] = array.ExtractValue();
  result["total"] = items.size();
  return result.ExtractValue();
}

json::Value UserToJson(const bson::Document& doc) {
  json::ValueBuilder result;
  result["id"] = doc["_id"].As<bson::Oid>().ToString();
  result["login"] = doc["login"].As<std::string>();
  result["firstName"] = doc["first_name"].As<std::string>();
  result["lastName"] = doc["last_name"].As<std::string>();
  result["displayName"] = doc["display_name"].As<std::string>();
  result["email"] = doc["email"].As<std::string>();
  result["isActive"] = doc["is_active"].As<bool>();
  result["createdAt"] = DateField(doc, "created_at");
  return result.ExtractValue();
}

json::Value GroupChatToJson(const bson::Document& doc) {
  json::ValueBuilder result;
  result["id"] = doc["_id"].As<bson::Oid>().ToString();
  result["name"] = doc["name"].As<std::string>();
  result["description"] = doc["description"].As<std::string>();
  result["createdBy"] = doc["created_by"].As<bson::Oid>().ToString();
  result["memberCount"] = doc["member_count"].As<int>();
  result["createdAt"] = DateField(doc, "created_at");
  result["updatedAt"] = DateField(doc, "updated_at");
  return result.ExtractValue();
}

json::Value GroupMessageToJson(const bson::Document& doc) {
  json::ValueBuilder result;
  result["id"] = doc["_id"].As<bson::Oid>().ToString();
  result["chatId"] = doc["chat_id"].As<bson::Oid>().ToString();
  result["senderId"] = doc["sender_id"].As<bson::Oid>().ToString();
  result["text"] = doc["text"].As<std::string>();
  result["createdAt"] = DateField(doc, "created_at");
  return result.ExtractValue();
}

json::Value PtpMessageToJson(const bson::Document& doc) {
  json::ValueBuilder result;
  result["id"] = doc["_id"].As<bson::Oid>().ToString();
  result["chatId"] = doc["chat_id"].As<bson::Oid>().ToString();
  result["senderId"] = doc["sender_id"].As<bson::Oid>().ToString();
  result["receiverId"] = doc["receiver_id"].As<bson::Oid>().ToString();
  result["text"] = doc["text"].As<std::string>();
  result["createdAt"] = DateField(doc, "created_at");
  return result.ExtractValue();
}

class MessengerStorage final : public components::ComponentBase {
 public:
  static constexpr std::string_view kName = "messenger-storage";

  MessengerStorage(
      const components::ComponentConfig& config,
      const components::ComponentContext& context
  )
      : ComponentBase(config, context),
        pool_(context.FindComponent<components::Mongo>("messenger-mongo").GetPool()) {}

  std::optional<bson::Document> FindUserByLogin(const std::string& login) const {
    return Users().FindOne(bson::MakeDoc("login", login));
  }

  std::optional<bson::Document> FindUserById(const bson::Oid& user_id) const {
    return Users().FindOne(bson::MakeDoc("_id", user_id));
  }

  std::optional<bson::Document> FindGroupChatById(const bson::Oid& chat_id) const {
    return GroupChats().FindOne(bson::MakeDoc("_id", chat_id));
  }

  bson::Document CreateUser(const json::Value& request) {
    const auto user_id = bson::Oid();
    const auto login = request["login"].As<std::string>("");
    const auto first_name = request["firstName"].As<std::string>("");
    const auto last_name = request["lastName"].As<std::string>("");
    const auto display_name = request["displayName"].As<std::string>(first_name + " " + last_name);
    const auto email = request["email"].As<std::string>("");
    const auto now = Now();

    auto doc = bson::MakeDoc(
        "_id", user_id,
        "login", login,
        "first_name", first_name,
        "last_name", last_name,
        "display_name", display_name,
        "email", email,
        "is_active", true,
        "profile", bson::MakeDoc(
            "position", request["position"].As<std::string>("Member"),
            "department", request["department"].As<std::string>("General"),
            "timezone", request["timezone"].As<std::string>("Europe/Moscow"),
            "skills", bson::MakeArray("API", "MongoDB")
        ),
        "created_at", now,
        "updated_at", now
    );

    Users().InsertOne(doc);
    return *FindUserById(user_id);
  }

  std::vector<bson::Document> SearchUsers(
      const std::string& first_name_mask,
      const std::string& last_name_mask,
      std::size_t limit
  ) const {
    const auto filter = bson::MakeDoc(
        "$and", bson::MakeArray(
            bson::MakeDoc("first_name", bson::MakeDoc("$regex", first_name_mask, "$options", "i")),
            bson::MakeDoc("last_name", bson::MakeDoc("$regex", last_name_mask, "$options", "i"))
        )
    );
    return Collect(Users().Find(filter), limit);
  }

  bson::Document CreateGroupChat(const json::Value& request, const bson::Oid& owner_id) {
    const auto chat_id = bson::Oid();
    const auto now = Now();

    auto doc = bson::MakeDoc(
        "_id", chat_id,
        "name", request["name"].As<std::string>(""),
        "description", request["description"].As<std::string>(""),
        "created_by", owner_id,
        "members", bson::MakeArray(bson::MakeDoc(
            "user_id", owner_id,
            "role", "owner",
            "joined_at", now,
            "muted", false
        )),
        "member_count", 1,
        "settings", bson::MakeDoc(
            "is_private", request["isPrivate"].As<bool>(false),
            "allow_threads", true,
            "retention_days", 365
        ),
        "created_at", now,
        "updated_at", now
    );

    GroupChats().InsertOne(doc);
    return *FindGroupChatById(chat_id);
  }

  bool GroupMemberExists(const bson::Oid& chat_id, const bson::Oid& user_id) const {
    return GroupChats().FindOne(bson::MakeDoc("_id", chat_id, "members.user_id", user_id)).has_value();
  }

  void AddUserToGroupChat(const bson::Oid& chat_id, const bson::Oid& user_id) {
    const auto now = Now();
    GroupChats().UpdateOne(
        bson::MakeDoc("_id", chat_id),
        bson::MakeDoc(
            "$addToSet", bson::MakeDoc(
                "members", bson::MakeDoc(
                    "user_id", user_id,
                    "role", "member",
                    "joined_at", now,
                    "muted", false
                )
            ),
            "$inc", bson::MakeDoc("member_count", 1),
            "$set", bson::MakeDoc("updated_at", now)
        )
    );
  }

  bson::Document AddGroupMessage(
      const bson::Oid& chat_id,
      const bson::Oid& sender_id,
      const std::string& text
  ) {
    const auto message_id = bson::Oid();
    auto doc = bson::MakeDoc(
        "_id", message_id,
        "chat_id", chat_id,
        "sender_id", sender_id,
        "text", text,
        "sequence_number", 1,
        "reactions", bson::MakeArray(),
        "attachments", bson::MakeArray(),
        "created_at", Now(),
        "edited_at", nullptr
    );

    GroupMessages().InsertOne(doc);
    return *GroupMessages().FindOne(bson::MakeDoc("_id", message_id));
  }

  std::vector<bson::Document> GroupMessagesByChat(const bson::Oid& chat_id, std::size_t limit) const {
    return Collect(GroupMessages().Find(bson::MakeDoc("chat_id", chat_id)), limit);
  }

  bson::Document SendPtpMessage(
      const bson::Oid& sender_id,
      const bson::Oid& receiver_id,
      const std::string& text
  ) {
    const auto key = ParticipantKey(sender_id, receiver_id);
    const auto sender_key = sender_id.ToString();
    const auto receiver_key = receiver_id.ToString();
    auto chat = PtpChats().FindOne(bson::MakeDoc("participant_key", key));

    bson::Oid chat_id;
    if (chat) {
      chat_id = (*chat)["_id"].As<bson::Oid>();
    } else {
      chat_id = bson::Oid();
      const auto now = Now();
      PtpChats().InsertOne(bson::MakeDoc(
          "_id", chat_id,
          "participant_ids", bson::MakeArray(sender_id, receiver_id),
          "participant_key", key,
          "last_message_at", now,
          "unread_counts", bson::MakeDoc(sender_key, 0, receiver_key, 1),
          "created_at", now,
          "updated_at", now
      ));
    }

    const auto message_id = bson::Oid();
    const auto now = Now();
    PtpMessages().InsertOne(bson::MakeDoc(
        "_id", message_id,
        "chat_id", chat_id,
        "sender_id", sender_id,
        "receiver_id", receiver_id,
        "text", text,
        "delivery", bson::MakeDoc("status", "delivered", "attempts", 1),
        "created_at", now,
        "read_at", nullptr
    ));

    PtpChats().UpdateOne(
        bson::MakeDoc("_id", chat_id),
        bson::MakeDoc(
            "$set", bson::MakeDoc("last_message_at", now, "updated_at", now),
            "$inc", bson::MakeDoc("unread_counts." + receiver_key, 1)
        )
    );

    return *PtpMessages().FindOne(bson::MakeDoc("_id", message_id));
  }

  std::vector<bson::Document> PtpMessagesForUser(const bson::Oid& user_id, std::size_t limit) const {
    return Collect(
        PtpMessages().Find(bson::MakeDoc(
            "$or", bson::MakeArray(
                bson::MakeDoc("sender_id", user_id),
                bson::MakeDoc("receiver_id", user_id)
            )
        )),
        limit
    );
  }

 private:
  mongo::Collection Users() const { return pool_->GetCollection("users"); }
  mongo::Collection GroupChats() const { return pool_->GetCollection("group_chats"); }
  mongo::Collection GroupMessages() const { return pool_->GetCollection("group_messages"); }
  mongo::Collection PtpChats() const { return pool_->GetCollection("ptp_chats"); }
  mongo::Collection PtpMessages() const { return pool_->GetCollection("ptp_messages"); }

  static std::vector<bson::Document> Collect(mongo::Cursor cursor, std::size_t limit) {
    std::vector<bson::Document> result;
    for (auto&& doc : cursor) {
      if (result.size() >= limit) break;
      result.push_back(doc);
    }
    return result;
  }

  mongo::PoolPtr pool_;
};

class CorsOptionsHandler final : public server::handlers::ImplicitOptions {
 public:
  static constexpr std::string_view kName = "handler-implicit-http-options";
  using ImplicitOptions::ImplicitOptions;

  std::string HandleRequestThrow(
      const server::http::HttpRequest& request,
      server::request::RequestContext& context
  ) const override {
    AddCorsHeaders(request);
    return ImplicitOptions::HandleRequestThrow(request, context);
  }
};

class HandlerBase : public server::handlers::HttpHandlerJsonBase {
 public:
  HandlerBase(
      const components::ComponentConfig& config,
      const components::ComponentContext& context
  )
      : HttpHandlerJsonBase(config, context),
        storage_(context.FindComponent<MessengerStorage>()) {}

 protected:
  MessengerStorage& storage_;

  json::Value Error(
      const server::http::HttpRequest& request,
      server::http::HttpStatus status,
      const std::string& code,
      const std::string& message
  ) const {
    AddCorsHeaders(request);
    request.SetResponseStatus(status);
    return ErrorJson(code, message);
  }
};

class HealthHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-health";
  using HandlerBase::HandlerBase;

  json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const json::Value&,
      server::request::RequestContext&
  ) const override {
    AddCorsHeaders(request);
    json::ValueBuilder result;
    result["status"] = "ok";
    return result.ExtractValue();
  }
};

class CreateUserHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-create-user";
  using HandlerBase::HandlerBase;

  json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const json::Value& body,
      server::request::RequestContext&
  ) const override {
    AddCorsHeaders(request);
    const auto login = body["login"].As<std::string>("");
    const auto first_name = body["firstName"].As<std::string>("");
    const auto last_name = body["lastName"].As<std::string>("");
    const auto email = body["email"].As<std::string>("");

    if (login.empty() || first_name.empty() ||
        last_name.empty() || email.empty()) {
      return Error(request, server::http::HttpStatus::kUnprocessableEntity,
                   "VALIDATION_ERROR", "login, firstName, lastName and email are required");
    }
    if (storage_.FindUserByLogin(login)) {
      return Error(request, server::http::HttpStatus::kConflict,
                   "LOGIN_ALREADY_EXISTS", "User with this login already exists");
    }

    try {
      auto created = storage_.CreateUser(body);
      request.SetResponseStatus(server::http::HttpStatus::kCreated);
      return UserToJson(created);
    } catch (const std::exception& error) {
      return Error(request, server::http::HttpStatus::kBadRequest,
                   "MONGO_ERROR", error.what());
    }
  }
};

class SearchUsersHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-search-users";
  using HandlerBase::HandlerBase;

  json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const json::Value&,
      server::request::RequestContext&
  ) const override {
    AddCorsHeaders(request);
    const auto login = request.GetArg("login");
    if (!login.empty()) {
      const auto user = storage_.FindUserByLogin(login);
      if (!user) {
        return Error(request, server::http::HttpStatus::kNotFound,
                     "USER_NOT_FOUND", "User with this login was not found");
      }
      return UserToJson(*user);
    }

    const auto first_name_mask = request.GetArg("firstNameMask");
    const auto last_name_mask = request.GetArg("lastNameMask");
    if (first_name_mask.empty() && last_name_mask.empty()) {
      return Error(request, server::http::HttpStatus::kBadRequest,
                   "SEARCH_PARAMS_REQUIRED", "login, firstNameMask or lastNameMask is required");
    }

    std::vector<json::Value> items;
    for (const auto& user : storage_.SearchUsers(first_name_mask, last_name_mask, QueryLimit(request))) {
      items.push_back(UserToJson(user));
    }
    return ItemsResponse(items);
  }
};

class CreateGroupChatHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-create-group-chat";
  using HandlerBase::HandlerBase;

  json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const json::Value& body,
      server::request::RequestContext&
  ) const override {
    AddCorsHeaders(request);
    const auto name = body["name"].As<std::string>("");
    const auto owner = ParseOid(body["createdBy"].As<std::string>(""));
    if (name.empty() || !owner) {
      return Error(request, server::http::HttpStatus::kUnprocessableEntity,
                   "VALIDATION_ERROR", "name and createdBy are required");
    }
    if (!storage_.FindUserById(*owner)) {
      return Error(request, server::http::HttpStatus::kNotFound,
                   "USER_NOT_FOUND", "Chat owner was not found");
    }

    const auto created = storage_.CreateGroupChat(body, *owner);
    request.SetResponseStatus(server::http::HttpStatus::kCreated);
    return GroupChatToJson(created);
  }
};

class AddGroupMemberHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-add-group-member";
  using HandlerBase::HandlerBase;

  json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const json::Value& body,
      server::request::RequestContext&
  ) const override {
    AddCorsHeaders(request);
    const auto chat_id = ParseOid(body["chatId"].As<std::string>(""));
    const auto user_id = ParseOid(body["userId"].As<std::string>(""));
    if (!chat_id || !user_id) {
      return Error(request, server::http::HttpStatus::kUnprocessableEntity,
                   "VALIDATION_ERROR", "chatId and userId are required");
    }
    if (!storage_.FindGroupChatById(*chat_id)) {
      return Error(request, server::http::HttpStatus::kNotFound,
                   "CHAT_NOT_FOUND", "Group chat was not found");
    }
    if (!storage_.FindUserById(*user_id)) {
      return Error(request, server::http::HttpStatus::kNotFound,
                   "USER_NOT_FOUND", "User was not found");
    }
    if (storage_.GroupMemberExists(*chat_id, *user_id)) {
      return Error(request, server::http::HttpStatus::kConflict,
                   "USER_ALREADY_IN_CHAT", "User is already a member of this chat");
    }

    storage_.AddUserToGroupChat(*chat_id, *user_id);
    json::ValueBuilder result;
    result["chatId"] = chat_id->ToString();
    result["userId"] = user_id->ToString();
    result["status"] = "added";
    return result.ExtractValue();
  }
};

class AddGroupMessageHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-add-group-message";
  using HandlerBase::HandlerBase;

  json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const json::Value& body,
      server::request::RequestContext&
  ) const override {
    AddCorsHeaders(request);
    const auto chat_id = ParseOid(body["chatId"].As<std::string>(""));
    const auto sender_id = ParseOid(body["senderId"].As<std::string>(""));
    const auto text = body["text"].As<std::string>("");
    if (!chat_id || !sender_id || text.empty()) {
      return Error(request, server::http::HttpStatus::kUnprocessableEntity,
                   "VALIDATION_ERROR", "chatId, senderId and text are required");
    }
    if (!storage_.FindGroupChatById(*chat_id)) {
      return Error(request, server::http::HttpStatus::kNotFound,
                   "CHAT_NOT_FOUND", "Group chat was not found");
    }
    if (!storage_.GroupMemberExists(*chat_id, *sender_id)) {
      return Error(request, server::http::HttpStatus::kForbidden,
                   "SENDER_NOT_IN_CHAT", "Sender must be a member of the group chat");
    }

    const auto created = storage_.AddGroupMessage(*chat_id, *sender_id, text);
    request.SetResponseStatus(server::http::HttpStatus::kCreated);
    return GroupMessageToJson(created);
  }
};

class ListGroupMessagesHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-list-group-messages";
  using HandlerBase::HandlerBase;

  json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const json::Value&,
      server::request::RequestContext&
  ) const override {
    AddCorsHeaders(request);
    const auto chat_id = ParseOid(request.GetArg("chatId"));
    if (!chat_id) {
      return Error(request, server::http::HttpStatus::kBadRequest,
                   "CHAT_ID_REQUIRED", "chatId query parameter is required");
    }

    std::vector<json::Value> items;
    for (const auto& message : storage_.GroupMessagesByChat(*chat_id, QueryLimit(request))) {
      items.push_back(GroupMessageToJson(message));
    }
    return ItemsResponse(items);
  }
};

class SendPtpMessageHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-send-ptp-message";
  using HandlerBase::HandlerBase;

  json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const json::Value& body,
      server::request::RequestContext&
  ) const override {
    AddCorsHeaders(request);
    const auto sender_id = ParseOid(body["senderId"].As<std::string>(""));
    const auto receiver_id = ParseOid(body["receiverId"].As<std::string>(""));
    const auto text = body["text"].As<std::string>("");
    if (!sender_id || !receiver_id ||
        text.empty()) {
      return Error(request, server::http::HttpStatus::kUnprocessableEntity,
                   "VALIDATION_ERROR", "senderId, receiverId and text are required");
    }
    if (sender_id->ToString() == receiver_id->ToString()) {
      return Error(request, server::http::HttpStatus::kUnprocessableEntity,
                   "VALIDATION_ERROR", "senderId and receiverId must be different");
    }
    if (!storage_.FindUserById(*sender_id) || !storage_.FindUserById(*receiver_id)) {
      return Error(request, server::http::HttpStatus::kNotFound,
                   "USER_NOT_FOUND", "Sender or receiver was not found");
    }

    const auto created = storage_.SendPtpMessage(*sender_id, *receiver_id, text);
    request.SetResponseStatus(server::http::HttpStatus::kCreated);
    return PtpMessageToJson(created);
  }
};

class ListPtpMessagesHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-list-ptp-messages";
  using HandlerBase::HandlerBase;

  json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const json::Value&,
      server::request::RequestContext&
  ) const override {
    AddCorsHeaders(request);
    const auto user_id = ParseOid(request.GetArg("userId"));
    if (!user_id) {
      return Error(request, server::http::HttpStatus::kBadRequest,
                   "USER_ID_REQUIRED", "userId query parameter is required");
    }

    std::vector<json::Value> items;
    for (const auto& message : storage_.PtpMessagesForUser(*user_id, QueryLimit(request))) {
      items.push_back(PtpMessageToJson(message));
    }
    return ItemsResponse(items);
  }
};

}

int main(int argc, char* argv[]) {
  const auto component_list =
      components::MinimalServerComponentList()
          .Append<clients::dns::Component>()
          .Append<components::Mongo>("messenger-mongo")
          .Append<messenger::CorsOptionsHandler>()
          .Append<messenger::MessengerStorage>()
          .Append<messenger::HealthHandler>()
          .Append<messenger::CreateUserHandler>()
          .Append<messenger::SearchUsersHandler>()
          .Append<messenger::CreateGroupChatHandler>()
          .Append<messenger::AddGroupMemberHandler>()
          .Append<messenger::AddGroupMessageHandler>()
          .Append<messenger::ListGroupMessagesHandler>()
          .Append<messenger::SendPtpMessageHandler>()
          .Append<messenger::ListPtpMessagesHandler>();

  return utils::DaemonMain( argc, argv, component_list);
}
