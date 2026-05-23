#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <functional>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <userver/components/component_base.hpp>
#include <userver/components/component_context.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/formats/json.hpp>
#include <userver/http/common_headers.hpp>
#include <userver/server/handlers/auth/auth_checker_factory.hpp>
#include <userver/server/handlers/http_handler_json_base.hpp>
#include <userver/server/handlers/implicit_options.hpp>
#include <userver/server/http/http_status.hpp>
#include <userver/utils/daemon_run.hpp>

using namespace userver;

namespace warehouse {

struct UserDto {
  std::uint64_t id{};
  std::string login;
  std::string first_name;
  std::string last_name;
  std::string email;
  std::string role;
  std::string password_hash;
  std::string created_at;
};

struct ProductDto {
  std::uint64_t id{};
  std::string name;
  std::string sku;
  std::string unit;
  std::string description;
  std::string created_at;
};

struct ReceiptDto {
  std::uint64_t id{};
  std::uint64_t product_id{};
  std::uint64_t quantity{};
  std::string received_at;
  std::uint64_t created_by{};
  std::string comment;
};

struct WriteOffDto {
  std::uint64_t id{};
  std::uint64_t product_id{};
  std::uint64_t quantity{};
  std::string written_off_at;
  std::uint64_t created_by{};
  std::string reason;
};

std::string NowUtc() {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);

  std::tm utc{};
  gmtime_r(&time, &utc);

  std::ostringstream result;
  result << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
  return result.str();
}

std::string ToLowerAscii( std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

bool ContainsIgnoreCase(
    const std::string& source,
      const std::string& mask
) {
  return ToLowerAscii(source).find(ToLowerAscii(mask)) != std::string::npos;
}

formats::json::Value UserToJson(const UserDto& user) {
  formats::json::ValueBuilder json;
  json["id"] = user.id;
  json["login"] = user.login;
  json["firstName"] = user.first_name;
  json["lastName"] = user.last_name;
  json["email"] = user.email;
  json["role"] = user.role;
  json["createdAt"] = user.created_at;
  return json.ExtractValue();
}

formats::json::Value ProductToJson(const ProductDto& product) {
  formats::json::ValueBuilder json;
  json["id"] = product.id;
  json["name"] = product.name;
  json["sku"] = product.sku;
  json["unit"] = product.unit;
  json["description"] = product.description;
  json["createdAt"] = product.created_at;
  return json.ExtractValue();
}

formats::json::Value ReceiptToJson(
    const ReceiptDto& receipt,
    const std::optional<ProductDto>& product = std::nullopt,
    const std::optional<UserDto>& user = std::nullopt
) {
  formats::json::ValueBuilder json;
  json["id"] = receipt.id;
  json["productId"] = receipt.product_id;
  if (product) {
    json["productName"] = product->name;
    json["sku"] = product->sku;
  }
  json["quantity"] = receipt.quantity;
  json["receivedAt"] = receipt.received_at;
  if (user) {
    formats::json::ValueBuilder created_by;
    created_by["id"] = user->id;
    created_by["login"] = user->login;
    json["createdBy"] = created_by.ExtractValue();
  } else {
    json["createdBy"] = receipt.created_by;
  }
  json["comment"] = receipt.comment;
  return json.ExtractValue();
}

formats::json::Value ErrorJson(const std::string& code, const std::string& message) {
  formats::json::ValueBuilder json;
  json["error"]["code"] = code;
  json["error"]["message"] = message;
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
  if (value.empty())  return default_value;
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
    if (id == 0) return std::nullopt;
    return id;
  } catch (...) {
    return std::nullopt;
  }
}

void AddCorsHeaders(const server::http::HttpRequest& request) {
  auto& response = request.GetHttpResponse();
  response.SetHeader(std::string{"Access-Control-Allow-Origin"}, "*");
  response.SetHeader(std::string{"Access-Control-Allow-Methods"}, "GET, POST, OPTIONS");
  response.SetHeader(std::string{"Access-Control-Allow-Headers"}, "Authorization, Content-Type");
  response.SetHeader(std::string{"Access-Control-Max-Age"}, "86400");
}

std::string PasswordHash(const std::string& password) {
  return std::to_string(std::hash<std::string>{}(password));
}

class WarehouseStorage final : public components::ComponentBase {
 public:

  static constexpr std::string_view kName = "warehouse-storage";



  WarehouseStorage(
      const components::ComponentConfig& config,
      const components::ComponentContext& context
  )
      : ComponentBase(config, context) {}

  enum class CreateUserResult { kCreated, kLoginExists };
  enum class CreateProductResult { kCreated, kSkuExists };
  enum class MovementResult { kCreated, kProductNotFound, kUserNotFound, kNotEnoughStock };

  std::string CreateSession(std::uint64_t user_id) {
    const auto token = "token-" + std::to_string(user_id) + "-" +
                       std::to_string(next_token_id_.fetch_add(1));
    tokens_[token] = user_id;
    return token;
  }

  bool IsValidToken(const std::string& token) const {
    std::lock_guard lock(mutex_);
    return tokens_.count(token) != 0;
  }

  CreateUserResult CreateUser(UserDto user, UserDto& created) {
    std::lock_guard lock(mutex_);
    if (user_by_login_.count(user.login) != 0) {
      return CreateUserResult::kLoginExists;
    }

    user.id = next_user_id_++;
    user.created_at = NowUtc();
    users_[user.id] = user;
    user_by_login_[user.login] = user.id;
    created = user;
    return CreateUserResult::kCreated;
  }

  std::optional<std::pair<UserDto, std::string>> RegisterUser(UserDto user) {
    std::lock_guard lock(mutex_);
    if (user_by_login_.count(user.login) != 0) {
      return std::nullopt;
    }

    user.id = next_user_id_++;
    user.created_at = NowUtc();
    users_[user.id] = user;
    user_by_login_[user.login] = user.id;
    const auto token = CreateSession(user.id);
    return std::make_pair(user, token);
  }

  std::optional<std::pair<UserDto, std::string>> Login(
      const std::string& login,
      const std::string& password
  ) {
    std::lock_guard lock(mutex_);
    const auto login_it = user_by_login_.find(login);
    if (login_it == user_by_login_.end()) return std::nullopt;

    const auto& user = users_.at(login_it->second);
    if (user.password_hash.empty() || user.password_hash != PasswordHash(password)) {
      return std::nullopt;
    }

    const auto token = CreateSession(user.id);
    return std::make_pair(user, token);
  }

  std::optional<UserDto> FindUserByLogin(const std::string& login) const {
    std::lock_guard lock(mutex_);
    const auto login_it = user_by_login_.find(login);
    if (login_it == user_by_login_.end()) return std::nullopt;
    return users_.at(login_it->second);
  }

  std::optional<UserDto> FindUserById(std::uint64_t id) const {
    std::lock_guard lock(mutex_);
    const auto it = users_.find(id);
    if (it == users_.end()) return std::nullopt;
    return it->second;
  }

  std::vector<UserDto> SearchUsers(
      const std::string& first_name_mask,
      const std::string& last_name_mask
  ) const {
    std::lock_guard lock(mutex_);
    std::vector<UserDto> result;
    for (const auto& [_, user] : users_) {
      const bool first_ok =
          first_name_mask.empty() || ContainsIgnoreCase(user.first_name, first_name_mask);
      const bool last_ok =
          last_name_mask.empty() || ContainsIgnoreCase(user.last_name, last_name_mask);
      if (first_ok && last_ok) result.push_back(user);
    }
    return result;
  }

  CreateProductResult CreateProduct(ProductDto product, ProductDto& created) {
    std::lock_guard lock(mutex_);
    if (product_by_sku_.count(product.sku) != 0) {
      return CreateProductResult::kSkuExists;
    }

    product.id = next_product_id_++;
    product.created_at = NowUtc();
    products_[product.id] = product;
    product_by_sku_[product.sku] = product.id;
    stock_balances_[product.id] = 0;
    created = product;
    return CreateProductResult::kCreated;
  }

  std::optional<ProductDto> FindProductById(std::uint64_t id) const {
    std::lock_guard lock(mutex_);
    const auto it = products_.find(id);
    if (it == products_.end()) return std::nullopt;
    return it->second;
  }

  std::vector<ProductDto> SearchProducts(const std::string& name_mask) const {
    std::lock_guard lock(mutex_);
    std::vector<ProductDto> result;
    for (const auto& [_, product] : products_) {
      if (ContainsIgnoreCase(product.name, name_mask)) result.push_back(product);
    }
    return result;
  }

  std::vector<formats::json::Value> StockBalances(
      std::optional<std::uint64_t> product_id,
      const std::string& name_mask
  ) const {
    std::lock_guard lock(mutex_);
    std::vector<formats::json::Value> result;
    for (const auto& [id, product] : products_) {
      if (product_id && id != *product_id) continue;
      if (!name_mask.empty() && !ContainsIgnoreCase(product.name, name_mask)) continue;

      formats::json::ValueBuilder item;
      item["productId"] = product.id;
      item["productName"] = product.name;
      item["sku"] = product.sku;
      item["unit"] = product.unit;
      item["quantity"] = stock_balances_.at(id);
      result.push_back(item.ExtractValue());
    }
    return result;
  }

  MovementResult CreateReceipt(ReceiptDto receipt, ReceiptDto& created, std::uint64_t& balance) {
    std::lock_guard lock(mutex_);
    if (products_.count(receipt.product_id) == 0) {
      return MovementResult::kProductNotFound;
    }
    if (users_.count(receipt.created_by) == 0) {
      return MovementResult::kUserNotFound;
    }

    receipt.id = next_receipt_id_++;
    if (receipt.received_at.empty()) receipt.received_at = NowUtc();
    receipts_.push_back(receipt);
    stock_balances_[receipt.product_id] += receipt.quantity;
    balance = stock_balances_[receipt.product_id];
    created = receipt;
    return MovementResult::kCreated;
  }

  std::vector<ReceiptDto> Receipts(std::optional<std::uint64_t> product_id) const {
    std::lock_guard lock(mutex_);
    std::vector<ReceiptDto> result;
    for (const auto& receipt : receipts_) {
      if (!product_id || receipt.product_id == *product_id) {
        result.push_back(receipt);
      }
    }
    return result;
  }

  MovementResult CreateWriteOff(WriteOffDto write_off, WriteOffDto& created, std::uint64_t& balance) {
    std::lock_guard lock(mutex_);
    if (products_.count(write_off.product_id) == 0) {
      return MovementResult::kProductNotFound;
    }
    if (users_.count(write_off.created_by) == 0) {
      return MovementResult::kUserNotFound;
    }
    if (stock_balances_[write_off.product_id] < write_off.quantity) {
      return MovementResult::kNotEnoughStock;
    }

    write_off.id = next_write_off_id_++;
    if (write_off.written_off_at.empty()) write_off.written_off_at = NowUtc();
    write_offs_.push_back(write_off);
    stock_balances_[write_off.product_id] -= write_off.quantity;
    balance = stock_balances_[write_off.product_id];
    created = write_off;
    return MovementResult::kCreated;
  }

 private:
  mutable std::mutex mutex_;
  std::uint64_t next_user_id_{1};
  std::uint64_t next_product_id_{1};
  std::uint64_t next_receipt_id_{1};
  std::uint64_t next_write_off_id_{1};

  std::unordered_map<std::uint64_t, UserDto> users_;
  std::unordered_map<std::string, std::uint64_t> user_by_login_;
  std::unordered_map<std::string, std::uint64_t> tokens_;
  std::unordered_map<std::uint64_t, ProductDto> products_;
  std::unordered_map<std::string, std::uint64_t> product_by_sku_;
  std::unordered_map<std::uint64_t, std::uint64_t> stock_balances_;
  std::vector<ReceiptDto> receipts_;
  std::vector<WriteOffDto> write_offs_;
  std::atomic<std::uint64_t> next_token_id_{1};
};

class BearerAuthChecker final : public server::handlers::auth::AuthCheckerBase {
 public:
  using AuthCheckResult = server::handlers::auth::AuthCheckResult;

  explicit BearerAuthChecker(const WarehouseStorage& storage) : storage_(storage) {}

  [[nodiscard]] AuthCheckResult CheckAuth(
      const server::http::HttpRequest& request,
      server::request::RequestContext&
  ) const override {
    const auto& auth_value = request.GetHeader(http::headers::kAuthorization);
    if (auth_value.empty()) {
      return AuthCheckResult{
          AuthCheckResult::Status::kTokenNotFound,
          {},
          "Authorization header is required",
          server::handlers::HandlerErrorCode::kUnauthorized};
    }

    constexpr std::string_view kPrefix = "Bearer ";
    if (auth_value.rfind(kPrefix, 0) != 0) {
      return AuthCheckResult{
          AuthCheckResult::Status::kInvalidToken,
          {},
          "Authorization header must use Bearer token",
          server::handlers::HandlerErrorCode::kUnauthorized};
    }

    const auto token = auth_value.substr(kPrefix.size());
    if (!storage_.IsValidToken(token)) {
      return AuthCheckResult{
          AuthCheckResult::Status::kInvalidToken,
          {},
          "Token is invalid",
          server::handlers::HandlerErrorCode::kUnauthorized};
    }

    return AuthCheckResult{AuthCheckResult::Status::kOk};
  }

  [[nodiscard]] bool SupportsUserAuth() const noexcept override { return true; }

 private:
  const WarehouseStorage& storage_;
};

class BearerAuthCheckerFactory final
    : public server::handlers::auth::AuthCheckerFactoryBase {
 public:
  static constexpr std::string_view kAuthType = "bearer";

  explicit BearerAuthCheckerFactory(const components::ComponentContext& context)
      : storage_(context.FindComponent<WarehouseStorage>()) {}

  server::handlers::auth::AuthCheckerBasePtr MakeAuthChecker(
      const server::handlers::auth::HandlerAuthConfig&
  ) const override {
    return std::make_shared<BearerAuthChecker>(storage_);
  }

 private:
  const WarehouseStorage& storage_;
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
        storage_(context.FindComponent<WarehouseStorage>()) {}

 protected:
  WarehouseStorage& storage_;

  formats::json::Value Error(
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

class AuthRegisterHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-auth-register";
  using HandlerBase::HandlerBase;

  formats::json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const formats::json::Value& json,
      server::request::RequestContext&
  ) const override {
    AddCorsHeaders(request);
    UserDto user;
    user.login = json["login"].As<std::string>("");
    user.first_name = json["firstName"].As<std::string>("");
    user.last_name = json["lastName"].As<std::string>("");
    user.email = json["email"].As<std::string>("");
    user.role = json["role"].As<std::string>("warehouse_manager");
    const auto password = json["password"].As<std::string>("");

    if (user.login.empty() || user.first_name.empty() || user.last_name.empty() ||
        user.email.empty() || password.empty()) {
      return Error(request, server::http::HttpStatus::kUnprocessableEntity,
                   "VALIDATION_ERROR",
                   "login, firstName, lastName, email and password are required");
    }

    user.password_hash = PasswordHash(password);
    const auto created = storage_.RegisterUser(user);
    if (!created) {
      return Error(request, server::http::HttpStatus::kConflict,
                   "LOGIN_ALREADY_EXISTS", "User with this login already exists");
    }

    formats::json::ValueBuilder response;
    response["token"] = created->second;
    response["user"] = UserToJson(created->first);
    request.SetResponseStatus(server::http::HttpStatus::kCreated);
    return response.ExtractValue();
  }
};

class AuthLoginHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-auth-login";
  using HandlerBase::HandlerBase;

  formats::json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const formats::json::Value& json,
      server::request::RequestContext&
  ) const override {
    AddCorsHeaders(request);
    const auto login = json["login"].As<std::string>("");
    const auto password = json["password"].As<std::string>("");
    if (login.empty() || password.empty()) {
      return Error(request, server::http::HttpStatus::kUnprocessableEntity,
                   "VALIDATION_ERROR", "login and password are required");
    }

    const auto session = storage_.Login(login, password);
    if (!session) {
      return Error(request, server::http::HttpStatus::kUnauthorized,
                   "INVALID_CREDENTIALS", "Login or password is incorrect");
    }

    formats::json::ValueBuilder response;
    response["token"] = session->second;
    response["user"] = UserToJson(session->first);
    return response.ExtractValue();
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
    AddCorsHeaders(request);
    UserDto user;
    user.login = json["login"].As<std::string>("");
    user.first_name = json["firstName"].As<std::string>("");
    user.last_name = json["lastName"].As<std::string>("");
    user.email = json["email"].As<std::string>("");
    user.role = json["role"].As<std::string>("warehouse_manager");

    if (user.login.empty() || user.first_name.empty() || user.last_name.empty() ||
        user.email.empty()) {
      return Error(request, server::http::HttpStatus::kUnprocessableEntity,
                   "VALIDATION_ERROR", "login, firstName, lastName and email are required");
    }

    UserDto created;
    const auto result = storage_.CreateUser(user, created);
    if (result == WarehouseStorage::CreateUserResult::kLoginExists) {
      return Error(request, server::http::HttpStatus::kConflict,
                   "LOGIN_ALREADY_EXISTS", "User with this login already exists");
    }

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
    AddCorsHeaders(request);
    const auto& login = request.GetArg("login");
    if (!login.empty()) {
      const auto user = storage_.FindUserByLogin(login);
      if (!user) {
        return Error(request, server::http::HttpStatus::kNotFound,
                     "USER_NOT_FOUND", "User with this login was not found");
      }

      return UserToJson(*user);
    }

    const auto& first_name_mask = request.GetArg("firstNameMask");
    const auto& last_name_mask = request.GetArg("lastNameMask");
    if (first_name_mask.empty() && last_name_mask.empty()) {
      return Error(request, server::http::HttpStatus::kBadRequest,
                   "SEARCH_PARAMS_REQUIRED", "login, firstNameMask or lastNameMask is required");
    }

    std::vector<formats::json::Value> items;
    for (const auto& user : storage_.SearchUsers(first_name_mask, last_name_mask)) {
      items.push_back(UserToJson(user));
    }
    return PagedResponse(items, QuerySize(request, "limit", 50), QuerySize(request, "offset", 0));
  }
};

class CreateProductHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-create-product";
  using HandlerBase::HandlerBase;

  formats::json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const formats::json::Value& json,
      server::request::RequestContext&
  ) const override {
    AddCorsHeaders(request);
    ProductDto product;
    product.name = json["name"].As<std::string>("");
    product.sku = json["sku"].As<std::string>("");
    product.unit = json["unit"].As<std::string>("");
    product.description = json["description"].As<std::string>("");

    if (product.name.empty() || product.sku.empty() || product.unit.empty()) {
      return Error(request, server::http::HttpStatus::kUnprocessableEntity,
                   "VALIDATION_ERROR", "name, sku and unit are required");
    }

    ProductDto created;
    const auto result = storage_.CreateProduct(product, created);
    if (result == WarehouseStorage::CreateProductResult::kSkuExists) {
      return Error(request, server::http::HttpStatus::kConflict,
                   "SKU_ALREADY_EXISTS", "Product with this sku already exists");
    }

    request.SetResponseStatus(server::http::HttpStatus::kCreated);
    return ProductToJson(created);
  }
};

class SearchProductsHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-search-products";
  using HandlerBase::HandlerBase;

  formats::json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const formats::json::Value&,
      server::request::RequestContext&

  ) const override {
    AddCorsHeaders(request);
    const auto& name = request.GetArg("name");
    if (name.empty()) {
      return Error(request, server::http::HttpStatus::kBadRequest,
                   "SEARCH_PARAMS_REQUIRED", "name query parameter is required");
    }

    std::vector<formats::json::Value> items;
    for (const auto& product : storage_.SearchProducts(name)) {
      items.push_back(ProductToJson(product));
    }
    return PagedResponse(items, QuerySize(request, "limit", 50), QuerySize(request, "offset", 0));
  }
};

class StockBalancesHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-stock-balances";
  using HandlerBase::HandlerBase;

  formats::json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const formats::json::Value&,
      server::request::RequestContext&
  ) const override {
    AddCorsHeaders(request);
    std::optional<std::uint64_t> product_id;
    if (!request.GetArg("productId").empty()) {
      product_id = ParsePositiveId(request.GetArg("productId"));
      if (!product_id) {
        return Error(request, server::http::HttpStatus::kBadRequest,
                     "INVALID_PRODUCT_ID", "productId must be a positive integer");
      }
      if (!storage_.FindProductById(*product_id)) {
        return Error(request, server::http::HttpStatus::kNotFound,
                     "PRODUCT_NOT_FOUND", "Product was not found");
      }
    }

    const auto items = storage_.StockBalances(product_id, request.GetArg("name"));
    return PagedResponse(items, QuerySize(request, "limit", 50), QuerySize(request, "offset", 0));
  }
};

class CreateReceiptHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-create-receipt";
  using HandlerBase::HandlerBase;

  formats::json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const formats::json::Value& json,
      server::request::RequestContext&
  ) const override {
    AddCorsHeaders(request);
    ReceiptDto receipt;
    receipt.product_id = json["productId"].As<std::uint64_t>(0);
    receipt.quantity = json["quantity"].As<std::uint64_t>(0);
    receipt.received_at = json["receivedAt"].As<std::string>("");
    receipt.created_by = json["createdBy"].As<std::uint64_t>(0);
    receipt.comment = json["comment"].As<std::string>("");

    if (receipt.product_id == 0 || receipt.created_by == 0 || receipt.quantity == 0) {
      return Error(request, server::http::HttpStatus::kUnprocessableEntity,
                   "VALIDATION_ERROR", "productId, createdBy and positive quantity are required");
    }

    ReceiptDto created;
    std::uint64_t balance = 0;
    const auto result = storage_.CreateReceipt(receipt, created, balance);
    if (result == WarehouseStorage::MovementResult::kProductNotFound) {
      return Error(request, server::http::HttpStatus::kNotFound,
                   "PRODUCT_NOT_FOUND", "Product was not found");
    }
    if (result == WarehouseStorage::MovementResult::kUserNotFound) {
      return Error(request, server::http::HttpStatus::kNotFound,
                   "USER_NOT_FOUND", "User was not found");
    }

    formats::json::ValueBuilder response(ReceiptToJson(created));
    response["stockBalance"]["productId"] = created.product_id;
    response["stockBalance"]["quantity"] = balance;
    request.SetResponseStatus(server::http::HttpStatus::kCreated);
    return response.ExtractValue();
  }
};

class ListReceiptsHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-list-receipts";
  using HandlerBase::HandlerBase;

  formats::json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const formats::json::Value&,
      server::request::RequestContext&
  ) const override {
    AddCorsHeaders(request);
    std::optional<std::uint64_t> product_id;
    if (!request.GetArg("productId").empty()) {
      product_id = ParsePositiveId(request.GetArg("productId"));
      if (!product_id) {
        return Error(request, server::http::HttpStatus::kBadRequest,
                     "INVALID_PRODUCT_ID", "productId must be a positive integer");
      }
      if (!storage_.FindProductById(*product_id)) {
        return Error(request, server::http::HttpStatus::kNotFound,
                     "PRODUCT_NOT_FOUND", "Product was not found");
      }
    }

    std::vector<formats::json::Value> items;
    for (const auto& receipt : storage_.Receipts(product_id)) {
      items.push_back(ReceiptToJson(
          receipt,
          storage_.FindProductById(receipt.product_id),
          storage_.FindUserById(receipt.created_by)
      ));
    }
    return PagedResponse(items, QuerySize(request, "limit", 50), QuerySize(request, "offset", 0));
  }
};

class CreateWriteOffHandler final : public HandlerBase {
 public:
  static constexpr std::string_view kName = "handler-create-write-off";
  using HandlerBase::HandlerBase;

  formats::json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest& request,
      const formats::json::Value& json,
      server::request::RequestContext&
  ) const override {
    AddCorsHeaders(request);
    WriteOffDto write_off;
    write_off.product_id = json["productId"].As<std::uint64_t>(0);
    write_off.quantity = json["quantity"].As<std::uint64_t>(0);
    write_off.written_off_at = json["writtenOffAt"].As<std::string>("");
    write_off.created_by = json["createdBy"].As<std::uint64_t>(0);
    write_off.reason = json["reason"].As<std::string>("");

    if (write_off.product_id == 0 || write_off.created_by == 0 || write_off.quantity == 0) {
      return Error(request, server::http::HttpStatus::kUnprocessableEntity,
                   "VALIDATION_ERROR", "productId, createdBy and positive quantity are required");
    }

    WriteOffDto created;
    std::uint64_t balance = 0;
    const auto result = storage_.CreateWriteOff(write_off, created, balance);
    if (result == WarehouseStorage::MovementResult::kProductNotFound) {
      return Error(
      request, server::http::HttpStatus::kNotFound,
                   "PRODUCT_NOT_FOUND", "Product was not found"
                   );
    }
    if (result == WarehouseStorage::MovementResult::kUserNotFound) {
      return Error(request, server::http::HttpStatus::kNotFound,
                   "USER_NOT_FOUND", "User was not found");
    }
    if (result == WarehouseStorage::MovementResult::kNotEnoughStock) {
      return Error(request, server::http::HttpStatus::kConflict,
                   "NOT_ENOUGH_STOCK", "Not enough stock for write-off");
    }

    formats::json::ValueBuilder response;
    response["id"] = created.id;
    response["productId"] = created.product_id;
    response["quantity"] = created.quantity;
    response["writtenOffAt"] = created.written_off_at;
    response["createdBy"] = created.created_by;
    response["reason"] = created.reason;
    response["stockBalance"]["productId"] = created.product_id;
    response["stockBalance"]["quantity"] = balance;
    request.SetResponseStatus(server::http::HttpStatus::kCreated);
    return response.ExtractValue();
  }
};

}

int main(int argc, char* argv[]) {
  server::handlers::auth::RegisterAuthCheckerFactory<
      warehouse::BearerAuthCheckerFactory>();

  const auto component_list =
      components::MinimalServerComponentList()
          .Append<warehouse::CorsOptionsHandler>()
          .Append<warehouse::WarehouseStorage>()
          .Append<warehouse::AuthRegisterHandler>()
          .Append<warehouse::AuthLoginHandler>()
          .Append<warehouse::CreateUserHandler>()
          .Append<warehouse::SearchUsersHandler>()
          .Append<warehouse::CreateProductHandler>()
          .Append<warehouse::SearchProductsHandler>()
          .Append<warehouse::StockBalancesHandler>()
          .Append<warehouse::CreateReceiptHandler>()
          .Append<warehouse::ListReceiptsHandler>()
          .Append<warehouse::CreateWriteOffHandler>();

  return utils::DaemonMain(argc, argv, component_list);
}
