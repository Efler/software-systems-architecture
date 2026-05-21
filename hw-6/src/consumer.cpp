#include <chrono>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_context.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/concurrent/variable.hpp>
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

class ProcessedEventsStorage final : public components::ComponentBase {
 public:
  static constexpr std::string_view kName = "processed-events-storage";

  using ComponentBase::ComponentBase;

  void Add(std::string message) {
    auto events = events_.Lock();
    events->push_back(std::move(message));

    if (events->size() > 100) {
      events->erase(events->begin());
    }
  }

  formats::json::Value GetAll() const {
    const auto events = events_.Lock();

    formats::json::ValueBuilder result;
    result.Resize(events->size());

    for (std::size_t i = 0; i < events->size(); ++i) {
      result[i] = formats::json::FromString((*events)[i]);
    }

    return result.ExtractValue();
  }

 private:
  mutable concurrent::Variable<std::vector<std::string>> events_;
};

class ConsumerTopology final : public components::ComponentBase {
 public:
  static constexpr std::string_view kName = "consumer-topology";

  ConsumerTopology(
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
        const auto deadline =
            engine::Deadline::FromDuration(std::chrono::seconds{10});
        const urabbitmq::Exchange exchange{std::string{kExchangeName}};
        const urabbitmq::Queue queue{std::string{kQueueName}};

        rabbit_client_->DeclareExchange(
            exchange, urabbitmq::Exchange::Type::kTopic, deadline);
        rabbit_client_->DeclareQueue(queue, deadline);
        rabbit_client_->BindQueue(exchange, queue, "user.created", deadline);
        rabbit_client_->BindQueue(exchange, queue, "event.created", deadline);
        rabbit_client_->BindQueue(exchange, queue, "registration.created", deadline);
        rabbit_client_->BindQueue(exchange, queue, "registration.cancelled", deadline);

        LOG_INFO() << "Consumer RabbitMQ topology is ready";
        return;
      } catch (const std::exception& ex) {
        LOG_WARNING() << "Consumer waits for RabbitMQ, attempt " << attempt
                      << ": " << ex.what();
        engine::SleepFor(std::chrono::seconds{1});
      }
    }

    throw std::runtime_error("Consumer RabbitMQ topology setup failed");
  }

 private:
  std::shared_ptr<urabbitmq::Client> rabbit_client_;
};

class ProcessedEventsHandler final
    : public server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-processed-events";

  ProcessedEventsHandler(
      const components::ComponentConfig& config,
      const components::ComponentContext& context
  )
      : HttpHandlerJsonBase(config, context),
        storage_(context.FindComponent<ProcessedEventsStorage>()) {}

  formats::json::Value HandleRequestJsonThrow(
      const server::http::HttpRequest&,
      const formats::json::Value&,
      server::request::RequestContext&
  ) const override {
    formats::json::ValueBuilder response;
    response["events"] = storage_.GetAll();
    return response.ExtractValue();
  }

 private:
  ProcessedEventsStorage& storage_;
};

class EventConsumer final : public urabbitmq::ConsumerComponentBase {
 public:
  static constexpr std::string_view kName = "event-consumer";

  EventConsumer(
      const components::ComponentConfig& config,
      const components::ComponentContext& context
  )
      : ConsumerComponentBase(config, context),
        storage_(context.FindComponent<ProcessedEventsStorage>()) {}

 protected:
  void Process(std::string message) override {
    const auto event = formats::json::FromString(message);
    const auto event_type = event["event_type"].As<std::string>();

    LOG_INFO() << "Consumed RabbitMQ event: " << event_type;
    storage_.Add(std::move(message));
  }

 private:
  ProcessedEventsStorage& storage_;
};

}  // namespace event_driven_architecture

int main(int argc, char* argv[]) {
  const auto component_list =
      components::MinimalServerComponentList()
          .Append<clients::dns::Component>()
          .Append<components::DefaultSecdistProvider>()
          .Append<components::Secdist>()
          .Append<components::RabbitMQ>("event-rabbit")
          .Append<event_driven_architecture::ProcessedEventsStorage>()
          .Append<event_driven_architecture::ConsumerTopology>()
          .Append<event_driven_architecture::ProcessedEventsHandler>()
          .Append<event_driven_architecture::EventConsumer>();

  return utils::DaemonMain(argc, argv, component_list);
}
