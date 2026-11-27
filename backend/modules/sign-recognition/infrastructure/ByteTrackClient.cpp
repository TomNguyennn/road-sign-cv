#include "ByteTrackClient.hpp"

#include <BYTETracker.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace RoadSign::SignRecognition {
namespace {
constexpr std::size_t CATEGORY_COUNT = 8;
constexpr std::size_t MAX_TRACKS = 100;
constexpr double TRACK_RETENTION_MS = 2000.0;
constexpr double MIN_TRACK_SCORE = 0.10;
constexpr double MIN_NEW_SCORE = 0.60;
constexpr double MAX_ASSOCIATION_COST = 0.80;
using VendorKey = std::pair<std::uint32_t, int>;

double intersectionOverUnion(Box first, Box second) noexcept {
    const double left = std::max(first.x, second.x);
    const double top = std::max(first.y, second.y);
    const double right = std::min(first.x + first.width, second.x + second.width);
    const double bottom = std::min(first.y + first.height, second.y + second.height);
    const double intersection = std::max(0.0, right - left) * std::max(0.0, bottom - top);
    const double total = first.width * first.height + second.width * second.height - intersection;
    if (!std::isfinite(total) || total <= 0.0) return 0.0;
    return intersection / total;
}

Box vendorBox(const STrack& track) noexcept {
    return {track.tlbr[0], track.tlbr[1], track.tlbr[2] - track.tlbr[0],
            track.tlbr[3] - track.tlbr[1]};
}

std::optional<std::size_t> matchingObservation(
    const STrack& track, const std::vector<std::size_t>& candidates,
    const std::vector<SignObservation>& observations, std::set<std::size_t>& used,
    ImageSize frameSize) {
    const auto predicted = vendorBox(track).clipped(frameSize);
    if (!predicted) return std::nullopt;
    double bestOverlap = 1.0 - MAX_ASSOCIATION_COST;
    std::optional<std::size_t> best;
    for (const auto index : candidates) {
        if (used.contains(index)) continue;
        const auto visible = observations[index].box.clipped(frameSize);
        if (!visible) continue;
        const double overlap = intersectionOverUnion(*predicted, *visible);
        if (overlap < bestOverlap) continue;
        bestOverlap = overlap;
        best = index;
    }
    if (best) used.insert(*best);
    return best;
}
} // namespace

struct ByteTrackClient::State {
    std::string sessionId;
    std::uint32_t epoch;
    std::uint64_t nextLocalId{1};
    std::optional<double> lastSourceTimeMs;
    std::array<std::unique_ptr<BYTETracker>, CATEGORY_COUNT> trackers;
    std::map<VendorKey, TrackedSign> tracks;

    State(std::string id, std::uint32_t value) : sessionId(std::move(id)), epoch(value) {
        if (sessionId.empty() || epoch == 0) throw std::invalid_argument("Invalid tracking session");
        resetTrackers();
    }

    void resetTrackers() {
        for (auto& tracker : trackers) tracker = std::make_unique<BYTETracker>(30, 60);
    }

    void expire(double sourceTimeMs) {
        for (auto it = tracks.begin(); it != tracks.end();) {
            it->second.markLost(sourceTimeMs);
            if (it->second.state() == TrackedSign::State::removed)
                it = tracks.erase(it);
            else
                ++it;
        }
    }

    std::array<std::vector<std::size_t>, CATEGORY_COUNT> select(
        const std::vector<SignObservation>& observations, double sourceTimeMs,
        ImageSize frameSize) const {
        std::array<std::vector<std::size_t>, CATEGORY_COUNT> selected;
        std::vector<std::size_t> newCandidates;
        std::map<VendorKey, std::size_t> knownCandidates;
        for (std::size_t index = 0; index < observations.size(); ++index) {
            const auto& candidate = observations[index];
            if (candidate.sourceTimeMs != sourceTimeMs || candidate.confidence < MIN_TRACK_SCORE ||
                !candidate.isValid(epoch, epoch, frameSize)) continue;
            const auto known = std::find_if(tracks.begin(), tracks.end(), [&](const auto& entry) {
                return entry.second.canAssociate(candidate, epoch, frameSize);
            });
            if (known != tracks.end()) {
                const auto previous = knownCandidates.find(known->first);
                if (previous == knownCandidates.end() ||
                    candidate.confidence > observations[previous->second].confidence)
                    knownCandidates[known->first] = index;
            } else if (candidate.confidence >= MIN_NEW_SCORE) {
                newCandidates.push_back(index);
            }
        }
        for (const auto& [key, index] : knownCandidates)
            selected[key.first].push_back(index);
        std::sort(newCandidates.begin(), newCandidates.end(), [&](auto first, auto second) {
            if (observations[first].confidence == observations[second].confidence)
                return first < second;
            return observations[first].confidence > observations[second].confidence;
        });
        const auto available = MAX_TRACKS - tracks.size();
        for (std::size_t index = 0; index < std::min(available, newCandidates.size()); ++index) {
            const auto observationIndex = newCandidates[index];
            selected[observations[observationIndex].categoryId].push_back(observationIndex);
        }
        return selected;
    }

    std::optional<std::string> accept(const STrack& vendorTrack,
                                      const SignObservation& observation,
                                      ImageSize frameSize) {
        const VendorKey key{observation.categoryId, vendorTrack.track_id};
        auto existing = tracks.find(key);
        if (existing != tracks.end()) {
            if (existing->second.observe(observation, epoch, frameSize))
                return existing->second.trackId();
            tracks.erase(existing);
        }
        if (tracks.size() >= MAX_TRACKS || observation.confidence < MIN_NEW_SCORE ||
            nextLocalId == std::numeric_limits<std::uint64_t>::max()) return std::nullopt;
        auto created = TrackedSign::start(sessionId, epoch, nextLocalId, observation, frameSize);
        if (!created) return std::nullopt;
        ++nextLocalId;
        auto [inserted, isInserted] = tracks.emplace(key, std::move(*created));
        if (!isInserted) return std::nullopt;
        return inserted->second.trackId();
    }

    void processCategory(std::size_t category, const std::vector<std::size_t>& selected,
                         const std::vector<SignObservation>& observations,
                         double sourceTimeMs, ImageSize frameSize,
                         std::set<VendorKey>& touched,
                         std::vector<TrackAssignment>& assignments) {
        std::vector<Object> objects;
        for (const auto index : selected) {
            const auto box = *observations[index].box.clipped(frameSize);
            objects.push_back({cv::Rect2f{static_cast<float>(box.x), static_cast<float>(box.y),
                                          static_cast<float>(box.width),
                                          static_cast<float>(box.height)},
                               static_cast<int>(category),
                               static_cast<float>(observations[index].confidence)});
        }
        auto vendorTracks = trackers[category]->update(objects, sourceTimeMs);
        std::set<std::size_t> used;
        for (const auto& vendorTrack : vendorTracks) {
            const auto index = matchingObservation(vendorTrack, selected, observations,
                                                   used, frameSize);
            if (!index) continue;
            const auto trackId = accept(vendorTrack, observations[*index], frameSize);
            if (!trackId) continue;
            touched.emplace(static_cast<std::uint32_t>(category), vendorTrack.track_id);
            assignments.push_back({*index, *trackId});
        }
    }
};

ByteTrackClient::ByteTrackClient(std::string sessionId, std::uint32_t epoch)
    : state_(std::make_unique<State>(std::move(sessionId), epoch)) {}

ByteTrackClient::~ByteTrackClient() = default;

std::optional<std::vector<TrackAssignment>> ByteTrackClient::update(
    const std::vector<SignObservation>& observations, double sourceTimeMs, ImageSize frameSize) {
    if (!std::isfinite(sourceTimeMs) || sourceTimeMs < 0.0 ||
        !frameSize.pixelBytes(1) ||
        (state_->lastSourceTimeMs && sourceTimeMs <= *state_->lastSourceTimeMs))
        return std::nullopt;
    if (state_->lastSourceTimeMs &&
        sourceTimeMs - *state_->lastSourceTimeMs > TRACK_RETENTION_MS)
        state_->resetTrackers();
    state_->expire(sourceTimeMs);
    const auto selected = state_->select(observations, sourceTimeMs, frameSize);
    std::vector<TrackAssignment> assignments;
    std::set<VendorKey> touched;
    for (std::size_t category = 0; category < CATEGORY_COUNT; ++category)
        state_->processCategory(category, selected[category], observations, sourceTimeMs,
                                frameSize, touched, assignments);
    for (auto& [key, track] : state_->tracks)
        if (!touched.contains(key)) track.markLost(sourceTimeMs);
    state_->lastSourceTimeMs = sourceTimeMs;
    std::sort(assignments.begin(), assignments.end(), [](const auto& first, const auto& second) {
        return first.observationIndex < second.observationIndex;
    });
    return assignments;
}

const TrackedSign* ByteTrackClient::find(std::string_view trackId) const noexcept {
    for (const auto& [key, track] : state_->tracks)
        if (track.trackId() == trackId) return &track;
    return nullptr;
}

std::size_t ByteTrackClient::trackCount() const noexcept { return state_->tracks.size(); }

void ByteTrackClient::reset(std::string sessionId, std::uint32_t epoch) {
    if (sessionId == state_->sessionId && epoch <= state_->epoch)
        throw std::invalid_argument("Tracking epoch must advance");
    state_ = std::make_unique<State>(std::move(sessionId), epoch);
}
} // namespace RoadSign::SignRecognition
