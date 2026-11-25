#include "MediaAdmissionController.hpp"
#include "../../media-processing/public/OpenCvImageClient.hpp"

#include <drogon/HttpResponse.h>
#include <drogon/MultiPart.h>

#include <opencv2/videoio.hpp>
#include <uuid/uuid.h>

#include <array>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace RoadSign::RecognitionSession {
namespace {
using namespace drogon;
constexpr std::string_view kMediaDirectory = "/run/road-sign";

HttpResponsePtr jsonResponse(Json::Value body, HttpStatusCode status) {
    auto response = HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(status);
    response->addHeader("Cache-Control", "no-store");
    return response;
}

std::optional<std::uint32_t> parseUnsigned(std::string_view value) {
    std::uint32_t result{};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || end != value.data() + value.size()) return std::nullopt;
    return result;
}

std::optional<std::string> sessionIdFromUri(std::string_view uri) {
    constexpr std::string_view prefix = "/api/v1/sessions/";
    constexpr std::string_view suffix = "/media";
    if (!uri.starts_with(prefix) || !uri.ends_with(suffix)) return std::nullopt;
    uri.remove_prefix(prefix.size());
    uri.remove_suffix(suffix.size());
    if (uri.size() != 36 || uri.find('/') != std::string_view::npos) return std::nullopt;
    return std::string{uri};
}

const char* uploadStatusName(UploadStatus status) {
    switch (status) {
    case UploadStatus::idle: return "idle";
    case UploadStatus::receiving: return "receiving";
    case UploadStatus::validating: return "validating";
    case UploadStatus::accepted: return "accepted";
    case UploadStatus::rejected: return "rejected";
    case UploadStatus::aborted: return "aborted";
    }
    return "idle";
}

Json::Value uploadSnapshot(const Entity& session) {
    Json::Value body;
    body["epoch"] = session.epoch;
    body["stateRevision"] = Json::UInt64{session.stateRevision};
    if (session.uploadReceipt.uploadAttemptId)
        body["uploadAttemptId"] = *session.uploadReceipt.uploadAttemptId;
    else
        body["uploadAttemptId"] = Json::nullValue;
    body["uploadStatus"] = uploadStatusName(session.uploadReceipt.status);
    if (session.uploadReceipt.errorCode)
        body["errorCode"] = *session.uploadReceipt.errorCode;
    else
        body["errorCode"] = Json::nullValue;
    body["uploadAttemptsUsed"] = session.uploadAttemptsUsed;
    body["maxUploadAttempts"] = session.maxUploadAttempts;
    body["hasAcceptedUpload"] = session.hasAcceptedUpload;
    body["canRetryUpload"] = session.canAdmitUpload();
    return body;
}

HttpResponsePtr admissionError(UploadAdmissionResult::Status status,
                               const std::optional<Entity>& session) {
    Json::Value body;
    auto httpStatus = k409Conflict;
    switch (status) {
    case UploadAdmissionResult::Status::missing:
        body["code"] = "NOT_FOUND"; body["message"] = "Session was not found";
        httpStatus = k404NotFound; break;
    case UploadAdmissionResult::Status::forbidden:
        body["code"] = "FORBIDDEN"; body["message"] = "Request authentication failed";
        httpStatus = k403Forbidden; break;
    case UploadAdmissionResult::Status::invalidAttempt:
        body["code"] = "INVALID_REQUEST"; body["message"] = "Upload attempt ID is invalid";
        httpStatus = k400BadRequest; break;
    case UploadAdmissionResult::Status::wrongSource:
        body["code"] = "STATE_CONFLICT"; body["message"] = "Camera sessions do not accept files";
        break;
    case UploadAdmissionResult::Status::staleEpoch:
        body["code"] = "STALE_EPOCH"; body["message"] = "Session epoch is stale";
        break;
    case UploadAdmissionResult::Status::duplicateAttempt:
        body["code"] = "DUPLICATE_UPLOAD_ATTEMPT"; body["message"] = "Upload attempt was already used";
        break;
    case UploadAdmissionResult::Status::sourceAccepted:
        body["code"] = "UPLOAD_ALREADY_ACCEPTED"; body["message"] = "A file is already accepted";
        break;
    case UploadAdmissionResult::Status::exhausted:
        body["code"] = "UPLOAD_ATTEMPTS_EXHAUSTED"; body["message"] = "Upload attempts are exhausted";
        break;
    case UploadAdmissionResult::Status::busy:
        body["code"] = "BUSY"; body["message"] = "Another upload is active";
        break;
    case UploadAdmissionResult::Status::admitted:
        body["code"] = "INVALID_REQUEST"; body["message"] = "Upload admission failed";
        httpStatus = k500InternalServerError; break;
    }
    body["retryable"] = status == UploadAdmissionResult::Status::busy ||
        status == UploadAdmissionResult::Status::staleEpoch;
    if (session) body["uploadStatus"] = uploadSnapshot(*session);
    return jsonResponse(std::move(body), httpStatus);
}

std::string uuid() {
    uuid_t bytes;
    uuid_generate_random(bytes);
    char output[37]{};
    uuid_unparse_lower(bytes, output);
    return output;
}

std::optional<MediaProcessing::MediaSource> validateImage(
    const drogon::HttpFile& file, const Infrastructure::RuntimeConfig& config,
    const std::shared_ptr<const MediaProcessing::OwnedTemporaryFile>& ownedFile) {
    const MediaProcessing::ImageLimits limits{
        config.maxImageBytes, config.maxDecodedPixels,
        config.maxDecodedPixels, config.maxDecodedPixels};
    const auto decoded = MediaProcessing::OpenCvImageClient::decode(
        file.fileContent(), MediaProcessing::SourceKind::image, limits);
    if (!decoded) return std::nullopt;
    return MediaProcessing::MediaSource{
        MediaProcessing::SourceKind::image,
        decoded->size.width, decoded->size.height,
        std::nullopt, std::nullopt, ownedFile};
}

bool isMp4(std::string_view bytes) {
    return bytes.size() >= 12 && bytes.substr(4, 4) == "ftyp";
}

bool isWebm(std::string_view bytes) {
    constexpr std::array<unsigned char, 4> signature{0x1A, 0x45, 0xDF, 0xA3};
    return bytes.size() >= signature.size() &&
        std::memcmp(bytes.data(), signature.data(), signature.size()) == 0;
}

std::string videoCodec(const cv::VideoCapture& capture) {
    const auto code = static_cast<std::uint32_t>(capture.get(cv::CAP_PROP_FOURCC));
    std::string value(4, '\0');
    for (std::size_t index = 0; index < value.size(); ++index)
        value[index] = static_cast<char>((code >> (index * 8U)) & 0xFFU);
    for (auto& character : value)
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    return value;
}

std::optional<MediaProcessing::MediaSource> validateVideo(
    std::string_view bytes, const Infrastructure::RuntimeConfig& config,
    const std::shared_ptr<const MediaProcessing::OwnedTemporaryFile>& ownedFile) {
    if (bytes.empty() || bytes.size() > config.maxVideoBytes) return std::nullopt;
    const bool isMp4File = isMp4(bytes);
    const bool isWebmFile = isWebm(bytes);
    if (!isMp4File && !isWebmFile) return std::nullopt;
    cv::VideoCapture capture(ownedFile->path.string(), cv::CAP_FFMPEG);
    if (!capture.isOpened()) return std::nullopt;
    const auto width = capture.get(cv::CAP_PROP_FRAME_WIDTH);
    const auto height = capture.get(cv::CAP_PROP_FRAME_HEIGHT);
    const auto frameRate = capture.get(cv::CAP_PROP_FPS);
    const auto frameCount = capture.get(cv::CAP_PROP_FRAME_COUNT);
    const auto duration = frameCount / frameRate;
    const auto codec = videoCodec(capture);
    const bool supportedCodec = isMp4File
        ? codec == "avc1" || codec == "avc3" || codec == "h264"
        : codec == "vp80" || codec == "vp90" || codec == "vp08" || codec == "vp09";
    if (!supportedCodec || !std::isfinite(width) || !std::isfinite(height) ||
        !std::isfinite(frameRate) || !std::isfinite(frameCount) || !std::isfinite(duration) ||
        width < 1 || height < 1 || width > config.maxVideoWidth ||
        height > config.maxVideoHeight || frameRate < 1 ||
        frameRate > config.maxVideoFramesPerSecond || frameCount < 1 || duration > config.maxVideoDurationSeconds)
        return std::nullopt;
    cv::Mat firstFrame;
    if (!capture.read(firstFrame) || firstFrame.empty()) return std::nullopt;
    return MediaProcessing::MediaSource{
        MediaProcessing::SourceKind::video,
        static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height),
        static_cast<std::uint64_t>(duration * 1000.0), 0, ownedFile};
}

std::shared_ptr<const MediaProcessing::OwnedTemporaryFile> saveTemporaryMedia(
    const drogon::HttpFile& file, std::string_view suffix) {
    const auto path = std::filesystem::path{kMediaDirectory} / (uuid() + std::string{suffix});
    if (file.saveAs(path.string()) != 0) {
        std::error_code error;
        std::filesystem::remove(path, error);
        return {};
    }
    return std::make_shared<MediaProcessing::OwnedTemporaryFile>(path);
}

HttpResponsePtr uploadError(SessionService& sessions, const std::string& sessionId,
                            const std::string& attemptId, const std::string& code,
                            const std::string& message, HttpStatusCode status,
                            bool finishAttempt) {
    if (finishAttempt)
        static_cast<void>(sessions.finishUpload(sessionId, attemptId, false, code));
    Json::Value body;
    body["code"] = code; body["message"] = message; body["retryable"] = true;
    if (const auto session = sessions.snapshot(); session && session->sessionId == sessionId)
        body["uploadStatus"] = uploadSnapshot(*session);
    return jsonResponse(std::move(body), status);
}

} // namespace

void registerMediaAdmissionRoutes(HttpAppFramework& app, SessionService& sessions,
                                  const Infrastructure::RuntimeConfig& config) {
    app.registerHandler("/internal/admit-media", [&sessions, &config](const HttpRequestPtr& request,
        std::function<void(const HttpResponsePtr&)>&& callback) {
        if (request->getHeader("x-forwarded-method") != "POST") {
            Json::Value body; body["code"] = "FORBIDDEN"; body["message"] = "Admission is internal";
            body["retryable"] = false; callback(jsonResponse(std::move(body), k403Forbidden)); return;
        }
        const auto id = sessionIdFromUri(request->getHeader("x-forwarded-uri"));
        const auto cookie = request->getCookie("rs_session");
        const auto session = sessions.snapshot();
        if (!id || cookie.empty() || !session || session->sessionId != *id ||
            cookie != session->cookieToken) {
            Json::Value body; body["code"] = "NOT_FOUND";
            body["message"] = "Session was not found"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k404NotFound)); return;
        }
        const auto epoch = parseUnsigned(request->getHeader("x-epoch"));
        if (!epoch) {
            Json::Value body; body["code"] = "INVALID_REQUEST";
            body["message"] = "Upload epoch is invalid"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k400BadRequest)); return;
        }
        auto admission = sessions.admitUpload(*id, request->getHeader("x-csrf-token"),
            request->getHeader("origin"), *epoch, request->getHeader("x-upload-attempt-id"),
            std::chrono::steady_clock::now());
        if (admission.status != UploadAdmissionResult::Status::admitted) {
            callback(admissionError(admission.status, admission.snapshot)); return;
        }
        auto response = HttpResponse::newHttpResponse();
        response->setStatusCode(k200OK);
        response->addHeader("Cache-Control", "no-store");
        response->addHeader("X-Upload-Admission-Nonce", *admission.nonce);
        callback(response);
    }, {Get});

    app.registerHandler("/api/v1/sessions/{1}/media", [&sessions, &config](
        const HttpRequestPtr& request, std::function<void(const HttpResponsePtr&)>&& callback,
        const std::string& id) {
        const auto cookie = request->getCookie("rs_session");
        const auto owned = sessions.snapshot();
        if (cookie.empty() || !owned || owned->sessionId != id ||
            cookie != owned->cookieToken ||
            request->getHeader("origin") != config.trustedOrigin ||
            request->getHeader("x-csrf-token").empty()) {
            Json::Value body; body["code"] = "FORBIDDEN";
            body["message"] = "Request authentication failed"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k403Forbidden)); return;
        }
        const auto epoch = parseUnsigned(request->getHeader("x-epoch"));
        const auto attemptId = request->getHeader("x-upload-attempt-id");
        if (!epoch) {
            Json::Value body; body["code"] = "INVALID_REQUEST";
            body["message"] = "Upload epoch is invalid"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k400BadRequest)); return;
        }
        const auto session = sessions.snapshot();
        if (!session || session->sessionId != id) {
            Json::Value body; body["code"] = "NOT_FOUND";
            body["message"] = "Session was not found"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k404NotFound)); return;
        }
        const auto nonce = request->getHeader("x-upload-admission-nonce");
        if (!sessions.authenticate(id, request->getHeader("x-csrf-token"),
                                  request->getHeader("origin")) || *epoch != session->epoch ||
            !sessions.setUploadValidating(id, attemptId, nonce, std::chrono::steady_clock::now())) {
            callback(uploadError(sessions, id, attemptId, "ADMISSION_INVALID",
                                 "Upload admission expired or is invalid", k409Conflict, false));
            return;
        }
        MultiPartParser parser;
        if (parser.parse(request) != 0 || parser.getFiles().size() != 1 ||
            !parser.getParameters().empty() || parser.getFiles().front().getItemName() != "file") {
            callback(uploadError(sessions, id, attemptId, "INVALID_MEDIA",
                                 "Upload must contain one media file", k422UnprocessableEntity, true));
            return;
        }
        const auto& file = parser.getFiles().front();
        if (session->sourceKind == SourceKind::image) {
            const auto bytes = file.fileContent();
            const auto suffix = MediaProcessing::OpenCvImageClient::isPng(bytes) ? ".png" : ".jpg";
            auto ownedFile = saveTemporaryMedia(file, suffix);
            if (!ownedFile) {
                callback(uploadError(sessions, id, attemptId, "UPLOAD_STORAGE_FAILED",
                                     "Upload could not be stored", k503ServiceUnavailable, true));
                return;
            }
            auto media = validateImage(file, config, ownedFile);
            if (!media || !sessions.finishUpload(id, attemptId, true, std::nullopt, std::move(media))) {
                ownedFile.reset();
                callback(uploadError(sessions, id, attemptId, "INVALID_MEDIA",
                                     "Image format or dimensions are unsupported",
                                     k422UnprocessableEntity, true));
                return;
            }
        } else {
            const auto bytes = file.fileContent();
            const std::string_view suffix = isMp4(bytes) ? ".mp4" : ".webm";
            auto ownedFile = saveTemporaryMedia(file, suffix);
            if (!ownedFile) {
                callback(uploadError(sessions, id, attemptId, "UPLOAD_STORAGE_FAILED",
                                     "Upload could not be stored", k503ServiceUnavailable, true));
                return;
            }
            auto media = validateVideo(bytes, config, ownedFile);
            if (!media || !sessions.finishUpload(id, attemptId, true, std::nullopt, std::move(media))) {
                ownedFile.reset();
                callback(uploadError(sessions, id, attemptId, "INVALID_MEDIA",
                                     "Video format, codec or dimensions are unsupported",
                                     k422UnprocessableEntity, true));
                return;
            }
        }
        const auto accepted = sessions.snapshot();
        if (!accepted || accepted->sessionId != id) {
            Json::Value body; body["code"] = "NOT_FOUND";
            body["message"] = "Session was not found"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k404NotFound)); return;
        }
        callback(jsonResponse(uploadSnapshot(*accepted), k202Accepted));
    }, {Post});
}
} // namespace RoadSign::RecognitionSession
