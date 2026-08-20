#include "st/core/event_projection_relation_state.hpp"

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

    static_assert(!std::is_constructible_v<EventProjectionLink, EventProjectionLinkCandidate>);
    static_assert(std::is_copy_constructible_v<EventProjectionLink>);

    check(!EventProjectionRelationLimits::create(0U).has_value(), "zero relation limit is rejected");
    check(
        !EventProjectionRelationLimits::create(kAbsoluteMaxEventProjectionLinks + 1U).has_value(),
        "relation limit above absolute cap is rejected");
    check(
        EventProjectionRelationLimits::production_default().max_links() ==
            kAbsoluteMaxEventProjectionLinks,
        "production default uses the reviewed absolute cap");

    const auto one_link_limit = EventProjectionRelationLimits::create(1U);
    check(one_link_limit.has_value(), "bounded one-link fixture limit is accepted");
    if (!one_link_limit) {
        return 1;
    }

    const auto project_a = require_id<ProjectId>("00000000000000000000000000000001");
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

    TestValidationView view{project_a};
    view.events_.push_back(event_a);
    view.scores_.push_back(score_a);
    view.scores_.push_back(score_b);

    const auto state0 = EventProjectionRelationStateCandidate::initial(
        project_a,
        *one_link_limit);
    check(state0.snapshot().matches(project_a, ProjectRevision::initial()), "initial state candidate starts at Project revision zero");
    check(state0.links().empty(), "initial state candidate has no accepted links");
    check(state0.limits() == *one_link_limit, "initial state candidate retains its bounded resource policy");

    const auto revalidated_a = require_revalidated(
        link_a,
        view,
        ProjectRevision::initial());
    if (!revalidated_a) {
        return 1;
    }

    const auto transition1 = build_event_projection_relation_state_candidate(
        state0,
        *revalidated_a);
    check(static_cast<bool>(transition1), "revalidated first link builds a next state candidate");
    check(transition1.error == EventProjectionRelationStateTransitionError::none, "successful transition has no error");
    check(state0.links().empty(), "successful transition leaves prior state candidate unchanged");
    check(state0.snapshot().revision() == ProjectRevision::initial(), "successful transition does not revise prior snapshot");
    if (!transition1) {
        return 1;
    }

    const auto state1 = *transition1.value;
    check(state1.snapshot().matches(project_a, ProjectRevision::from_persisted(1U)), "next state candidate advances snapshot exactly once");
    check(state1.links().size() == 1U, "next state candidate contains exactly one accepted link");
    check(state1.links().front().event_id() == event_a, "accepted link preserves MusicalEventId");
    check(state1.links().front().projection_id() == link_a.projection_id(), "accepted link preserves typed projection identity");
    check(state1.limits() == *one_link_limit, "resource limit remains immutable across transition");

    {
        const auto stale = build_event_projection_relation_state_candidate(
            state1,
            *revalidated_a);
        check(!stale, "revalidated value from revision zero cannot apply to revision one candidate");
        check(stale.error == EventProjectionRelationStateTransitionError::base_snapshot_mismatch, "stale transition fails with base snapshot mismatch");
        check(state1.links().size() == 1U, "stale transition leaves current candidate unchanged");
    }

    const auto revision1 = ProjectRevision::from_persisted(1U);
    const auto duplicate_revalidated = require_revalidated(link_a, view, revision1);
    if (!duplicate_revalidated) {
        return 1;
    }
    {
        const auto duplicate = build_event_projection_relation_state_candidate(
            state1,
            *duplicate_revalidated);
        check(!duplicate, "transition rejects duplicate even if an inconsistent validation view missed it");
        check(duplicate.error == EventProjectionRelationStateTransitionError::duplicate_link, "duplicate transition has explicit duplicate error");
        check(state1.links().size() == 1U, "duplicate failure leaves current candidate unchanged");
    }

    const auto second_revalidated = require_revalidated(link_b, view, revision1);
    if (!second_revalidated) {
        return 1;
    }
    {
        const auto limited = build_event_projection_relation_state_candidate(
            state1,
            *second_revalidated);
        check(!limited, "transition rejects growth beyond immutable relation limit");
        check(limited.error == EventProjectionRelationStateTransitionError::relation_limit_exceeded, "resource-limit failure is explicit");
        check(state1.links().size() == 1U, "resource-limit failure leaves current candidate unchanged");
    }

    for (int iteration = 0; iteration < 10000; ++iteration) {
        const auto repeated = build_event_projection_relation_state_candidate(
            state0,
            *revalidated_a);
        check(static_cast<bool>(repeated), "repeated immutable transition remains successful");
        check(repeated.value->snapshot().revision().value() == 1U, "repeated immutable transition remains deterministic");
        check(repeated.value->links().size() == 1U, "repeated transition produces the same relation count");
        check(state0.links().empty(), "repeated transition never mutates the base candidate");
    }

    return failures == 0 ? 0 : 1;
}
