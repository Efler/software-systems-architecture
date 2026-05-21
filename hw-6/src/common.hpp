#pragma once

#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#include <fmt/format.h>

#include <userver/formats/json.hpp>

namespace event_driven_architecture {

constexpr std::string_view kExchangeName = "events.exchange";
constexpr std::string_view kQueueName = "event-driven.events.queue";

inline std::string NowUtc() {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);

  std::tm utc{};
  gmtime_r(&time, &utc);

  std::ostringstream result;
  result << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
  return result.str();
}

inline std::string NextMessageId() {
  static std::atomic<std::uint64_t> counter{1};
  return fmt::format("event-message-{}", counter.fetch_add(1));
}

inline userver::formats::json::Value BuildEnvelope(
    const std::string& event_type,
    const std::string& producer,
    userver::formats::json::Value payload
) {
  userver::formats::json::ValueBuilder message;
  message["event_id"] = NextMessageId();
  message["event_type"] = event_type;
  message["occurred_at"] = NowUtc();
  message["producer"] = producer;
  message["payload"] = std::move(payload);
  return message.ExtractValue();
}

}  // namespace event_driven_architecture
