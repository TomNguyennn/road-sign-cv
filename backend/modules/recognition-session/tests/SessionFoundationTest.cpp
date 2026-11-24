#include "../application/SessionService.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

namespace RoadSign::RecognitionSession {
TEST(SessionFoundationTest, AdmitsOnlyOneSessionAcrossConcurrentClients) {
    SessionService service{3, "https://demo.example"};
    const auto now = std::chrono::steady_clock::now();
    std::vector<CreateResult> results(8, {CreateResult::Status::busy, std::nullopt});
    std::vector<std::thread> clients;
    for (auto& result : results)
        clients.emplace_back([&service, &result, now] {
            result = service.create(SourceKind::camera, "https://demo.example", now);
        });
    for (auto& client : clients) client.join();
    EXPECT_EQ(std::count_if(results.begin(), results.end(), [](const auto& result) {
                  return result.status == CreateResult::Status::created;
              }), 1);
}

TEST(SessionFoundationTest, RequiresTrustedOriginAndCsrfForOwnership) {
    SessionService service{3, "https://demo.example"};
    auto result = service.create(SourceKind::image, "https://demo.example",
                                 std::chrono::steady_clock::now());
    ASSERT_TRUE(result.session);
    const auto& session = *result.session;
    EXPECT_TRUE(service.authenticate(session.sessionId, session.csrfToken, "https://demo.example"));
    EXPECT_FALSE(service.authenticate(session.sessionId, session.csrfToken, "https://attacker.example"));
    EXPECT_FALSE(service.authenticate(session.sessionId, "wrong", "https://demo.example"));
}

TEST(SessionFoundationTest, RequiresEventAttachmentBeforeInputAndCleanupBeforeNewLease) {
    SessionService service{0, "https://demo.example"};
    const auto now = std::chrono::steady_clock::now();
    auto result = service.create(SourceKind::video, "https://demo.example", now);
    ASSERT_TRUE(result.session);
    const auto session = *result.session;
    EXPECT_FALSE(service.attachEvents(session.sessionId, now + std::chrono::seconds{11}));
    EXPECT_TRUE(service.attachEvents(session.sessionId, now + std::chrono::seconds{5}));
    EXPECT_FALSE(service.attachEvents(session.sessionId, now + std::chrono::seconds{6}));
    EXPECT_TRUE(service.beginEnd(session.sessionId, session.csrfToken, "https://demo.example", now));
    EXPECT_FALSE(service.create(SourceKind::camera, "https://demo.example", now).session);
    EXPECT_TRUE(service.completeCleanup(session.sessionId));
    EXPECT_TRUE(service.create(SourceKind::camera, "https://demo.example", now).session);
}

TEST(SessionFoundationTest, AllowsEventAttachmentUntilTenSecondDeadline) {
    SessionService service{3, "https://demo.example"};
    const auto now = std::chrono::steady_clock::now();
    const auto session = service.create(SourceKind::image, "https://demo.example", now).session.value();
    EXPECT_TRUE(service.attachEvents(session.sessionId, now + std::chrono::seconds{9}));
}

TEST(SessionFoundationTest, RejectsLateCallbacksAfterEndBegins) {
    SessionService service{3, "https://demo.example"};
    const auto now = std::chrono::steady_clock::now();
    const auto session = service.create(SourceKind::camera, "https://demo.example", now).session.value();
    EXPECT_TRUE(service.acceptsCallback(session.sessionId, 1, 1));
    ASSERT_TRUE(service.beginEnd(session.sessionId, session.csrfToken, "https://demo.example", now));
    EXPECT_FALSE(service.acceptsCallback(session.sessionId, 1, 1));
}

TEST(SessionFoundationTest, ShutdownReleasesTheOnlySessionLease) {
    SessionService service{3, "https://demo.example"};
    ASSERT_TRUE(service.create(SourceKind::image, "https://demo.example",
                               std::chrono::steady_clock::now()).session);
    service.shutdown();
    EXPECT_FALSE(service.snapshot());
    EXPECT_TRUE(service.create(SourceKind::camera, "https://demo.example",
                               std::chrono::steady_clock::now()).session);
}

TEST(SessionFoundationTest, HeartbeatRejectsStaleEpoch) {
    SessionService service{3, "https://demo.example"};
    const auto now = std::chrono::steady_clock::now();
    const auto session = service.create(SourceKind::camera, "https://demo.example", now).session.value();
    ASSERT_TRUE(service.attachEvents(session.sessionId, now));
    EXPECT_FALSE(service.heartbeat(session.sessionId, session.csrfToken, "https://demo.example",
                                   2, now + std::chrono::seconds{5}));
    EXPECT_TRUE(service.heartbeat(session.sessionId, session.csrfToken, "https://demo.example",
                                  1, now + std::chrono::seconds{5}));
}

TEST(SessionFoundationTest, FailedUploadCanRetryButAcceptedUploadConsumesSourceSlot) {
    SessionService service{2, "https://demo.example"};
    const auto now = std::chrono::steady_clock::now();
    const auto session = service.create(SourceKind::image, "https://demo.example", now).session.value();
    ASSERT_TRUE(service.attachEvents(session.sessionId, now));
    auto state = service.snapshot().value();
    const auto createdRevision = state.stateRevision;
    ASSERT_TRUE(service.beginUpload(session.sessionId, "attempt-one"));
    EXPECT_FALSE(service.beginUpload(session.sessionId, "duplicate-attempt"));
    ASSERT_TRUE(service.setUploadValidating(session.sessionId, "attempt-one"));
    EXPECT_FALSE(service.beginUpload(session.sessionId, "duplicate-validating-attempt"));
    ASSERT_TRUE(service.finishUpload(session.sessionId, "attempt-one", false, "INVALID_MEDIA"));
    state = service.snapshot().value();
    EXPECT_EQ(state.uploadAttemptsUsed, 1);
    EXPECT_FALSE(state.hasAcceptedUpload);
    EXPECT_EQ(state.uploadReceipt.status, UploadStatus::rejected);
    EXPECT_GT(state.stateRevision, createdRevision);
    ASSERT_TRUE(service.beginUpload(session.sessionId, "attempt-two"));
    ASSERT_TRUE(service.setUploadValidating(session.sessionId, "attempt-two"));
    ASSERT_TRUE(service.finishUpload(session.sessionId, "attempt-two", true));
    EXPECT_TRUE(service.snapshot()->hasAcceptedUpload);
    EXPECT_FALSE(service.beginUpload(session.sessionId, "attempt-three"));
}
} // namespace RoadSign::RecognitionSession
