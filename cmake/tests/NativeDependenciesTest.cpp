#include <BYTETracker.h>
#include <drogon/HttpResponse.h>
#include <espeak-ng/speak_lib.h>
#include <gtest/gtest.h>
#include <opencv2/imgcodecs.hpp>
#include <tesseract/baseapi.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <cstdint>
#include <regex>
#include <vector>

namespace {
constexpr int IMAGE_ROWS = 2;
constexpr int IMAGE_COLUMNS = 3;
constexpr int PIXEL_VALUE = 10;
constexpr int PIXEL_VALUE_SECOND = 20;
constexpr int PIXEL_VALUE_THIRD = 30;
constexpr int TRACKER_FRAME_RATE = 30;
constexpr int TRACKER_BUFFER_LENGTH = 30;
} // namespace

TEST(NativeDependencies, ImageCodecsAndVideoBackendAreAvailable) {
    const cv::Mat image{IMAGE_ROWS, IMAGE_COLUMNS, CV_8UC3,
                        cv::Scalar{PIXEL_VALUE, PIXEL_VALUE_SECOND, PIXEL_VALUE_THIRD}};
    std::vector<unsigned char> encoded;
    ASSERT_TRUE(cv::imencode(".png", image, encoded));
    const auto decoded = cv::imdecode(encoded, cv::IMREAD_COLOR);
    EXPECT_EQ(decoded.size(), image.size());
    EXPECT_EQ(cv::norm(image, decoded, cv::NORM_INF), 0);
    EXPECT_TRUE(std::regex_search(cv::getBuildInformation(), std::regex{"FFMPEG:[ ]+YES"}));
    EXPECT_NE(avcodec_find_decoder(AV_CODEC_ID_H264), nullptr);
    EXPECT_NE(avcodec_find_decoder(AV_CODEC_ID_VP8), nullptr);
    EXPECT_NE(avcodec_find_decoder(AV_CODEC_ID_VP9), nullptr);
}

TEST(NativeDependencies, NativeOcrSpeechHttpAndTrackerLibrariesLoad) {
    EXPECT_STREQ(tesseract::TessBaseAPI::Version(), "5.5.0");
    EXPECT_STREQ(espeak_Info(nullptr), "1.52.0");
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(drogon::k202Accepted);
    EXPECT_EQ(response->statusCode(), drogon::k202Accepted);
    BYTETracker tracker{TRACKER_FRAME_RATE, TRACKER_BUFFER_LENGTH};
    EXPECT_TRUE(tracker.update({}, 0.0).empty());
}
