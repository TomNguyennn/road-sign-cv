#pragma once

#include "../application/SessionService.hpp"
#include "SessionEventBuffer.hpp"
#include "infrastructure/RuntimeConfig.hpp"

#include <drogon/drogon.h>

namespace RoadSign::RecognitionSession {
void registerSessionRoutes(drogon::HttpAppFramework& app, SessionService& sessions,
                           SessionEventBuffer& events,
                           const Infrastructure::RuntimeConfig& config);
} // namespace RoadSign::RecognitionSession
