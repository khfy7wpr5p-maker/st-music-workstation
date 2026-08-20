#include "st/core/event_projection_link.hpp"

#include <algorithm>
#include <iostream>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

template <typename Id>
Id require_id(std::string_view text)
{
    const auto parsed = Id::parse(text);
    if (!parsed) {
        std::cerr << "FAIL: fixture ID did not parse: " << text << '\n';
        ++failures;
        return Id::parse("00000000000000000000000000000001").value.value();
    }
    return *parsed.value;
}

class TestValidationView final : public st::core::EventProjectionValidationView {
public:
    explicit TestValidationView(st::core::ProjectId project_id)
        : project_id_(std::move(project_id))
    {
    }

    [[nodiscard]] st::core::ProjectId project_id() const noexcept override
    {
        return project_id_;
    }

    [[nodiscard]] bool contains_event(
        const st::core::ScopedMusicalEventId& id) const noexcept override
    {
        return contains(events_, id);
    }

    [[nodiscard]] bool contains_score(
        const st::core::ScopedScoreEntityId& id) const noexcept override
    {
        return contains(scores_, id);
    }

    [[nodiscard]] bool contains_midi(
        const st::core::ScopedMidiEntityId& id) const noexcept override
    {
        return contains(midis_, id);
    }

    [[nodiscard]] bool contains_tab(
        const st::core::ScopedTabEntityId& id) const noexcept override
    {
        return contains(tabs_, id);
    }

    [[nodiscard]] bool contains_audio(
        const st::core::ScopedAudioEntityId& id) const noexcept override
    {
        return contains(audios_, id);
    }

    [[nodiscard]] bool contains_link(
        const st::core::EventProjectionLinkCandidate& link) const noexcept override
    {
        return contains(links_, link);
    }

    std::vector<st::core::ScopedMusicalEventId> events_;
    std::vector<st::core::ScopedScoreEntityId> scores_;
    std::vector<st::core::ScopedMidiEntityId> midis_;
    std::vector<st::core::ScopedTabEntityId> tabs_;
    std::vector<st::core::ScopedAudioEntityId> audios_;
    std::vector<st::core::EventProjectionLinkCandidate> links_;

private:
    template <typename T>
    [[nodiscard]] static bool contains(
        const std::vector<T>& values,
        const T& value) noexcept
    {
        return std::find(values.begin(), values.end(), value) != values.end();
    }

    st::core::ProjectId project_id_;
};

} // namespace

int main()
{
    using namespace st::core;

    static_assert(std::is_constructible_v<ProjectionScopedId, ScopedScoreEntityId>);
    static_assert(std::is_constructible_v<ProjectionScopedId, ScopedMidiEntityId>);
    static_assert(std::is_constructible_v<ProjectionScopedId, ScopedTabEntityId>);
    static_assert(std::is_constructible_v<ProjectionScopedId, ScopedAudioEntityId>);
    static_assert(!std::is_constructible_v<ProjectionScopedId, ScopedTrackId>);
    static_assert(!std::is_constructible_v<ProjectionScopedId, ScopedClipId>);
    static_assert(!std::is_constructible_v<ProjectionScopedId, ScopedMusicalEventId>);

    const auto project_a = require_id<ProjectId>("00000000000000000000000000000001");
    const auto project_b = require_id<ProjectId>("00000000000000000000000000000002");

    const ScopedMusicalEventId event_a{
        project_a,
        require_id<MusicalEventId>("00000000000000000000000000000011")};
    const ScopedMusicalEventId event_b{
        project_b,
        require_id<MusicalEventId>("00000000000000000000000000000011")};
    const ScopedMusicalEventId missing_event_a{
        project_a,
        require_id<MusicalEventId>("00000000000000000000000000000012")};

    const ScopedScoreEntityId score_a{
        project_a,
        require_id<ScoreEntityId>("00000000000000000000000000000021")};
    const ScopedScoreEntityId score_b{
        project_b,
        require_id<ScoreEntityId>("00000000000000000000000000000021")};
    const ScopedScoreEntityId missing_score_a{
        project_a,
        require_id<ScoreEntityId>("00000000000000000000000000000022")};
    const ScopedMidiEntityId midi_a{
        project_a,
        require_id<MidiEntityId>("00000000000000000000000000000031")};
    const ScopedTabEntityId tab_a{
        project_a,
        require_id<TabEntityId>("00000000000000000000000000000041")};
    const ScopedAudioEntityId audio_a{
        project_a,
        require_id<AudioEntityId>("00000000000000000000000000000051")};

    TestValidationView view{project_a};
    view.events_.push_back(event_a);
    view.scores_.push_back(score_a);
    view.midis_.push_back(midi_a);
    view.tabs_.push_back(tab_a);
    view.audios_.push_back(audio_a);

    const EventProjectionLinkCandidate score_link{event_a, score_a};
    const EventProjectionLinkCandidate midi_link{event_a, midi_a};
    const EventProjectionLinkCandidate tab_link{event_a, tab_a};
    const EventProjectionLinkCandidate audio_link{event_a, audio_a};

    check(score_link.projection_kind() == ProjectionKind::score, "score identity determines score projection kind");
    check(midi_link.projection_kind() == ProjectionKind::midi, "MIDI identity determines MIDI projection kind");
    check(tab_link.projection_kind() == ProjectionKind::guitar_tab, "TAB identity determines Guitar TAB projection kind");
    check(audio_link.projection_kind() == ProjectionKind::audio, "audio identity determines audio projection kind");

    check(static_cast<bool>(validate_event_projection_link_candidate(score_link, view)), "existing same-project score link candidate validates");
    check(static_cast<bool>(validate_event_projection_link_candidate(midi_link, view)), "existing same-project MIDI link candidate validates");
    check(static_cast<bool>(validate_event_projection_link_candidate(tab_link, view)), "existing same-project TAB link candidate validates");
    check(static_cast<bool>(validate_event_projection_link_candidate(audio_link, view)), "existing same-project audio link candidate validates");

    const EventProjectionLinkCandidate foreign_event{event_b, score_a};
    const auto foreign_event_result = validate_event_projection_link_candidate(foreign_event, view);
    check(!foreign_event_result, "foreign-project event is rejected");
    check(foreign_event_result.error == EventProjectionValidationError::event_wrong_project, "foreign event reports event_wrong_project");

    const EventProjectionLinkCandidate foreign_projection{event_a, score_b};
    const auto foreign_projection_result = validate_event_projection_link_candidate(foreign_projection, view);
    check(!foreign_projection_result, "foreign-project projection is rejected");
    check(foreign_projection_result.error == EventProjectionValidationError::projection_wrong_project, "foreign projection reports projection_wrong_project");

    const EventProjectionLinkCandidate fully_foreign{event_b, score_b};
    const auto fully_foreign_result = validate_event_projection_link_candidate(fully_foreign, view);
    check(!fully_foreign_result, "fully foreign link is rejected");
    check(fully_foreign_result.error == EventProjectionValidationError::event_wrong_project, "event scope check has deterministic precedence");

    const EventProjectionLinkCandidate missing_event{missing_event_a, score_a};
    const auto missing_event_result = validate_event_projection_link_candidate(missing_event, view);
    check(!missing_event_result, "missing event is rejected");
    check(missing_event_result.error == EventProjectionValidationError::event_missing, "missing event reports event_missing");

    const EventProjectionLinkCandidate missing_projection{event_a, missing_score_a};
    const auto missing_projection_result = validate_event_projection_link_candidate(missing_projection, view);
    check(!missing_projection_result, "missing projection is rejected");
    check(missing_projection_result.error == EventProjectionValidationError::projection_missing, "missing projection reports projection_missing");

    view.links_.push_back(score_link);
    const auto duplicate_result = validate_event_projection_link_candidate(score_link, view);
    check(!duplicate_result, "stored duplicate link is rejected");
    check(duplicate_result.error == EventProjectionValidationError::duplicate_link, "duplicate link reports duplicate_link");

    check(score_link.event_id().project_id() == project_a, "candidate retains event ProjectId");
    check(score_link.projection_project_id() == project_a, "candidate retains projection ProjectId");

    for (int iteration = 0; iteration < 10000; ++iteration) {
        const auto repeated = validate_event_projection_link_candidate(midi_link, view);
        check(static_cast<bool>(repeated), "repeated valid relation decision remains accepted");
        check(repeated.error == EventProjectionValidationError::none, "repeated valid relation decision remains deterministic");
    }

    return failures == 0 ? 0 : 1;
}
