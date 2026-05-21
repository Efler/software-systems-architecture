#include <chrono>
#include <exception>
#include <memory>
#include <string>

#include <fmt/format.h>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_context.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/engine/deadline.hpp>
#include <userver/engine/sleep.hpp>
#include <userver/formats/json.hpp>
#include <userver/logging/log.hpp>
#include <userver/rabbitmq.hpp>
#include <userver/server/handlers/http_handler_json_base.hpp>
#include <userver/storages/secdist/component.hpp>
#include <userver/storages/secdist/provider_component.hpp>
#include <userver/utils/daemon_run.hpp>

#include "common.hpp"

using namespace userver;

namespace event_driven_architecture {

class PublisherTopology final : public components::ComponentBase {
 public:
  static constexpr std::string_view kName = "publisher-topology";

  PublisherTopology(
      const components::ComponentConfig& config,
      const components::ComponentContext& context
  )
      : ComponentBase(config, context),
        rabbit_client_(
            context.FindComponent<components::RabbitMQ>("event-rabbit")
                .GetClient()) {}

  void OnAllComponentsLoaded() override {
    for (int attempt = 1; attempt <= 20; ++attempt) {
      try {
        rabbit_client_->DeclareExchange(
            urabbitmq::Exchange{std::string{kExchangeName}},
            urabbitmq::Exchange::Type::kTopic,
            engine::Deadline::FromDuration(std::chrono::seconds{10})
        );
        LOG_INFO() << "Producer RabbitMQ exchange is ready";
        return;
      } catch (const std::exception& ex) {
        LOG_WARNING() << "Producer waits for RabbitMQ, attempt " << attempt
                      << ": " << ex.what();
        engine::SleepFor(std::chrono::seconds{1});
      }
    }

    throw std::runtime_error("Producer RabbitMQ topology setup failed");
  }

 private:
  std::shared_ptr<urabbitmq::Client> rabbit_client_;
};

class EventPublisher final : public components::ComponentBase {
 public:
  static constexpr std::string_view kName = "event-publisher";

  EventPublisher(
      const components::ComponentConfig& config,
      const components::ComponentContext& context
  )
      : ComponentBase(config, context),
        rabbit_client_(
            context.FindComponent<components::RabbitMQ>("event-rabbit")
                .GetClient()) {}

  formats::json::Value Publish(
      const std::string& routing_key,
      const std::string& event_type,
      const std::string& producer,
      formats::json::Value payload
  ) const {
    const auto event = BuildEnvelope(event_type, producer, std::move(payload));

    rabbit_client_->PublishReliable(
        urabbitmq::Exchange{std::string{kExchangeName}},
        routing_key,
        formats::json::ToString(event),
        engine::Deadline::FromDuration(std::chrono::seconds{5})
    );

    formats::json::ValueBuilder response;
    response["status"] = "published";
    response["routing_key"] = routing_key;
    response["message"] = event;
    return response.ExtractValue();
  }

 private:
  std::shared_ptr<urabbitmq::Client> rabbit_client_;
};

class CreateUserHandler final : public server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-create-user";

  CreateUserHandler(
      const components::ComponentConfig& config,
      const components::ComponentContext& context
  )
      : HttpHandlerJsonBase(config, context),
        publisher_(context.FindComponent<EventPublisher>()) {}

  formats::json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest&,
      const formats::json::Value& request_json,
      server::request::RequestContext&
  ) const override {
    formats::json::ValueBuilder payload;
    payload["user_id"] = request_json["user_id"].As<std::string>();
    payload["login"] = request_json["login"].As<std::string>();
    payload["first_name"] = request_json["first_name"].As<std::string>();
    payload["last_name"] = request_json["last_name"].As<std::string>();
    payload["email"] = request_json["email"].As<std::string>();
    payload["created_at"] = NowUtc();

    return publisher_.Publish(
        "user.created",
        "UserCreatedEvent",
        "User Service",
        payload.ExtractValue()
    );
  }

 private:
  EventPublisher& publisher_;
};

class CreateEventHandler final : public server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-create-event";

  CreateEventHandler(
      const components::ComponentConfig& config,
      const components::ComponentContext& context
  )
      : HttpHandlerJsonBase(config, context),
        publisher_(context.FindComponent<EventPublisher>()) {}

  formats::json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest&,
      const formats::json::Value& request_json,
      server::request::RequestContext&
  ) const override {
    formats::json::ValueBuilder payload;
    payload["event_id"] = request_json["event_id"].As<std::string>();
    payload["title"] = request_json["title"].As<std::string>();
    payload["description"] = request_json["description"].As<std::string>();
    payload["event_date"] = request_json["event_date"].As<std::string>();
    payload["location"] = request_json["location"].As<std::string>();
    payload["owner_user_id"] = request_json["owner_user_id"].As<std::string>();
    payload["created_at"] = NowUtc();

    return publisher_.Publish(
        "event.created",
        "EventCreatedEvent",
        "Event Service",
        payload.ExtractValue()
    );
  }

 private:
  EventPublisher& publisher_;
};

class RegisterForEventHandler final
    : public server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-register-for-event";

  RegisterForEventHandler(
      const components::ComponentConfig& config,
      const components::ComponentContext& context
  )
      : HttpHandlerJsonBase(config, context),
        publisher_(context.FindComponent<EventPublisher>()) {}

  formats::json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest&,
      const formats::json::Value& request_json,
      server::request::RequestContext&
  ) const override {
    const auto user_id = request_json["user_id"].As<std::string>();
    const auto event_id = request_json["event_id"].As<std::string>();

    formats::json::ValueBuilder payload;
    payload["registration_id"] = fmt::format("registration-{}-{}", user_id, event_id);
    payload["event_id"] = event_id;
    payload["user_id"] = user_id;
    payload["registered_at"] = NowUtc();
    payload["status"] = "registered";

    return publisher_.Publish(
        "registration.created",
        "UserRegisteredForEventEvent",
        "Registration Service",
        payload.ExtractValue()
    );
  }

 private:
  EventPublisher& publisher_;
};

class CancelRegistrationHandler final
    : public server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-cancel-registration";

  CancelRegistrationHandler(
      const components::ComponentConfig& config,
      const components::ComponentContext& context
  )
      : HttpHandlerJsonBase(config, context),
        publisher_(context.FindComponent<EventPublisher>()) {}

  formats::json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest&,
      const formats::json::Value& request_json,
      server::request::RequestContext&
  ) const override {
    const auto user_id = request_json["user_id"].As<std::string>();
    const auto event_id = request_json["event_id"].As<std::string>();

    formats::json::ValueBuilder payload;
    payload["registration_id"] = fmt::format("registration-{}-{}", user_id, event_id);
    payload["event_id"] = event_id;
    payload["user_id"] = user_id;
    payload["cancelled_at"] = NowUtc();
    payload["status"] = "cancelled";

    return publisher_.Publish(
        "registration.cancelled",
        "EventRegistrationCancelledEvent",
        "Registration Service",
        payload.ExtractValue()
    );
  }

 private:
  EventPublisher& publisher_;
};

}  // namespace event_driven_architecture

int main(int argc, char* argv[]) {
  const auto component_list =
      components::MinimalServerComponentList()
          .Append<clients::dns::Component>()
          .Append<components::DefaultSecdistProvider>()
          .Append<components::Secdist>()
          .Append<components::RabbitMQ>("event-rabbit")
          .Append<event_driven_architecture::PublisherTopology>()
          .Append<event_driven_architecture::EventPublisher>()
          .Append<event_driven_architecture::CreateUserHandler>()
          .Append<event_driven_architecture::CreateEventHandler>()
          .Append<event_driven_architecture::RegisterForEventHandler>()
          .Append<event_driven_architecture::CancelRegistrationHandler>();

  return utils::DaemonMain(argc, argv, component_list);
}
