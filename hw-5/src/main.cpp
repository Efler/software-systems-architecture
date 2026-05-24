#include <algorithm>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_base.hpp>
#include <userver/components/component_context.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/formats/json.hpp>
#include <userver/server/handlers/http_handler_json_base.hpp>
#include <userver/server/http/http_status.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/redis/client.hpp>
#include <userver/storages/redis/component.hpp>
#include <userver/storages/secdist/component.hpp>
#include <userver/storages/secdist/provider_component.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/daemon_run.hpp>

using namespace userver;

namespace profi {


struct UserDto {
  std::uint64_t id{};
  std::string login;
  std::string first_name;
  std::string last_name;
  std::string email;
  std::string created_at;
};

struct ServiceDto {
  std::uint64_t id{};
  std::string name;
  std::string description;
  double price{};
  std::uint64_t duration_minutes{};
  std::string created_at;
};

struct RateLimitResult {
  bool allowed{};
  std::uint64_t limit{};
  std::uint64_t remaining{};
  std::uint64_t reset_epoch{};
};

std::int64_t ToInt64( std::uint64_t value) {
  return static_cast<std::int64_t>( value );
}

std::uint64_t ReadUint64(const storages::postgres::Field& field) {
  return static_cast<std::uint64_t>(
      field.As<std::int64_t>());
}



formats::json::Value ErrorJson(const std::string& code, const std::string& message) {
  formats::json::ValueBuilder json;
  json["error"]["code"] = code;
  json["error"]["message"] = message;
  return json.ExtractValue();
}

formats::json::Value UserToJson(const UserDto& user) {
  formats::json::ValueBuilder json;
  json["id"] = user.id;
  json["login"] = user.login;
  json["firstName"] = user.first_name;
  json["lastName"] = user.last_name;
  json["email"] = user.email;
  json["createdAt"] = user.created_at;
  return json.ExtractValue();
}

formats::json::Value ServiceToJson(const ServiceDto& service) {
  formats::json::ValueBuilder json;
  json["id"] = service.id;
  json["name"] = service.name;
  json["description"] = service.description;
  json["price"] = service.price;
  json["durationMinutes"] = service.duration_minutes;
  json["createdAt"] = service.created_at;
  return json.ExtractValue();
}

formats::json::Value PagedResponse(
    const std::vector<formats::json::Value>& items,
    std::size_t limit,
      std::size_t offset
) {
  formats::json::ValueBuilder json;
  formats::json::ValueBuilder array(formats::common::Type::kArray);
  for (const auto& item : items) {
    array.PushBack(item);
  }
  json["items"] = array.ExtractValue();
  json["limit"] = limit;
  json["offset"] = offset;
  json["total"] = items.size();
  return json.ExtractValue();
}



std::size_t QuerySize(
    const server::http::HttpRequest& request,
    const std::string& name,
    std::size_t default_value
) {
  const auto& value = request.GetArg(name);
  if (value.empty()) return default_value;
  try {
    return static_cast<std::size_t>(std::stoull(value));
  } catch (...) {
    return default_value;
  }
}

std::optional<std::uint64_t> ParsePositiveId(const std::string& value) {
  if (value.empty()) return std::nullopt;
  try {
    const auto id = std::stoull(value);
    if (id == 0)  return std::nullopt;
    return id;
  } catch (...) {
    return std::nullopt;
  }
}

void AddCommonHeaders(const server::http::HttpRequest& request) {
  auto& response = request.GetHttpResponse();
  response.SetHeader(std::string{"Access-Control-Allow-Origin"}, "*");
  response.SetHeader(std::string{"Access-Control-Allow-Methods"}, "GET, POST, OPTIONS");
  response.SetHeader(std::string{"Access-Control-Allow-Headers"}, "Content-Type");
}

void AddRateLimitHeaders(
    const server::http::HttpRequest& request,
    const RateLimitResult& limit
) {
  auto& response = request.GetHttpResponse();
  response.SetHeader(std::string{"X-RateLimit-Limit"}, std::to_string(limit.limit));
  response.SetHeader(std::string{"X-RateLimit-Remaining"}, std::to_string(limit.remaining));
  response.SetHeader(std::string{"X-RateLimit-Reset"}, std::to_string(limit.reset_epoch));
  if (!limit.allowed) {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const auto retry_after =
        limit.reset_epoch > static_cast<std::uint64_t>(now)
            ? limit.reset_epoch - static_cast<std::uint64_t>(now)
            : 1;
    response.SetHeader(std::string{"Retry-After"}, std::to_string(retry_after));
  }
}

std::string ClientKey(const server::http::HttpRequest& request) {
  const auto forwarded_for = request.GetHeader("X-Forwarded-For");
  if (!forwarded_for.empty())
    return forwarded_for;
  return "local";
}



class CacheStore final : public components::ComponentBase {
 public:
  static constexpr std::string_view kName = "cache-store";

  CacheStore(
      const components::ComponentConfig& config,
      const components::ComponentContext& context
  )
      : ComponentBase(config, context),
        redis_client_(context.FindComponent<components::Redis>("profi-redis").GetClient("profi-cache")),
        redis_cc_(std::chrono::seconds{1}, std::chrono::seconds{2}, 2) {}

  std::optional<formats::json::Value> Get(const std::string& key) const {
    const auto value = redis_client_->Get(key, redis_cc_).Get();
    if (!value) return std::nullopt;
    return formats::json::FromString(*value);
  }

  void Set(const std::string& key, const formats::json::Value& value, std::chrono::seconds ttl) {
    redis_client_->Set(
        key,
        formats::json::ToString(value),
          std::chrono::duration_cast<std::chrono::milliseconds>(ttl),
        redis_cc_
    ).Get();
  }

  void Delete(const std::string& key) {
    redis_client_->Del(key, redis_cc_).Get();
  }

  std::uint64_t GetVersion(const std::string& key) const {
    const auto value = redis_client_->Get(key, redis_cc_).Get();
    if (!value) return 0;
    try {
      return std::stoull(*value);
    } catch (...) {
      return 0;
    }
  }

  void IncrementVersion(const std::string& key) {
    redis_client_->Incr(key, redis_cc_).Get();
  }

 private:
  storages::redis::ClientPtr redis_client_;
  storages::redis::CommandControl redis_cc_;
};

class RateLimiter final : public components::ComponentBase {
 public:
  static constexpr std::string_view kName = "rate-limiter";

  RateLimiter(
      const components::ComponentConfig& config,
      const components::ComponentContext& context
  )
      : ComponentBase(config, context),
        redis_client_(context.FindComponent<components::Redis>("profi-redis").GetClient("profi-cache")),
        redis_cc_(std::chrono::seconds{1}, std::chrono::seconds{2}, 2) {}

  RateLimitResult CheckTokenBucket(
      const std::string& key,
      std::uint64_t limit,
      std::uint64_t burst,
      std::chrono::seconds refill_period
  ) {
    const auto now = std::chrono::system_clock::now();
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    const auto refill_per_ms = static_cast<double>(limit) /
                               static_cast<double>(
                                   std::chrono::duration_cast<std::chrono::milliseconds>(
                                       refill_period).count());

    std::lock_guard<std::mutex> lock(mutex_);
    double tokens = static_cast<double>(burst);
    std::int64_t last_ms = now_ms;
    const auto stored = redis_client_->Get(key, redis_cc_).Get();
    if (stored) {
      const auto delimiter = stored->find(':');
      if (delimiter != std::string::npos) {
        tokens = std::stod(stored->substr(0, delimiter));
        last_ms = std::stoll(stored->substr(delimiter + 1));
      }
    }

    const auto elapsed_ms = std::max<std::int64_t>(0, now_ms - last_ms);
    tokens = std::min<double>(burst, tokens + static_cast<double>(elapsed_ms) * refill_per_ms);

    const auto allowed = tokens >= 1.0;
    if (allowed) tokens -= 1.0;

    const auto reset_epoch = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count() +
        (allowed ? 0 : 1));
    const auto remaining = static_cast<std::uint64_t>(std::max<double>(0.0, tokens));

    redis_client_->Set(
        key,
        std::to_string(tokens) + ":" + std::to_string(now_ms),
        std::chrono::duration_cast<std::chrono::milliseconds>(refill_period * 2),
        redis_cc_
    ).Get();

    return RateLimitResult{
        allowed, limit, remaining, reset_epoch};
  }

  RateLimitResult CheckSlidingWindow(
      const std::string& key,
      std::uint64_t limit,
      std::chrono::seconds window
  ) {
    const auto now = std::chrono::system_clock::now();
    const auto epoch_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    const auto window_seconds = window.count();
    const auto window_start = (epoch_seconds / window_seconds) * window_seconds;
    const auto previous_start = window_start - window_seconds;
    const auto elapsed = epoch_seconds - window_start;
    const auto previous_weight =
        static_cast<double>(window_seconds - elapsed) / static_cast<double>(window_seconds);
    const auto current_key = key + ":" + std::to_string(window_start);
    const auto previous_key = key + ":" + std::to_string(previous_start);

    std::lock_guard<std::mutex> lock(mutex_);
    const auto current_count = static_cast<std::uint64_t>(
        redis_client_->Incr(current_key, redis_cc_).Get());
    if (current_count == 1) {
      redis_client_->Expire(current_key, window * 2, redis_cc_).Get();
    }

    std::uint64_t previous_count = 0;
    const auto previous = redis_client_->Get(previous_key, redis_cc_).Get();
    if (previous) previous_count = std::stoull(*previous);

    const auto estimated =
        static_cast<double>(current_count) + static_cast<double>(previous_count) * previous_weight;
    const auto allowed = estimated <= static_cast<double>(limit);
    const auto remaining = allowed && estimated < static_cast<double>(limit)
                               ? static_cast<std::uint64_t>(
                                     static_cast<double>(limit) - estimated)
                               : 0;
    const auto reset_epoch = static_cast<std::uint64_t>(window_start + window_seconds);
    return RateLimitResult{allowed, limit, remaining, reset_epoch};
  }

 private:
  std::mutex mutex_;
  storages::redis::ClientPtr redis_client_;
  storages::redis::CommandControl redis_cc_;
};

class ProfiStorage final : public components::ComponentBase {
 public:
  static constexpr std::string_view kName = "profi-storage";

  enum class CreateUserResult { kCreated, kLoginExists };
  enum class CreateServiceResult { kCreated };
  enum class AddToOrderResult { kCreated, kUserNotFound, kServiceNotFound };

  ProfiStorage(
      const components::ComponentConfig& config,
      const components::ComponentContext& context
  )
      : ComponentBase(config, context),
        pg_cluster_(context.FindComponent<components::Postgres>("profi-database").GetCluster()) {}

  CreateUserResult CreateUser(UserDto user, UserDto& created) {
    const auto result = pg_cluster_->Execute(
        storages::postgres::ClusterHostType::kMaster,
        R"(
          INSERT INTO users (login, first_name, last_name, email)
          VALUES ($1, $2, $3, $4)
          ON CONFLICT (login) DO NOTHING
          RETURNING id, login, first_name, last_name, email,
                    to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"')
        )",
        user.login,
        user.first_name,
        user.last_name,
        user.email
    );
    if (result.IsEmpty()) return CreateUserResult::kLoginExists;
    created = ReadUser(result[0]);
    return CreateUserResult::kCreated;
  }

  std::optional<UserDto> FindUserByLogin(const std::string& login) const {
    const auto result = pg_cluster_->Execute(
        storages::postgres::ClusterHostType::kSlave,
        R"(
          SELECT id, login, first_name, last_name, email,
                 to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"')
          FROM users
          WHERE login = $1
        )",
        login
    );
    if (result.IsEmpty()) return std::nullopt;
    return ReadUser(result[0]);
  }

  bool UserExists(std::uint64_t user_id) const {
    const auto result = pg_cluster_->Execute(
        storages::postgres::ClusterHostType::kSlave,
        "SELECT 1 FROM users WHERE id = $1",
        ToInt64(user_id)
    );
    return !result.IsEmpty();
  }

  bool ServiceExists(std::uint64_t service_id) const {
    const auto result = pg_cluster_->Execute(
        storages::postgres::ClusterHostType::kSlave,
        "SELECT 1 FROM services WHERE id = $1 AND active = true",
        ToInt64(service_id)
    );
    return !result.IsEmpty();
  }

  std::vector<UserDto> SearchUsers(
      const std::string& first_name_mask,
      const std::string& last_name_mask,
      std::size_t limit,
      std::size_t offset
  ) const {
    const auto result = pg_cluster_->Execute(
        storages::postgres::ClusterHostType::kSlave,
        R"(
          SELECT id, login, first_name, last_name, email,
                 to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"')
          FROM users
          WHERE ($1 = '' OR lower(first_name) LIKE '%' || lower($1) || '%')
            AND ($2 = '' OR lower(last_name) LIKE '%' || lower($2) || '%')
          ORDER BY last_name, first_name, id
          LIMIT $3 OFFSET $4
        )",
        first_name_mask,
        last_name_mask,
        static_cast<std::int64_t>(limit),
        static_cast<std::int64_t>(offset)
    );

    std::vector<UserDto> users;
    for (const auto& row : result) {
      users.push_back(ReadUser(row));
    }
    return users;
  }

  CreateServiceResult CreateService(ServiceDto service, ServiceDto& created) {
    const auto result = pg_cluster_->Execute(
        storages::postgres::ClusterHostType::kMaster,
        R"(
          INSERT INTO services (name, description, price, duration_minutes)
          VALUES ($1, $2, $3, $4)
          RETURNING id, name, description, price, duration_minutes,
                    to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"')
        )",
        service.name,
        service.description,
        service.price,
        ToInt64(service.duration_minutes)
    );
    created = ReadService(result[0]);
    return CreateServiceResult::kCreated;
  }

  std::vector<ServiceDto> ListServices(std::size_t limit, std::size_t offset) const {
    const auto result = pg_cluster_->Execute(
        storages::postgres::ClusterHostType::kSlave,
        R"(
          SELECT id, name, description, price, duration_minutes,
                 to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"')
          FROM services
          WHERE active = true
          ORDER BY name, id
          LIMIT $1 OFFSET $2
        )",
        static_cast<std::int64_t>(limit),
        static_cast<std::int64_t>(offset)
    );

    std::vector<ServiceDto> services;
    for (const auto& row : result) {

      services.push_back(ReadService(row));
    }
    return services;
  }

  AddToOrderResult AddServicesToOrder(
      std::uint64_t user_id,
      const std::vector<std::uint64_t>& service_ids,
      std::uint64_t& order_id
  ) {
    if (!UserExists(user_id)) return AddToOrderResult::kUserNotFound;
    for (const auto service_id : service_ids) {
      if (!ServiceExists(service_id)) return AddToOrderResult::kServiceNotFound;
    }

    const auto order_result = pg_cluster_->Execute(
        storages::postgres::ClusterHostType::kMaster,
        R"(
          WITH existing_order AS (
            SELECT id
            FROM orders
            WHERE user_id = $1 AND status = 'draft'
            ORDER BY id DESC
            LIMIT 1
          ),
          new_order AS (
            INSERT INTO orders (user_id, status)
            SELECT $1, 'draft'
            WHERE NOT EXISTS (SELECT 1 FROM existing_order)
            RETURNING id
          )
          SELECT id FROM existing_order
          UNION ALL
          SELECT id FROM new_order
          LIMIT 1
        )",
        ToInt64(user_id)
    );
    order_id = ReadUint64(order_result[0][0]);

    for (const auto service_id : service_ids) {
      pg_cluster_->Execute(
          storages::postgres::ClusterHostType::kMaster,
          R"(
            INSERT INTO order_items (order_id, service_id, quantity)
            VALUES ($1, $2, 1)
            ON CONFLICT (order_id, service_id)
            DO UPDATE SET quantity = order_items.quantity + 1
          )",
          ToInt64(order_id),
          ToInt64(service_id)
      );
    }

    pg_cluster_->Execute(
        storages::postgres::ClusterHostType::kMaster,
        "UPDATE orders SET updated_at = now() WHERE id = $1",
        ToInt64(order_id)
    );

    return AddToOrderResult::kCreated;
  }

  std::optional<formats::json::Value> GetLatestOrderForUser(std::uint64_t user_id) const {
    const auto order_result = pg_cluster_->Execute(
        storages::postgres::ClusterHostType::kSlave,
        R"(
          SELECT id, status,
                 to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"'),
                 to_char(updated_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"')
          FROM orders
          WHERE user_id = $1
          ORDER BY created_at DESC, id DESC
          LIMIT 1
        )",
        ToInt64(user_id)
    );
    if (order_result.IsEmpty()) return std::nullopt;

    const auto order_id = ReadUint64(order_result[0][0]);
    const auto items_result = pg_cluster_->Execute(
        storages::postgres::ClusterHostType::kSlave,
        R"(
          SELECT s.id, s.name, s.description, s.price, s.duration_minutes, oi.quantity
          FROM order_items oi
          JOIN services s ON s.id = oi.service_id
          WHERE oi.order_id = $1
          ORDER BY s.name, s.id
        )",
        ToInt64(order_id)
    );

    formats::json::ValueBuilder order;
    order["id"] = order_id;
    order["userId"] = user_id;
    order["status"] = order_result[0][1].As<std::string>();
    order["createdAt"] = order_result[0][2].As<std::string>();
    order["updatedAt"] = order_result[0][3].As<std::string>();

    formats::json::ValueBuilder items(formats::common::Type::kArray);
    double total = 0.0;
    for (const auto& row : items_result) {
      formats::json::ValueBuilder item;
      const auto price = row[3].As<double>();
      const auto quantity = static_cast<std::uint64_t>(row[5].As<std::int32_t>());
      item["serviceId"] = ReadUint64(row[0]);
      item["name"] = row[1].As<std::string>();
      item["description"] = row[2].As<std::string>();
      item["price"] = price;
      item["durationMinutes"] = static_cast<std::uint64_t>(row[4].As<std::int32_t>());
      item["quantity"] = quantity;
      total += price * static_cast<double>(quantity);
      items.PushBack(item.ExtractValue());
    }
    order["items"] = items.ExtractValue();
    order["totalPrice"] = total;
    return order.ExtractValue();
  }

 private:
  static UserDto ReadUser(const storages::postgres::Row& row) {
    UserDto user;
    user.id = ReadUint64(row[0]);
    user.login = row[1].As<std::string>();
    user.first_name = row[2].As<std::string>();
    user.last_name = row[3].As<std::string>();
    user.email = row[4].As<std::string>();
    user.created_at = row[5].As<std::string>();
    return user;
  }

  static ServiceDto ReadService(const storages::postgres::Row& row) {
    ServiceDto service;
    service.id = ReadUint64(row[0]);
    service.name = row[1].As<std::string>();
    service.description = row[2].As<std::string>();
    service.price = row[3].As<double>();
    service.duration_minutes = static_cast<std::uint64_t>(row[4].As<std::int32_t>());
    service.created_at = row[5].As<std::string>();
    return service;
  }

  storages::postgres::ClusterPtr pg_cluster_;
};

class HandlerBase : public server::handlers::HttpHandlerJsonBase {
 public:
  HandlerBase(
      const components::ComponentConfig& config,
      const components::ComponentContext& context
  )
      : HttpHandlerJsonBase(config, context),
        storage_(context.FindComponent<ProfiStorage>()),
        cache_(context.FindComponent<CacheStore>()),
        rate_limiter_(context.FindComponent<RateLimiter>()) {}

 protected:
  ProfiStorage& storage_;
  CacheStore& cache_;
  RateLimiter& rate_limiter_;

  formats::json::Value Error(
      const server::http::HttpRequest& request,
      server::http::HttpStatus status,
      const std::string& code,
      const std::string& message
  ) const {
    AddCommonHeaders(request);
    request.SetResponseStatus(status);
    return ErrorJson(code, message);
  }
};

class CreateUserHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-create-user";
  using HandlerBase::HandlerBase;

  formats::json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const formats::json::Value& json,
      server::request::RequestContext&
  ) const override {
    AddCommonHeaders(request);

    const auto rate_limit = rate_limiter_.CheckSlidingWindow(
        "rl:users:create:" + ClientKey(request),
        10,
        std::chrono::minutes{1}
    );


    AddRateLimitHeaders(request, rate_limit);
    if (!rate_limit.allowed)  {

      return Error(request, server::http::HttpStatus::kTooManyRequests,
                   "RATE_LIMIT_EXCEEDED", "Too many registration requests. Try again later.");
    }

    UserDto user;
    user.login = json["login"].As<std::string>("");
    user.first_name = json["firstName"].As<std::string>("");
    user.last_name = json["lastName"].As<std::string>("");
    user.email = json["email"].As<std::string>("");

    if (user.login.empty() || user.first_name.empty() || user.last_name.empty() ||
        user.email.empty()) {
      return Error(request, server::http::HttpStatus::kUnprocessableEntity,
                   "VALIDATION_ERROR", "login, firstName, lastName and email are required");
    }

    UserDto created;
    const auto result = storage_.CreateUser(user, created);
    if (result == ProfiStorage::CreateUserResult::kLoginExists) {
      return Error(request, server::http::HttpStatus::kConflict,
                   "LOGIN_ALREADY_EXISTS", "User with this login already exists");
    }

    cache_.Delete("user:login:" + created.login);
    request.SetResponseStatus(server::http::HttpStatus::kCreated);
    return UserToJson(created);
  }
};



class SearchUsersHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-search-users";
  using HandlerBase::HandlerBase;

  formats::json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const formats::json::Value&,
      server::request::RequestContext&
  ) const override {
    AddCommonHeaders(request);

    const auto& login = request.GetArg("login");
    if (!login.empty()) {
      const auto cache_key = "user:login:" + login;
      if (const auto cached = cache_.Get(cache_key)) {
        return *cached;
      }

      const auto user = storage_.FindUserByLogin(login);
      if (!user) {
        return Error(request, server::http::HttpStatus::kNotFound,
                     "USER_NOT_FOUND", "User with this login was not found");
      }

      const auto response = UserToJson(*user);

      cache_.Set(cache_key, response, std::chrono::minutes{10});
      return response
      ;
    }

    const auto& first_name_mask = request.GetArg("firstNameMask");
    const auto& last_name_mask = request.GetArg("lastNameMask");
    if (first_name_mask.empty() && last_name_mask.empty()) {
      return Error(request, server::http::HttpStatus::kBadRequest,
                   "SEARCH_PARAMS_REQUIRED", "login, firstNameMask or lastNameMask is required");
    }

    const auto limit = std::min<std::size_t>(QuerySize(request, "limit", 50), 100);
    const auto offset = QuerySize(request, "offset", 0);
    std::vector<formats::json::Value> items;
    for (const auto& user : storage_.SearchUsers(first_name_mask, last_name_mask, limit, offset)) {
      items.push_back(UserToJson(user));
    }
    return PagedResponse(items, limit, offset);
  }
};



class CreateServiceHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-create-service";
  using HandlerBase::HandlerBase;

  formats::json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const formats::json::Value& json,
      server::request::RequestContext&
  ) const override {
    AddCommonHeaders(request);

    ServiceDto service;
    service.name = json["name"].As<std::string>("");
    service.description = json["description"].As<std::string>("");
    service.price = json["price"].As<double>(0.0);
    service.duration_minutes = json["durationMinutes"].As<std::uint64_t>(60);

    if (service.name.empty() || service.price <= 0 || service.duration_minutes == 0) {
      return Error(request, server::http::HttpStatus::kUnprocessableEntity,
                   "VALIDATION_ERROR", "name, positive price and durationMinutes are required");
    }

    ServiceDto created;
    storage_.CreateService(service, created);
    cache_.IncrementVersion("cache:services:list:version");
    request.SetResponseStatus(server::http::HttpStatus::kCreated);
    return ServiceToJson(created);
  }
};

class ListServicesHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-list-services";
  using HandlerBase::HandlerBase;

  formats::json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const formats::json::Value&,
      server::request::RequestContext&
  ) const override {
    AddCommonHeaders(request);

    const auto limit_result = rate_limiter_.CheckTokenBucket(
        "rl:services:list:" + ClientKey(request),
        300,
        100,
        std::chrono::minutes{1}
    );
    AddRateLimitHeaders(request, limit_result);
    if (!limit_result.allowed) {
      return Error(request, server::http::HttpStatus::kTooManyRequests,
                   "RATE_LIMIT_EXCEEDED", "Too many requests. Try again later.");
    }

    const auto limit = std::min<std::size_t>(QuerySize(request, "limit", 50), 100);
    const auto offset = QuerySize(request, "offset", 0);
    const auto version = cache_.GetVersion("cache:services:list:version");
    const auto cache_key =
        "services:list:v:" + std::to_string(version) +
        ":limit:" + std::to_string(limit) + ":offset:" + std::to_string(offset);
    if (const auto cached = cache_.Get(cache_key)) {
      return *cached;
    }

    std::vector<formats::json::Value> items;
    for (const auto& service : storage_.ListServices(limit, offset)) {
      items.push_back(ServiceToJson(service));
    }
    const auto response = PagedResponse(items, limit, offset);
    cache_.Set(cache_key, response, std::chrono::minutes{10});
    return response;
  }
};



class AddServicesToOrderHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-add-services-to-order";
  using HandlerBase::HandlerBase;

  formats::json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const formats::json::Value& json,
      server::request::RequestContext&
  ) const override {
    AddCommonHeaders(request);

    const auto user_id = json["userId"].As<std::uint64_t>(0);
    std::vector<std::uint64_t> service_ids;
    for (const auto& item : json["serviceIds"]) {
      const auto service_id = item.As<std::uint64_t>(0);
      if (service_id != 0) service_ids.push_back(service_id);
    }

    if (user_id == 0 || service_ids.empty()) {
      return Error(request, server::http::HttpStatus::kUnprocessableEntity,
                   "VALIDATION_ERROR", "userId and serviceIds are required");
    }

    const auto rate_limit = rate_limiter_.CheckSlidingWindow(
        "rl:orders:add-service:user:" + std::to_string(user_id),
        60,
        std::chrono::minutes{1}
    );
    AddRateLimitHeaders(request, rate_limit);
    if (!rate_limit.allowed) {
      return Error(request, server::http::HttpStatus::kTooManyRequests,
                   "RATE_LIMIT_EXCEEDED", "Too many order updates. Try again later.");
    }

    std::uint64_t order_id = 0;
    const auto result = storage_.AddServicesToOrder(user_id, service_ids, order_id);
    if (result == ProfiStorage::AddToOrderResult::kUserNotFound) {
      return Error(request, server::http::HttpStatus::kNotFound,
                   "USER_NOT_FOUND", "User was not found");
    }
    if (result == ProfiStorage::AddToOrderResult::kServiceNotFound) {
      return Error(request, server::http::HttpStatus::kNotFound,
                   "SERVICE_NOT_FOUND", "Service was not found");
    }

    cache_.Delete("order:user:" + std::to_string(user_id));

    formats::json::ValueBuilder response;
    response["id"] = order_id;
    response["userId"] = user_id;
    response["status"] = "draft";
    request.SetResponseStatus(server::http::HttpStatus::kCreated);
    return response.ExtractValue();
  }
};



class GetUserOrderHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-get-user-order";
  using HandlerBase::HandlerBase;

  formats::json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const formats::json::Value&,
      server::request::RequestContext&
  ) const override {
    AddCommonHeaders(request);

    const auto user_id = ParsePositiveId(request.GetArg("userId"));
    if (!user_id) {
      return Error(request, server::http::HttpStatus::kBadRequest,
                   "INVALID_USER_ID", "userId query parameter is required");
    }

    const auto cache_key = "order:user:" + std::to_string(*user_id);
    if (const auto cached = cache_.Get(cache_key)) {
      return *cached;
    }

    const auto order = storage_.GetLatestOrderForUser(*user_id);
    if (!order) {
      return Error(request, server::http::HttpStatus::kNotFound,
                   "ORDER_NOT_FOUND", "Order for this user was not found");
    }

    cache_.Set(cache_key, *order, std::chrono::seconds{60});
    return *order;
  }
};

}

int main(int argc, char* argv[]) {

  const auto component_list =
      components::MinimalServerComponentList()
          .Append<components::DefaultSecdistProvider>()
          .Append<components::Secdist>()
          .Append<components::Postgres>("profi-database")
          .Append<components::Redis>("profi-redis")
          .Append<components::TestsuiteSupport>()
          .Append<clients::dns::Component>()
          .Append<profi::CacheStore>()
          .Append<profi::RateLimiter>()
          .Append<profi::ProfiStorage>()
          .Append<profi::CreateUserHandler>()
          .Append<profi::SearchUsersHandler>()
          .Append<profi::CreateServiceHandler>()
          .Append<profi::ListServicesHandler>()
          .Append<profi::AddServicesToOrderHandler>()
          .Append<profi::GetUserOrderHandler>();

  return utils::DaemonMain(argc, argv, component_list);
}
