#include "st/core/event_projection_relation_state.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
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
        const st::core::ScopedMidiEntityId&) const noexcept override
    {
        return false;
    }

    [[nodiscard]] bool contains_tab(
        const st::core::ScopedTabEntityId&) const noexcept override
    {
        return false;
    }

    [[nodiscard]] bool contains_audio(
        const st::core::ScopedAudioEntityId&) const noexcept override
    {
        return false;
    }

    [[nodiscard]] bool contains_link(
        const st::core::EventProjectionLinkCandidate& link) const noexcept override
    {
        return contains(links_, link);
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
};

std::optional<st::core::RevalidatedEventProjectionLinkAddition> require_revalidated(
    const st::core::EventProjectionLinkCandidate& link,
    const st::core::EventProjectionValidationView& view,
    st::core::ProjectRevision revision)
{
    const auto prepared = st::core::prepare_event_projection_link_addition(
        link,
        view,
        revision,
        revision);
    check(static_cast<bool>(prepared), "fixture preparation succeeds");
    if (!prepared) {
        return std::nullopt;
    }

    const auto revalidated = st::core::revalidate_prepared_event_projection_link_addition(
        *prepared.value,
        view,
        revision);
    check(static_cast<bool>(revalidated), "fixture publication revalidation succeeds");
    if (!revalidated) {
        return std::nullopt;
    }

    return *revalidated.value;
}

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
        project_a,
        require_id<MusicalEventId>("00000000000000000000000000000012")};
    const ScopedMusicalEventId event_wrong_project{
        project_b,
        require_id<MusicalEventId>("00000000000000000000000000000011")};

    const ScopedScoreEntityId score_a{
        project_a,
        require_id<ScoreEntityId>("00000000000000000000000000000021")};
    const ScopedScoreEntityId score_b{
        project_a,
        require_id<ScoreEntityId>("00000000000000000000000000000022")};
    const ScopedScoreEntityId score_c{
        project_a,
        require_id<ScoreEntityId>("00000000000000000000000000000023")};

    const EventProjectionLinkCandidate link_a{event_a, score_a};
    const EventProjectionLinkCandidate link_b{event_b, score_b};
    const EventProjectionLinkCandidate missing_link{event_a, score_c};
    const EventProjectionLinkCandidate wrong_project_link{event_wrong_project, score_a};

    TestValidationView view{project_a};
    view.events_ = {event_a, event_b};
    view.scores_ = {score_a, score_b, score_c};

    const auto limits = EventProjectionRelationLimits::create(4U);
    check(limits.has_value(), "removal fixture relation limit is valid");
    if (!limits) {
        return 1;
    }

    const auto state0 = EventProjectionRelationStateCandidate::initial(project_a, *limits);
    const ProjectSnapshotToken snapshot0{project_a, ProjectRevision::initial()};

    const auto revalidated_a = require_revalidated(
        link_a,
        view,
        snapshot0.revision());
    if (!revalidated_a) {
        return 1;
    }
    const auto add_a = build_event_projection_relation_state_candidate(
        state0,
        snapshot0,
        *revalidated_a);
    check(static_cast<bool>(add_a), "first relation fixture transition succeeds");
    if (!add_a) {
        return 1;
    }

    const auto state1 = *add_a.value;
    const auto snapshot1 = *add_a.next_snapshot;
    const auto revalidated_b = require_revalidated(
        link_b,
        view,
        snapshot1.revision());
    if (!revalidated_b) {
        return 1;
    }
    const auto add_b = build_event_projection_relation_state_candidate(
        state1,
        snapshot1,
        *revalidated_b);
    check(static_cast<bool>(add_b), "second relation fixture transition succeeds");
    if (!add_b) {
        return 1;
    }

    const auto state2 = *add_b.value;
    const auto snapshot2 = *add_b.next_snapshot;
    check(state2.links().size() == 2U, "fixture starts with two accepted relations");
    check(snapshot2.revision().value() == 2U, "fixture Project revision is two");

    const auto removed = build_event_projection_relation_state_removal_candidate(
        state2,
        snapshot2,
        link_a);
    check(static_cast<bool>(removed), "existing exact relation can be removed immutably");
    check(
        removed.error == EventProjectionRelationStateRemovalError::none,
        "successful removal has no error");
    check(
        removed.next_snapshot->matches(project_a, ProjectRevision::from_persisted(3U)),
        "successful removal advances global Project revision exactly once");
    check(
        state2.links().size() == 2U,
        "successful removal leaves prior relation state unchanged");
    check(
        removed.value->links().size() == 1U &&
            removed.value->links().front().event_id() == event_b &&
            removed.value->links().front().projection_id() == link_b.projection_id(),
        "successful removal removes exactly the requested relation and preserves the other relation");
    check(
        removed.value->limits() == state2.limits(),
        "removal preserves immutable relation limits");

    {
        const auto missing = build_event_projection_relation_state_removal_candidate(
            state2,
            snapshot2,
            missing_link);
        check(!missing, "missing relation fails closed");
        check(
            missing.error == EventProjectionRelationStateRemovalError::link_not_found,
            "missing relation has an explicit not-found error");
        check(!missing.next_snapshot.has_value(), "missing relation consumes no Project revision");
        check(state2.links().size() == 2U, "missing relation leaves current state unchanged");
    }

    {
        const auto wrong_scope = build_event_projection_relation_state_removal_candidate(
            state2,
            snapshot2,
            wrong_project_link);
        check(!wrong_scope, "cross-Project removal key fails closed");
        check(
            wrong_scope.error == EventProjectionRelationStateRemovalError::link_wrong_project,
            "cross-Project removal key has an explicit scope error");
        check(!wrong_scope.next_snapshot.has_value(), "scope failure consumes no revision");
    }

    {
        const auto wrong_state = EventProjectionRelationStateCandidate::initial(
            project_b,
            *limits);
        const auto mismatch = build_event_projection_relation_state_removal_candidate(
            wrong_state,
            snapshot2,
            wrong_project_link);
        check(!mismatch, "relation state from another Project is rejected first");
        check(
            mismatch.error ==
                EventProjectionRelationStateRemovalError::current_state_project_mismatch,
            "state/snapshot Project mismatch has deterministic error precedence");
    }

    {
        const ProjectSnapshotToken overflow_snapshot{
            project_a,
            ProjectRevision::from_persisted(
                std::numeric_limits<ProjectRevision::Value>::max())};
        const auto overflow = build_event_projection_relation_state_removal_candidate(
            state2,
            overflow_snapshot,
            link_a);
        check(!overflow, "revision overflow fails closed");
        check(
            overflow.error == EventProjectionRelationStateRemovalError::revision_overflow,
            "revision overflow is explicit after exact relation existence is established");
        check(!overflow.next_snapshot.has_value(), "overflow returns no next Project snapshot");
        check(state2.links().size() == 2U, "overflow leaves current relation state unchanged");
    }

    {
        const ProjectSnapshotToken overflow_snapshot{
            project_a,
            ProjectRevision::from_persisted(
                std::numeric_limits<ProjectRevision::Value>::max())};
        const auto missing_at_overflow = build_event_projection_relation_state_removal_candidate(
            state2,
            overflow_snapshot,
            missing_link);
        check(
            missing_at_overflow.error == EventProjectionRelationStateRemovalError::link_not_found,
            "link-not-found deterministically precedes revision overflow");
    }

    for (int iteration = 0; iteration < 10000; ++iteration) {
        const auto repeated = build_event_projection_relation_state_removal_candidate(
            state2,
            snapshot2,
            link_a);
        check(static_cast<bool>(repeated), "repeated removal transition remains successful");
        check(
            repeated.next_snapshot->revision().value() == 3U,
            "repeated removal returns the same next Project revision");
        check(
            repeated.value->links().size() == 1U &&
                repeated.value->links().front().event_id() == event_b,
            "repeated removal produces the same remaining relation state");
        check(state2.links().size() == 2U, "repeated removal never mutates the base relation state");
    }

    return failures == 0 ? 0 : 1;
}
