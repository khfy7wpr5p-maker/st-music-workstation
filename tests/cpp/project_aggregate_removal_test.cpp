#include "st/core/project_aggregate.hpp"

#include <algorithm>
#include <iostream>
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
        const st::core::EventProjectionLinkCandidate&) const noexcept override
    {
        return false;
    }

    st::core::ProjectId project_id_;
    std::vector<st::core::ScopedMusicalEventId> events_;
    std::vector<st::core::ScopedScoreEntityId> scores_;
    std::vector<st::core::ScopedMidiEntityId> midis_;
    std::vector<st::core::ScopedTabEntityId> tabs_;
    std::vector<st::core::ScopedAudioEntityId> audios_;

private:
    template <typename T>
    [[nodiscard]] static bool contains(
        const std::vector<T>& values,
        const T& value) noexcept
    {
        return std::find(values.begin(), values.end(), value) != values.end();
    }
};

std::optional<st::core::PreparedEventProjectionLinkAddition> require_prepared(
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
    return *prepared.value;
}

bool seed_link(
    st::core::ProjectAggregate& aggregate,
    const st::core::EventProjectionLinkCandidate& link,
    const TestValidationView& view)
{
    const auto prepared = require_prepared(
        link,
        view,
        aggregate.snapshot().revision());
    if (!prepared) {
        return false;
    }

    const auto published = aggregate.publish_event_projection_link_addition(
        *prepared,
        view);
    check(static_cast<bool>(published), "fixture relation publishes");
    return static_cast<bool>(published);
}

class ReentrantRemovalValidationView final
    : public st::core::EventProjectionValidationView {
public:
    ReentrantRemovalValidationView(
        st::core::ProjectAggregate& aggregate,
        st::core::ProjectSnapshotToken expected_snapshot,
        st::core::EventProjectionLinkCandidate removal_key,
        const TestValidationView& delegate) noexcept
        : aggregate_(aggregate)
        , expected_snapshot_(expected_snapshot)
        , removal_key_(std::move(removal_key))
        , delegate_(delegate)
    {
    }

    [[nodiscard]] st::core::ProjectId project_id() const noexcept override
    {
        if (!attempted_) {
            attempted_ = true;
            const auto nested = aggregate_.publish_event_projection_link_removal(
                expected_snapshot_,
                removal_key_);
            nested_succeeded_ = static_cast<bool>(nested);
            nested_error_ = nested.error;
        }
        return delegate_.project_id_;
    }

    [[nodiscard]] bool contains_event(
        const st::core::ScopedMusicalEventId& id) const noexcept override
    {
        return delegate_.contains_event(id);
    }

    [[nodiscard]] bool contains_score(
        const st::core::ScopedScoreEntityId& id) const noexcept override
    {
        return delegate_.contains_score(id);
    }

    [[nodiscard]] bool contains_midi(
        const st::core::ScopedMidiEntityId& id) const noexcept override
    {
        return delegate_.contains_midi(id);
    }

    [[nodiscard]] bool contains_tab(
        const st::core::ScopedTabEntityId& id) const noexcept override
    {
        return delegate_.contains_tab(id);
    }

    [[nodiscard]] bool contains_audio(
        const st::core::ScopedAudioEntityId& id) const noexcept override
    {
        return delegate_.contains_audio(id);
    }

    [[nodiscard]] bool contains_link(
        const st::core::EventProjectionLinkCandidate& link) const noexcept override
    {
        return delegate_.contains_link(link);
    }

    mutable bool attempted_{false};
    mutable bool nested_succeeded_{false};
    mutable st::core::ProjectEventProjectionRemovalPublicationError nested_error_{
        st::core::ProjectEventProjectionRemovalPublicationError::none};

private:
    st::core::ProjectAggregate& aggregate_;
    st::core::ProjectSnapshotToken expected_snapshot_;
    st::core::EventProjectionLinkCandidate removal_key_;
    const TestValidationView& delegate_;
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
        require_id<MusicalEventId>("00000000000000000000000000000012")};
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
    const EventProjectionLinkCandidate link_b{event_a, score_b};
    const EventProjectionLinkCandidate missing_link{event_a, score_c};
    const EventProjectionLinkCandidate cross_project_link{event_b, score_a};

    TestValidationView view_a{project_a};
    view_a.events_.push_back(event_a);
    view_a.scores_.push_back(score_a);
    view_a.scores_.push_back(score_b);
    view_a.scores_.push_back(score_c);

    const auto three_link_limit = EventProjectionRelationLimits::create(3U);
    check(three_link_limit.has_value(), "three-link fixture limit is valid");
    if (!three_link_limit) {
        return 1;
    }

    auto aggregate = ProjectAggregate::initial(project_a, *three_link_limit);
    if (!seed_link(aggregate, link_a, view_a) ||
        !seed_link(aggregate, link_b, view_a)) {
        return 1;
    }

    check(
        aggregate.snapshot().revision().value() == 2U,
        "fixture reaches global Project revision two");
    check(
        aggregate.event_projection_relations().links().size() == 2U,
        "fixture has two authoritative relations");

    const auto removal_base = aggregate.snapshot();
    const auto removed = aggregate.publish_event_projection_link_removal(
        removal_base,
        link_a);
    check(static_cast<bool>(removed), "exact authoritative relation removal succeeds");
    check(
        removed.error == ProjectEventProjectionRemovalPublicationError::none,
        "successful removal has no top-level error");
    check(
        removed.removal_error == EventProjectionRelationStateRemovalError::none,
        "successful removal has no transition error");
    check(
        removed.published_snapshot->matches(
            project_a,
            ProjectRevision::from_persisted(3U)),
        "successful removal advances the global Project revision exactly once");
    check(
        aggregate.snapshot() == *removed.published_snapshot,
        "successful removal publishes the returned Project snapshot");
    check(
        aggregate.event_projection_relations().links().size() == 1U,
        "successful removal commits exactly one fewer relation");
    check(
        aggregate.event_projection_relations().links().front().event_id() == event_a &&
            aggregate.event_projection_relations().links().front().projection_id() ==
                link_b.projection_id(),
        "successful removal preserves the distinct surviving relation");
    check(
        aggregate.event_projection_relations().limits() == *three_link_limit,
        "successful removal preserves the relation safety limit");

    const auto snapshot_after_removal = aggregate.snapshot();
    const auto stale = aggregate.publish_event_projection_link_removal(
        removal_base,
        link_b);
    check(!stale, "stale expected snapshot fails closed");
    check(
        stale.error ==
            ProjectEventProjectionRemovalPublicationError::stale_expected_snapshot,
        "stale removal has an explicit top-level reason");
    check(
        aggregate.snapshot() == snapshot_after_removal,
        "stale removal consumes no Project revision");
    check(
        aggregate.event_projection_relations().links().size() == 1U,
        "stale removal leaves authoritative relation state unchanged");

    const ProjectSnapshotToken wrong_project_snapshot{
        project_b,
        snapshot_after_removal.revision(),
    };
    const auto wrong_snapshot = aggregate.publish_event_projection_link_removal(
        wrong_project_snapshot,
        link_b);
    check(!wrong_snapshot, "expected snapshot from another Project fails closed");
    check(
        wrong_snapshot.error ==
            ProjectEventProjectionRemovalPublicationError::expected_snapshot_project_mismatch,
        "wrong-Project expected snapshot has an explicit top-level reason");
    check(
        aggregate.snapshot() == snapshot_after_removal,
        "wrong-Project expected snapshot consumes no Project revision");

    const auto missing = aggregate.publish_event_projection_link_removal(
        aggregate.snapshot(),
        missing_link);
    check(!missing, "missing authoritative relation fails closed");
    check(
        missing.error ==
            ProjectEventProjectionRemovalPublicationError::relation_state_transition_failed,
        "missing relation is reported as a transition failure");
    check(
        missing.removal_error == EventProjectionRelationStateRemovalError::link_not_found,
        "missing relation preserves the exact transition reason");
    check(
        aggregate.snapshot() == snapshot_after_removal,
        "missing relation consumes no Project revision");

    const auto cross_project = aggregate.publish_event_projection_link_removal(
        aggregate.snapshot(),
        cross_project_link);
    check(!cross_project, "cross-Project removal key fails closed");
    check(
        cross_project.error ==
            ProjectEventProjectionRemovalPublicationError::relation_state_transition_failed,
        "cross-Project removal key is reported as a transition failure");
    check(
        cross_project.removal_error ==
            EventProjectionRelationStateRemovalError::link_wrong_project,
        "cross-Project removal preserves the exact transition reason");
    check(
        aggregate.snapshot() == snapshot_after_removal,
        "cross-Project removal consumes no Project revision");

    auto reentrant_aggregate = ProjectAggregate::initial(project_a, *three_link_limit);
    if (!seed_link(reentrant_aggregate, link_a, view_a)) {
        return 1;
    }
    const auto prepared_b = require_prepared(
        link_b,
        view_a,
        reentrant_aggregate.snapshot().revision());
    if (!prepared_b) {
        return 1;
    }

    ReentrantRemovalValidationView reentrant_view{
        reentrant_aggregate,
        reentrant_aggregate.snapshot(),
        link_a,
        view_a,
    };
    const auto outer_add = reentrant_aggregate.publish_event_projection_link_addition(
        *prepared_b,
        reentrant_view);
    check(reentrant_view.attempted_, "addition callback attempted nested authoritative removal");
    check(!reentrant_view.nested_succeeded_, "nested authoritative removal is rejected");
    check(
        reentrant_view.nested_error_ ==
            ProjectEventProjectionRemovalPublicationError::reentrant_publication,
        "nested removal reports the shared publication reentrancy guard");
    check(static_cast<bool>(outer_add), "outer addition can finish after nested removal is rejected");
    check(
        reentrant_aggregate.snapshot().revision().value() == 2U,
        "reentrant removal rejection prevents double revision consumption");
    check(
        reentrant_aggregate.event_projection_relations().links().size() == 2U,
        "reentrant removal rejection prevents authoritative state loss");

    for (int iteration = 0; iteration < 200; ++iteration) {
        auto repeated = ProjectAggregate::initial(project_a, *three_link_limit);
        if (!seed_link(repeated, link_a, view_a)) {
            return 1;
        }
        const auto repeated_removal = repeated.publish_event_projection_link_removal(
            repeated.snapshot(),
            link_a);
        check(static_cast<bool>(repeated_removal), "repeated authoritative removal remains deterministic");
        check(
            repeated.snapshot().revision().value() == 2U,
            "repeated removal advances the global revision exactly once");
        check(
            repeated.event_projection_relations().links().empty(),
            "repeated removal produces the same empty relation state");
    }

    return failures == 0 ? 0 : 1;
}
