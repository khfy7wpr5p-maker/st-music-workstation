#include "st/application/event_projection_commands.hpp"

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

class TestValidationView : public st::core::EventProjectionValidationView {
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
        ++score_reads_;
        if (score_disappears_after_first_read_ && score_reads_ > 1) {
            return false;
        }
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
        score_reads_ = 0;
    }

    st::core::ProjectId project_id_;
    std::vector<st::core::ScopedMusicalEventId> events_;
    std::vector<st::core::ScopedScoreEntityId> scores_;
    std::vector<st::core::ScopedMidiEntityId> midis_;
    std::vector<st::core::ScopedTabEntityId> tabs_;
    std::vector<st::core::ScopedAudioEntityId> audios_;
    std::vector<st::core::EventProjectionLinkCandidate> links_;
    bool force_contains_link_{false};
    bool score_disappears_after_first_read_{false};
    mutable int project_id_reads_{0};
    mutable int endpoint_reads_{0};
    mutable int link_reads_{0};
    mutable int score_reads_{0};

protected:
    template <typename T>
    [[nodiscard]] static bool contains(
        const std::vector<T>& values,
        const T& value) noexcept
    {
        return std::find(values.begin(), values.end(), value) != values.end();
    }
};

class MutatingValidationView final : public TestValidationView {
public:
    MutatingValidationView(
        st::core::ProjectId project_id,
        st::core::ProjectAggregate& aggregate,
        const st::core::PreparedEventProjectionLinkAddition& nested_prepared,
        const TestValidationView& nested_delegate)
        : TestValidationView(std::move(project_id))
        , aggregate_(aggregate)
        , nested_prepared_(nested_prepared)
        , nested_delegate_(nested_delegate)
    {
    }

    [[nodiscard]] bool contains_event(
        const st::core::ScopedMusicalEventId& id) const noexcept override
    {
        if (!mutation_attempted_) {
            mutation_attempted_ = true;
            const auto nested = aggregate_.publish_event_projection_link_addition(
                nested_prepared_,
                nested_delegate_);
            nested_succeeded_ = static_cast<bool>(nested);
        }
        return TestValidationView::contains_event(id);
    }

    mutable bool mutation_attempted_{false};
    mutable bool nested_succeeded_{false};

private:
    st::core::ProjectAggregate& aggregate_;
    const st::core::PreparedEventProjectionLinkAddition& nested_prepared_;
    const TestValidationView& nested_delegate_;
};

std::optional<st::core::PreparedEventProjectionLinkAddition> require_prepared(
    const st::core::EventProjectionLinkCandidate& candidate,
    const st::core::EventProjectionValidationView& view,
    st::core::ProjectRevision revision)
{
    const auto prepared = st::core::prepare_event_projection_link_addition(
        candidate,
        view,
        revision,
        revision);
    check(static_cast<bool>(prepared), "nested fixture preparation succeeds");
    if (!prepared) {
        return std::nullopt;
    }
    return *prepared.value;
}

} // namespace

int main()
{
    using namespace st::core;
    using namespace st::application;

    const auto project_a = require_id<ProjectId>("00000000000000000000000000000011");
    const auto project_b = require_id<ProjectId>("00000000000000000000000000000012");
    const auto event_local_a = require_id<MusicalEventId>("00000000000000000000000000000101");
    const auto event_local_b = require_id<MusicalEventId>("00000000000000000000000000000102");
    const auto score_local_a = require_id<ScoreEntityId>("00000000000000000000000000000201");
    const auto score_local_b = require_id<ScoreEntityId>("00000000000000000000000000000202");

    const ScopedMusicalEventId event_a{project_a, event_local_a};
    const ScopedMusicalEventId event_b{project_a, event_local_b};
    const ScopedScoreEntityId score_a{project_a, score_local_a};
    const ScopedScoreEntityId score_b{project_a, score_local_b};
    const ScopedMusicalEventId event_wrong_project{project_b, event_local_a};

    const EventProjectionLinkCandidate link_a{event_a, score_a};
    const EventProjectionLinkCandidate link_b{event_b, score_b};
    const EventProjectionLinkCandidate wrong_project_link{event_wrong_project, score_a};

    TestValidationView view_a{project_a};
    view_a.events_ = {event_a, event_b};
    view_a.scores_ = {score_a, score_b};

    auto aggregate = ProjectAggregate::initial(project_a);
    const auto snapshot0 = aggregate.snapshot();
    const AddEventProjectionLinkCommand command_a{snapshot0, link_a};

    const auto first = execute_add_event_projection_link_command(
        aggregate,
        command_a,
        view_a);
    check(static_cast<bool>(first), "valid command publishes through the Project command boundary");
    check(
        aggregate.snapshot().revision().value() == 1U,
        "successful command advances global Project revision exactly once");
    check(
        aggregate.event_projection_relations().links().size() == 1U,
        "successful command publishes exactly one authoritative relation");

    view_a.reset_counters();
    const auto stale = execute_add_event_projection_link_command(
        aggregate,
        command_a,
        view_a);
    check(!stale, "stale command fails closed");
    check(
        stale.error == AddEventProjectionLinkCommandError::stale_project_snapshot,
        "stale command has an explicit top-level error");
    check(
        view_a.project_id_reads_ == 0 && view_a.endpoint_reads_ == 0,
        "stale command rejects before consulting external endpoint state");
    check(
        aggregate.snapshot().revision().value() == 1U,
        "stale command consumes no Project revision");

    TestValidationView untouched_view{project_a};
    const AddEventProjectionLinkCommand wrong_project_command{
        ProjectSnapshotToken{project_b, ProjectRevision::initial()},
        link_a,
    };
    const auto wrong_project = execute_add_event_projection_link_command(
        aggregate,
        wrong_project_command,
        untouched_view);
    check(!wrong_project, "command for another Project is rejected");
    check(
        wrong_project.error == AddEventProjectionLinkCommandError::command_project_mismatch,
        "wrong command Project has an explicit error");
    check(
        untouched_view.project_id_reads_ == 0 && untouched_view.endpoint_reads_ == 0,
        "wrong command Project rejects before external endpoint reads");

    auto wrong_view_aggregate = ProjectAggregate::initial(project_a);
    TestValidationView wrong_view{project_b};
    const auto wrong_view_result = execute_add_event_projection_link_command(
        wrong_view_aggregate,
        AddEventProjectionLinkCommand{wrong_view_aggregate.snapshot(), link_a},
        wrong_view);
    check(!wrong_view_result, "endpoint view from another Project is rejected");
    check(
        wrong_view_result.error ==
            AddEventProjectionLinkCommandError::validation_view_project_mismatch,
        "wrong endpoint-view Project has an explicit error");
    check(
        wrong_view.endpoint_reads_ == 0,
        "wrong endpoint-view Project rejects before endpoint existence reads");

    auto invalid_candidate_aggregate = ProjectAggregate::initial(project_a);
    const auto invalid_candidate = execute_add_event_projection_link_command(
        invalid_candidate_aggregate,
        AddEventProjectionLinkCommand{
            invalid_candidate_aggregate.snapshot(),
            wrong_project_link,
        },
        view_a);
    check(!invalid_candidate, "wrong-project candidate fails preparation");
    check(
        invalid_candidate.error == AddEventProjectionLinkCommandError::preparation_failed,
        "candidate validation failure is classified as preparation failure");
    check(
        invalid_candidate.preparation_error ==
            EventProjectionMutationPreparationError::event_wrong_project,
        "candidate validation preserves the exact event Project error");
    check(
        invalid_candidate_aggregate.snapshot().revision() == ProjectRevision::initial(),
        "invalid candidate consumes no revision");

    auto external_claim_aggregate = ProjectAggregate::initial(project_a);
    TestValidationView external_claim_view{project_a};
    external_claim_view.events_.push_back(event_a);
    external_claim_view.scores_.push_back(score_a);
    external_claim_view.force_contains_link_ = true;
    const auto external_claim = execute_add_event_projection_link_command(
        external_claim_aggregate,
        AddEventProjectionLinkCommand{external_claim_aggregate.snapshot(), link_a},
        external_claim_view);
    check(
        static_cast<bool>(external_claim),
        "external duplicate claim cannot override empty Project-owned relation state");
    check(
        external_claim_view.link_reads_ == 0,
        "command preparation never asks external view for authoritative relation ownership");

    auto duplicate_aggregate = ProjectAggregate::initial(project_a);
    const auto duplicate_first = execute_add_event_projection_link_command(
        duplicate_aggregate,
        AddEventProjectionLinkCommand{duplicate_aggregate.snapshot(), link_a},
        view_a);
    check(static_cast<bool>(duplicate_first), "duplicate fixture first publication succeeds");
    const auto duplicate_snapshot = duplicate_aggregate.snapshot();
    const auto duplicate_second = execute_add_event_projection_link_command(
        duplicate_aggregate,
        AddEventProjectionLinkCommand{duplicate_snapshot, link_a},
        view_a);
    check(!duplicate_second, "duplicate command fails closed");
    check(
        duplicate_second.error == AddEventProjectionLinkCommandError::preparation_failed,
        "duplicate is rejected before authoritative publication");
    check(
        duplicate_second.preparation_error ==
            EventProjectionMutationPreparationError::duplicate_link,
        "duplicate command preserves exact preparation reason");
    check(
        duplicate_aggregate.snapshot() == duplicate_snapshot,
        "duplicate command leaves authoritative snapshot unchanged");

    auto disappearing_aggregate = ProjectAggregate::initial(project_a);
    TestValidationView disappearing_view{project_a};
    disappearing_view.events_.push_back(event_a);
    disappearing_view.scores_.push_back(score_a);
    disappearing_view.score_disappears_after_first_read_ = true;
    const auto disappearing = execute_add_event_projection_link_command(
        disappearing_aggregate,
        AddEventProjectionLinkCommand{disappearing_aggregate.snapshot(), link_a},
        disappearing_view);
    check(!disappearing, "endpoint disappearance between preparation and publication fails closed");
    check(
        disappearing.error == AddEventProjectionLinkCommandError::publication_failed,
        "TOCTOU endpoint disappearance is classified at publication");
    check(
        disappearing.revalidation_error ==
            EventProjectionPublicationRevalidationError::projection_missing,
        "TOCTOU rejection preserves exact revalidation reason");
    check(
        disappearing_aggregate.snapshot().revision() == ProjectRevision::initial(),
        "TOCTOU rejection consumes no revision");
    check(
        disappearing_aggregate.event_projection_relations().links().empty(),
        "TOCTOU rejection publishes no relation");

    auto callback_mutation_aggregate = ProjectAggregate::initial(project_a);
    TestValidationView nested_delegate{project_a};
    nested_delegate.events_ = {event_a, event_b};
    nested_delegate.scores_ = {score_a, score_b};
    const auto nested_prepared = require_prepared(
        link_b,
        nested_delegate,
        ProjectRevision::initial());
    if (!nested_prepared) {
        return 1;
    }

    MutatingValidationView mutating_view{
        project_a,
        callback_mutation_aggregate,
        *nested_prepared,
        nested_delegate,
    };
    mutating_view.events_ = {event_a, event_b};
    mutating_view.scores_ = {score_a, score_b};
    const auto outer_snapshot = callback_mutation_aggregate.snapshot();
    const auto callback_mutation = execute_add_event_projection_link_command(
        callback_mutation_aggregate,
        AddEventProjectionLinkCommand{outer_snapshot, link_a},
        mutating_view);
    check(mutating_view.mutation_attempted_, "adversarial endpoint callback attempted Project mutation");
    check(mutating_view.nested_succeeded_, "nested fixture mutation succeeds before outer publication guard begins");
    check(!callback_mutation, "outer command fails closed after callback changes Project state");
    check(
        callback_mutation.error == AddEventProjectionLinkCommandError::stale_project_snapshot,
        "callback mutation is detected as stale command state");
    check(
        callback_mutation_aggregate.snapshot().revision().value() == 1U,
        "outer stale command does not consume a second revision");
    check(
        callback_mutation_aggregate.event_projection_relations().links().size() == 1U &&
            callback_mutation_aggregate.event_projection_relations().links().front().event_id() == event_b,
        "outer stale command publishes no additional relation after callback mutation");

    for (int iteration = 0; iteration < 500; ++iteration) {
        auto repeated = ProjectAggregate::initial(project_a);
        const auto repeated_result = execute_add_event_projection_link_command(
            repeated,
            AddEventProjectionLinkCommand{repeated.snapshot(), link_a},
            view_a);
        check(static_cast<bool>(repeated_result), "command execution remains deterministic across repeated runs");
        check(
            repeated.snapshot().revision().value() == 1U &&
                repeated.event_projection_relations().links().size() == 1U,
            "repeated command produces identical authoritative state shape");
    }

    return failures == 0 ? 0 : 1;
}
