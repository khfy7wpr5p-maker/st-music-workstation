#include "st/core/project_aggregate.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <optional>
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
        ++project_id_reads_;
        return project_id_;
    }

    [[nodiscard]] bool contains_event(
        const st::core::ScopedMusicalEventId& id) const noexcept override
    {
        ++endpoint_reads_;
        return contains(events_, id);
    }

    [[nodiscard]] bool contains_score(
        const st::core::ScopedScoreEntityId& id) const noexcept override
    {
        ++endpoint_reads_;
        return contains(scores_, id);
    }

    [[nodiscard]] bool contains_midi(
        const st::core::ScopedMidiEntityId& id) const noexcept override
    {
        ++endpoint_reads_;
        return contains(midis_, id);
    }

    [[nodiscard]] bool contains_tab(
        const st::core::ScopedTabEntityId& id) const noexcept override
    {
        ++endpoint_reads_;
        return contains(tabs_, id);
    }

    [[nodiscard]] bool contains_audio(
        const st::core::ScopedAudioEntityId& id) const noexcept override
    {
        ++endpoint_reads_;
        return contains(audios_, id);
    }

    [[nodiscard]] bool contains_link(
        const st::core::EventProjectionLinkCandidate& link) const noexcept override
    {
        ++link_reads_;
        if (force_contains_link_) {
            return true;
        }
        return contains(links_, link);
    }

    void reset_counters() const noexcept
    {
        project_id_reads_ = 0;
        endpoint_reads_ = 0;
        link_reads_ = 0;
    }

    st::core::ProjectId project_id_;
    std::vector<st::core::ScopedMusicalEventId> events_;
    std::vector<st::core::ScopedScoreEntityId> scores_;
    std::vector<st::core::ScopedMidiEntityId> midis_;
    std::vector<st::core::ScopedTabEntityId> tabs_;
    std::vector<st::core::ScopedAudioEntityId> audios_;
    std::vector<st::core::EventProjectionLinkCandidate> links_;
    bool force_contains_link_{false};
    mutable int project_id_reads_{0};
    mutable int endpoint_reads_{0};
    mutable int link_reads_{0};

private:
    template <typename T>
    [[nodiscard]] static bool contains(
        const std::vector<T>& values,
        const T& value) noexcept
    {
        return std::find(values.begin(), values.end(), value) != values.end();
    }
};

class ReentrantValidationView final : public st::core::EventProjectionValidationView {
public:
    ReentrantValidationView(
        st::core::ProjectAggregate& aggregate,
        const st::core::PreparedEventProjectionLinkAddition& prepared,
        const TestValidationView& delegate) noexcept
        : aggregate_(aggregate)
        , prepared_(prepared)
        , delegate_(delegate)
    {
    }

    [[nodiscard]] st::core::ProjectId project_id() const noexcept override
    {
        if (!attempted_) {
            attempted_ = true;
            const auto nested = aggregate_.publish_event_projection_link_addition(
                prepared_,
                *this);
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
    mutable st::core::ProjectEventProjectionPublicationError nested_error_{
        st::core::ProjectEventProjectionPublicationError::none};

private:
    st::core::ProjectAggregate& aggregate_;
    const st::core::PreparedEventProjectionLinkAddition& prepared_;
    const TestValidationView& delegate_;
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

} // namespace

int main()
{
    using namespace st::core;

    static_assert(!std::is_copy_constructible_v<ProjectAggregate>);
    static_assert(!std::is_copy_assignable_v<ProjectAggregate>);
    static_assert(!std::is_move_constructible_v<ProjectAggregate>);
    static_assert(!std::is_move_assignable_v<ProjectAggregate>);

    const auto project_a = require_id<ProjectId>("00000000000000000000000000000001");
    const auto project_b = require_id<ProjectId>("00000000000000000000000000000002");
    const ScopedMusicalEventId event_a{
        project_a,
        require_id<MusicalEventId>("00000000000000000000000000000011")};
    const ScopedScoreEntityId score_a{
        project_a,
        require_id<ScoreEntityId>("00000000000000000000000000000021")};
    const ScopedScoreEntityId score_b{
        project_a,
        require_id<ScoreEntityId>("00000000000000000000000000000022")};
    const EventProjectionLinkCandidate link_a{event_a, score_a};
    const EventProjectionLinkCandidate link_b{event_a, score_b};

    TestValidationView view_a{project_a};
    view_a.events_.push_back(event_a);
    view_a.scores_.push_back(score_a);
    view_a.scores_.push_back(score_b);

    const auto two_link_limit = EventProjectionRelationLimits::create(2U);
    check(two_link_limit.has_value(), "two-link fixture limit is valid");
    if (!two_link_limit) {
        return 1;
    }

    auto aggregate = ProjectAggregate::initial(project_a, *two_link_limit);
    check(
        aggregate.snapshot().matches(project_a, ProjectRevision::initial()),
        "Project aggregate starts at exact revision zero");
    check(
        aggregate.event_projection_relations().project_id() == project_a,
        "Project aggregate relation state uses the same Project scope");
    check(
        aggregate.event_projection_relations().links().empty(),
        "Project aggregate starts with no authoritative relation links");

    const auto prepared_a = require_prepared(
        link_a,
        view_a,
        ProjectRevision::initial());
    if (!prepared_a) {
        return 1;
    }

    view_a.reset_counters();
    const auto published_a = aggregate.publish_event_projection_link_addition(
        *prepared_a,
        view_a);
    check(static_cast<bool>(published_a), "valid prepared relation publishes authoritatively");
    check(
        published_a.error == ProjectEventProjectionPublicationError::none,
        "successful authoritative publication has no top-level error");
    check(
        published_a.published_snapshot->matches(
            project_a,
            ProjectRevision::from_persisted(1U)),
        "successful publication advances the global Project revision exactly once");
    check(
        aggregate.snapshot() == *published_a.published_snapshot,
        "published snapshot is the aggregate's new authoritative snapshot");
    check(
        aggregate.event_projection_relations().links().size() == 1U,
        "successful publication commits exactly one authoritative relation");
    check(
        aggregate.event_projection_relations().links().front().event_id() == event_a,
        "authoritative relation preserves the event identity");
    check(
        aggregate.event_projection_relations().links().front().projection_id() ==
            link_a.projection_id(),
        "authoritative relation preserves the projection identity");
    check(
        view_a.link_reads_ == 0,
        "publication never delegates authoritative duplicate lookup to the endpoint view");

    const auto snapshot_after_first_publish = aggregate.snapshot();
    const auto relation_count_after_first_publish =
        aggregate.event_projection_relations().links().size();

    view_a.reset_counters();
    const auto stale_reuse = aggregate.publish_event_projection_link_addition(
        *prepared_a,
        view_a);
    check(!stale_reuse, "reusing a prepared revision-zero command after publication fails closed");
    check(
        stale_reuse.error ==
            ProjectEventProjectionPublicationError::publication_revalidation_failed,
        "stale publication reports revalidation failure");
    check(
        stale_reuse.revalidation_error ==
            EventProjectionPublicationRevalidationError::stale_prepared_revision,
        "stale publication preserves the exact stale-revision reason");
    check(
        aggregate.snapshot() == snapshot_after_first_publish,
        "stale publication does not consume another Project revision");
    check(
        aggregate.event_projection_relations().links().size() ==
            relation_count_after_first_publish,
        "stale publication does not mutate authoritative relation state");

    const auto revision1 = ProjectRevision::from_persisted(1U);
    const auto prepared_duplicate = require_prepared(link_a, view_a, revision1);
    if (!prepared_duplicate) {
        return 1;
    }

    view_a.reset_counters();
    const auto duplicate_publish = aggregate.publish_event_projection_link_addition(
        *prepared_duplicate,
        view_a);
    check(!duplicate_publish, "authoritative relation state rejects a duplicate link");
    check(
        duplicate_publish.error ==
            ProjectEventProjectionPublicationError::publication_revalidation_failed,
        "duplicate is rejected during current-state revalidation");
    check(
        duplicate_publish.revalidation_error ==
            EventProjectionPublicationRevalidationError::duplicate_link,
        "duplicate publication preserves the exact duplicate reason");
    check(
        view_a.link_reads_ == 0,
        "duplicate decision comes from Project-owned relation state, not external relation lookup");
    check(
        aggregate.snapshot() == snapshot_after_first_publish,
        "duplicate publication leaves authoritative revision unchanged");

    const auto prepared_b = require_prepared(link_b, view_a, revision1);
    if (!prepared_b) {
        return 1;
    }
    const auto published_b = aggregate.publish_event_projection_link_addition(
        *prepared_b,
        view_a);
    check(static_cast<bool>(published_b), "second distinct relation publishes successfully");
    check(
        aggregate.snapshot().matches(
            project_a,
            ProjectRevision::from_persisted(2U)),
        "second successful publication advances global Project revision to two");
    check(
        aggregate.event_projection_relations().links().size() == 2U,
        "second successful publication commits the second relation");

    const auto one_link_limit = EventProjectionRelationLimits::create(1U);
    check(one_link_limit.has_value(), "one-link fixture limit is valid");
    if (!one_link_limit) {
        return 1;
    }

    auto bounded_aggregate = ProjectAggregate::initial(project_a, *one_link_limit);
    const auto bounded_prepared_a = require_prepared(
        link_a,
        view_a,
        ProjectRevision::initial());
    if (!bounded_prepared_a) {
        return 1;
    }
    const auto bounded_first = bounded_aggregate.publish_event_projection_link_addition(
        *bounded_prepared_a,
        view_a);
    check(static_cast<bool>(bounded_first), "bounded aggregate accepts its first relation");

    const auto bounded_prepared_b = require_prepared(link_b, view_a, revision1);
    if (!bounded_prepared_b) {
        return 1;
    }
    const auto bounded_snapshot_before_failure = bounded_aggregate.snapshot();
    const auto bounded_limit_failure =
        bounded_aggregate.publish_event_projection_link_addition(
            *bounded_prepared_b,
            view_a);
    check(!bounded_limit_failure, "relation resource limit fails closed during authoritative publication");
    check(
        bounded_limit_failure.error ==
            ProjectEventProjectionPublicationError::relation_state_transition_failed,
        "relation resource limit is reported as transition failure");
    check(
        bounded_limit_failure.transition_error ==
            EventProjectionRelationStateTransitionError::relation_limit_exceeded,
        "relation resource limit preserves the exact transition reason");
    check(
        bounded_aggregate.snapshot() == bounded_snapshot_before_failure,
        "relation resource-limit failure consumes no Project revision");
    check(
        bounded_aggregate.event_projection_relations().links().size() == 1U,
        "relation resource-limit failure leaves authoritative state unchanged");

    auto endpoint_failure_aggregate = ProjectAggregate::initial(project_a, *two_link_limit);
    TestValidationView endpoint_view{project_a};
    endpoint_view.events_.push_back(event_a);
    endpoint_view.scores_.push_back(score_a);
    const auto prepared_before_endpoint_removal = require_prepared(
        link_a,
        endpoint_view,
        ProjectRevision::initial());
    if (!prepared_before_endpoint_removal) {
        return 1;
    }
    endpoint_view.scores_.clear();
    const auto endpoint_failure =
        endpoint_failure_aggregate.publish_event_projection_link_addition(
            *prepared_before_endpoint_removal,
            endpoint_view);
    check(!endpoint_failure, "endpoint disappearance before publication fails closed");
    check(
        endpoint_failure.revalidation_error ==
            EventProjectionPublicationRevalidationError::projection_missing,
        "endpoint disappearance preserves exact revalidation reason");
    check(
        endpoint_failure_aggregate.snapshot().revision() == ProjectRevision::initial(),
        "endpoint revalidation failure consumes no Project revision");
    check(
        endpoint_failure_aggregate.event_projection_relations().links().empty(),
        "endpoint revalidation failure publishes no relation");

    TestValidationView wrong_project_view{project_b};
    wrong_project_view.reset_counters();
    auto wrong_view_aggregate = ProjectAggregate::initial(project_a, *two_link_limit);
    const auto wrong_view = wrong_view_aggregate.publish_event_projection_link_addition(
        *prepared_a,
        wrong_project_view);
    check(!wrong_view, "validation view from another Project is rejected");
    check(
        wrong_view.error ==
            ProjectEventProjectionPublicationError::validation_view_project_mismatch,
        "validation-view Project mismatch has an explicit top-level error");
    check(
        wrong_project_view.endpoint_reads_ == 0 && wrong_project_view.link_reads_ == 0,
        "Project mismatch rejects before endpoint or relation lookup");
    check(
        wrong_view_aggregate.snapshot().revision() == ProjectRevision::initial(),
        "wrong validation view consumes no Project revision");

    TestValidationView external_relation_claim_view{project_a};
    external_relation_claim_view.events_.push_back(event_a);
    external_relation_claim_view.scores_.push_back(score_a);
    const auto prepared_external_claim = require_prepared(
        link_a,
        external_relation_claim_view,
        ProjectRevision::initial());
    if (!prepared_external_claim) {
        return 1;
    }
    external_relation_claim_view.force_contains_link_ = true;
    external_relation_claim_view.reset_counters();
    auto external_relation_claim_aggregate =
        ProjectAggregate::initial(project_a, *two_link_limit);
    const auto external_relation_claim_publish =
        external_relation_claim_aggregate.publish_event_projection_link_addition(
            *prepared_external_claim,
            external_relation_claim_view);
    check(
        static_cast<bool>(external_relation_claim_publish),
        "external relation claims cannot override empty Project-owned authoritative relation state");
    check(
        external_relation_claim_view.link_reads_ == 0,
        "authoritative publication never asks the endpoint view for relation ownership");

    auto reentrant_aggregate = ProjectAggregate::initial(project_a, *two_link_limit);
    ReentrantValidationView reentrant_view{
        reentrant_aggregate,
        *prepared_a,
        view_a,
    };
    const auto reentrant_outer = reentrant_aggregate.publish_event_projection_link_addition(
        *prepared_a,
        reentrant_view);
    check(reentrant_view.attempted_, "validation callback attempted a nested publication");
    check(!reentrant_view.nested_succeeded_, "nested publication is rejected fail closed");
    check(
        reentrant_view.nested_error_ ==
            ProjectEventProjectionPublicationError::reentrant_publication,
        "nested publication reports explicit reentrancy error");
    check(static_cast<bool>(reentrant_outer), "outer publication may complete after nested attempt is rejected");
    check(
        reentrant_aggregate.snapshot().revision().value() == 1U,
        "reentrancy rejection prevents double revision consumption");
    check(
        reentrant_aggregate.event_projection_relations().links().size() == 1U,
        "reentrancy rejection prevents duplicate authoritative state publication");

    for (int iteration = 0; iteration < 1000; ++iteration) {
        auto repeated = ProjectAggregate::initial(project_a, *two_link_limit);
        const auto result = repeated.publish_event_projection_link_addition(
            *prepared_a,
            view_a);
        check(static_cast<bool>(result), "repeated authoritative publication remains deterministic");
        check(
            repeated.snapshot().revision().value() == 1U,
            "repeated authoritative publication advances exactly once");
        check(
            repeated.event_projection_relations().links().size() == 1U,
            "repeated authoritative publication produces the same relation state");
    }

    return failures == 0 ? 0 : 1;
}
