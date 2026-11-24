#include "Logger.hpp"

#include <iostream>

namespace RoadSign::Infrastructure {
std::string_view eventName(Event event) noexcept {
    switch (event) {
    case Event::serverStarted: return "server.started";
    case Event::requestRejected: return "request.rejected";
    case Event::sessionCreated: return "session.created";
    case Event::sessionEnded: return "session.ended";
    case Event::runtimeUnavailable: return "runtime.unavailable";
    }
    return "event.unknown";
}

void logEvent(Event event) noexcept {
    std::clog << "{\"event\":\"" << eventName(event) << "\"}\n";
}
} // namespace RoadSign::Infrastructure
