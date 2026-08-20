#include "st/core/event_projection_mutation.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>
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
        return *Id::parse("00000000000000000000000000000001").value;
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
        ++project_id_calls_;
        return project_id_;
    }

    [[nodiscard]] bool contains_event(
        const st::core::ScopedMusicalEventId& id) const noexcept override
    {
        ++event_calls_;
        return contains(events_, id);
    }

    [[nodiscard]] bool contains_score(
        const st::core::ScopedScoreEntityId& id) const noexcept override
    {
        ++projection_calls_;
        return contains(scores_, id);
    }

    [[nodiscard]] bool contains_midi(
        const st::core::ScopedMidiEntityId&) const noexcept override
    {
        ++projection_calls_;
        return false;
    }

    [[nodiscard]] bool contains_tab(
        const st::core::ScopedTabEntityId&) const noexcept override
    {
        ++projection_calls_;
        return false;
    }

    [[nodiscard]] bool contains_audio(
        const st::core::ScopedAudioEntityId&) const noexcept override
    {
        ++projection_calls_;
        return false;
    }

    [[nodiscard]] bool contains_link(
        const st::core::EventProjectionLinkCandidate& link) const noexcept override
    {
        ++link_calls_;
        return contains(links_, link);
    }

    void reset_calls() const noexcept
    {
        project_id_calls_ = 0U;
        event_calls_ = 0U;
        projection_calls_ = 0U;
        link_calls_ = 0U;
    }

    [[nodiscard]] std::size_t project_id_calls() const noexcept
    {
        return project_id_calls_;
    }

    [[nodiscard]] std::size_t event_calls() const noexcept
    {
        return event_calls_;
    }

    [[nodiscard]] std::size_t projection_calls() const noexcept
    {
        return projection_calls_;
    }

    [[nodiscard]] std::size_t link_calls() const noexcept
    {
        return link_calls_;
    }

    std::vector<st::core::ScopedMusicalEventId> events_;
    std::vector<st::core::ScopedScoreEntityId> scores_;
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
    mutable std::size_t project_id_calls_{0U};
    mutable std::size_t event_calls_{0U};
    mutable std::size_t projection_calls_{0U};
    mutable std::size_t link_calls_{0U};
};

} // namespace

int main()
{
    using namespace st::core;

    const auto project_a = require_id<ProjectId>("00000000000000000000000000000001");
    const auto project_b = require_id<ProjectId>("00000000000000000000000000000002");
    const ScopedMusicalEventId event_a{
        project_a,
        require_id<MusicalEventId>("00000000000000000000000000000011")};
    const ScopedMusicalEventId event_b{
        project_b,
        require_id<MusicalEventId>("00000000000000000000000000000011")};
    const ScopedScoreEntityId score_a{
        project_a,
        require_id<ScoreEntityId>("00000000000000000000000000000021")};
    const ScopedScoreEntityId missing_score_a{
        project_a,
        require_id<ScoreEntityId>("00000000000000000000000000000022")};

    TestValidationView view{project_a};
    view.events_.push_back(event_a);
    view.scores_.push_back(score_a);

    const EventProjectionLinkCandidate valid_link{event_a, score_a};
    const EventProjectionLinkCandidate foreign_event_link{event_b, score_a};
    const EventProjectionLinkCandidate missing_projection_link{event_a, missing_score_a};
    const auto current = ProjectRevision::from_persisted(7U);

    {
        view.reset_calls();
        const auto result = prepare_event_projection_link_addition(
            valid_link,
            view,
            current,
            current);
        check(static_cast<bool>(result), "valid current relation prepares successfully");
        check(result.error == EventProjectionMutationPreparationError::none, "successful preparation has no error");
        check(result.value->base_snapshot.matches(project_a, current), "prepared plan records exact base ProjectId and revision");
        check(result.value->next_revision.value() == 8U, "prepared plan advances revision exactly once");
        check(result.value->link == valid_link, "prepared plan preserves the exact validated relation");
        check(view.links_.empty(), "preparation does not mutate authoritative relation storage");
        check(view.project_id_calls() == 1U, "successful preparation reads validation-view ProjectId only once");
        check(view.event_calls() == 1U, "successful preparation checks event existence once");
        check(view.projection_calls() == 1U, "successful preparation checks projection existence once");
        check(view.link_calls() == 1U, "successful preparation checks exact duplicate once");
    }

    {
        view.reset_calls();
        const auto stale = prepare_event_projection_link_addition(
            missing_projection_link,
            view,
            current,
            ProjectRevision::from_persisted(6U));
        check(!stale, "stale expected revision is rejected");
        check(stale.error == EventProjectionMutationPreparationError::stale_expected_revision, "stale revision has deterministic top precedence");
        check(view.project_id_calls() == 0U, "stale revision rejects before relation validation begins");
        check(view.event_calls() == 0U && view.projection_calls() == 0U && view.link_calls() == 0U, "stale revision performs no endpoint or duplicate lookup");
    }

    {
        view.reset_calls();
        const auto foreign = prepare_event_projection_link_addition(
            foreign_event_link,
            view,
            current,
            current);
        check(!foreign, "foreign event relation is rejected");
        check(foreign.error == EventProjectionMutationPreparationError::event_wrong_project, "relation validation error is preserved through preparation");
    }

    {
        view.reset_calls();
        const auto missing = prepare_event_projection_link_addition(
            missing_projection_link,
            view,
            current,
            current);
        check(!missing, "missing projection relation is rejected");
        check(missing.error == EventProjectionMutationPreparationError::projection_missing, "missing projection error is preserved");
    }

    {
        view.links_.push_back(valid_link);
        view.reset_calls();
        const auto duplicate = prepare_event_projection_link_addition(
            valid_link,
            view,
            current,
            current);
        check(!duplicate, "duplicate relation is rejected");
        check(duplicate.error == EventProjectionMutationPreparationError::duplicate_link, "duplicate relation error is preserved");
        view.links_.clear();
    }

    const auto maximum = ProjectRevision::from_persisted(
        std::numeric_limits<ProjectRevision::Value>::max());

    {
        view.reset_calls();
        const auto overflow = prepare_event_projection_link_addition(
            valid_link,
            view,
            maximum,
            maximum);
        check(!overflow, "valid relation at terminal revision cannot prepare publication");
        check(overflow.error == EventProjectionMutationPreparationError::revision_overflow, "terminal revision fails closed with revision_overflow");
        check(view.event_calls() == 1U && view.projection_calls() == 1U && view.link_calls() == 1U, "relation is fully validated before next-revision overflow is reported");
    }

    {
        view.reset_calls();
        const auto invalid_before_overflow = prepare_event_projection_link_addition(
            missing_projection_link,
            view,
            maximum,
            maximum);
        check(!invalid_before_overflow, "invalid relation at terminal revision is rejected");
        check(invalid_before_overflow.error == EventProjectionMutationPreparationError::projection_missing, "relation failure precedes overflow after revision precondition matches");
    }

    {
        view.reset_calls();
        const auto stale_before_everything = prepare_event_projection_link_addition(
            missing_projection_link,
            view,
            maximum,
            ProjectRevision::from_persisted(maximum.value() - 1U));
        check(!stale_before_everything, "stale terminal-revision request is rejected");
        check(stale_before_everything.error == EventProjectionMutationPreparationError::stale_expected_revision, "stale revision precedes relation failure and overflow");
        check(view.project_id_calls() == 0U, "stale terminal-revision request does not inspect relation state");
    }

    check(
        map_projection_validation_error(
            static_cast<EventProjectionValidationError>(255U)) ==
            EventProjectionMutationPreparationError::relation_validation_failure,
        "unknown relation-validation code fails closed to a generic validation failure");

    for (int iteration = 0; iteration < 10000; ++iteration) {
        const auto repeated = prepare_event_projection_link_addition(
            valid_link,
            view,
            current,
            current);
        check(static_cast<bool>(repeated), "repeated preparation remains successful");
        check(repeated.value->next_revision.value() == 8U, "repeated preparation remains deterministic");
        check(view.links_.empty(), "repeated preparation never publishes relation state");
    }

    return failures == 0 ? 0 : 1;
}
