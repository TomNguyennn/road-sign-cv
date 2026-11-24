#include "../application/SessionService.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

namespace RoadSign::RecognitionSession {
namespace {
MediaProcessing::MediaSource testImageSource() {
    return {MediaProcessing::SourceKind::image, 2, 2, std::nullopt, std::nullopt, nullptr};
}
} // namespace

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
    EXPECT_EQ(session.cookieToken.size(), 64);
    EXPECT_NE(session.cookieToken, session.sessionId);
    EXPECT_NE(session.cookieToken, session.csrfToken);
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

TEST(SessionFoundationTest, AdmissionBindsOwnerEpochNonceAndRetryBudget) {
    SessionService service{2, "https://demo.example"};
    const auto now = std::chrono::steady_clock::now();
    const auto session = service.create(SourceKind::image, "https://demo.example", now).session.value();
    ASSERT_TRUE(service.attachEvents(session.sessionId, now));
    constexpr std::string_view firstId = "edc6258a-a013-467e-ace1-865af6048911";
    constexpr std::string_view secondId = "d37f3719-c17a-4c7c-9a3d-8441d4f42722";
    EXPECT_EQ(service.admitUpload(session.sessionId, "wrong", "https://demo.example", 1,
                                  std::string{firstId}, now).status,
              UploadAdmissionResult::Status::forbidden);
    EXPECT_EQ(service.admitUpload(session.sessionId, session.csrfToken, "https://demo.example", 2,
                                  std::string{firstId}, now).status,
              UploadAdmissionResult::Status::staleEpoch);
    EXPECT_EQ(service.admitUpload(session.sessionId, session.csrfToken, "https://demo.example", 1,
                                  "not-a-uuid", now).status,
              UploadAdmissionResult::Status::invalidAttempt);

    auto state = service.snapshot().value();
    const auto createdRevision = state.stateRevision;
    auto first = service.admitUpload(session.sessionId, session.csrfToken, "https://demo.example",
                                     1, std::string{firstId}, now);
    ASSERT_EQ(first.status, UploadAdmissionResult::Status::admitted);
    ASSERT_TRUE(first.nonce);
    EXPECT_EQ(service.admitUpload(session.sessionId, session.csrfToken, "https://demo.example", 1,
                                  std::string{firstId}, now).status,
              UploadAdmissionResult::Status::duplicateAttempt);
    EXPECT_EQ(service.admitUpload(session.sessionId, session.csrfToken, "https://demo.example", 1,
                                  std::string{secondId}, now).status,
              UploadAdmissionResult::Status::busy);
    EXPECT_FALSE(service.setUploadValidating(session.sessionId, firstId, "wrong-nonce", now));
    ASSERT_TRUE(service.setUploadValidating(session.sessionId, firstId, *first.nonce, now));
    ASSERT_TRUE(service.finishUpload(session.sessionId, firstId, false, "INVALID_MEDIA"));
    state = service.snapshot().value();
    EXPECT_EQ(state.uploadAttemptsUsed, 1);
    EXPECT_FALSE(state.hasAcceptedUpload);
    EXPECT_EQ(state.uploadReceipt.status, UploadStatus::rejected);
    EXPECT_GT(state.stateRevision, createdRevision);
    auto second = service.admitUpload(session.sessionId, session.csrfToken, "https://demo.example",
                                      1, std::string{secondId}, now + std::chrono::seconds{1});
    ASSERT_EQ(second.status, UploadAdmissionResult::Status::admitted);
    ASSERT_TRUE(second.nonce);
    ASSERT_TRUE(service.setUploadValidating(session.sessionId, secondId, *second.nonce,
                                            now + std::chrono::seconds{1}));
    ASSERT_TRUE(service.finishUpload(session.sessionId, secondId, true, std::nullopt,
                                     testImageSource()));
    EXPECT_TRUE(service.snapshot()->hasAcceptedUpload);
    EXPECT_TRUE(service.snapshot()->acceptedMedia);
    EXPECT_EQ(service.admitUpload(session.sessionId, session.csrfToken, "https://demo.example", 1,
                                  "a4f9ad68-f9e5-49dd-b411-6441a4d50955",
                                  now + std::chrono::seconds{2}).status,
              UploadAdmissionResult::Status::sourceAccepted);
}

TEST(SessionFoundationTest, ExpiredUploadAdmissionAbortsAndEndsSession) {
    SessionService service{3, "https://demo.example"};
    const auto now = std::chrono::steady_clock::now();
    const auto session = service.create(SourceKind::video, "https://demo.example", now).session.value();
    ASSERT_TRUE(service.attachEvents(session.sessionId, now));
    auto admission = service.admitUpload(session.sessionId, session.csrfToken,
        "https://demo.example", 1, "edc6258a-a013-467e-ace1-865af6048911", now);
    ASSERT_EQ(admission.status, UploadAdmissionResult::Status::admitted);
    EXPECT_FALSE(service.expireUploadAdmission(now + std::chrono::seconds{64}));
    EXPECT_EQ(service.expireUploadAdmission(now + std::chrono::seconds{65}), session.sessionId);
    const auto expired = service.snapshot();
    ASSERT_TRUE(expired);
    EXPECT_EQ(expired->state, State::ending);
    EXPECT_EQ(expired->uploadReceipt.status, UploadStatus::aborted);
    EXPECT_EQ(expired->uploadReceipt.errorCode, "UPLOAD_TIMEOUT");
    EXPECT_FALSE(service.create(SourceKind::image, "https://demo.example", now).session);
    EXPECT_TRUE(service.completeCleanup(session.sessionId));
}

TEST(SessionFoundationTest, AdmissionRejectionsDoNotIncrementUploadAttempts) {
    SessionService service{1, "https://demo.example"};
    const auto now = std::chrono::steady_clock::now();
    const auto session = service.create(SourceKind::image, "https://demo.example", now).session.value();
    ASSERT_TRUE(service.attachEvents(session.sessionId, now));
    EXPECT_EQ(service.admitUpload(session.sessionId, session.csrfToken, "https://demo.example", 1,
        "edc6258a-a013-467e-ace1-865af6048911", now).status,
        UploadAdmissionResult::Status::admitted);
    EXPECT_EQ(service.snapshot()->uploadAttemptsUsed, 1);
    EXPECT_EQ(service.admitUpload(session.sessionId, session.csrfToken, "https://demo.example", 1,
        "d37f3719-c17a-4c7c-9a3d-8441d4f42722", now).status,
        UploadAdmissionResult::Status::exhausted);
    EXPECT_EQ(service.snapshot()->uploadAttemptsUsed, 1);
}
} // namespace RoadSign::RecognitionSession
