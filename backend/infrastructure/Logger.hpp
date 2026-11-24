#pragma once

#include <string_view>

namespace RoadSign::Infrastructure {
enum class Event { serverStarted, requestRejected, sessionCreated, sessionEnded, runtimeUnavailable };
void logEvent(Event event) noexcept;
std::string_view eventName(Event event) noexcept;
} // namespace RoadSign::Infrastructure
