#include "st/application/event_projection_commands.hpp"

#include <iostream>
#include <string_view>

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

class CountingValidationView final : public st::core::EventProjectionValidationView {
public:
    explicit CountingValidationView(st::core::ProjectId project_id)
        : project_id_(project_id)
    {
    }

    [[nodiscard]] st::core::ProjectId project_id() const noexcept override
    {
        ++project_id_reads;
        return project_id_;
    }

    [[nodiscard]] bool contains_event(
        const st::core::ScopedMusicalEventId&) const noexcept override
    {
        ++endpoint_reads;
        return true;
    }

    [[nodiscard]] bool contains_score(
        const st::core::ScopedScoreEntityId&) const noexcept override
    {
        ++endpoint_reads;
        return true;
    }

    [[nodiscard]] bool contains_midi(
        const st::core::ScopedMidiEntityId&) const noexcept override
    {
        ++endpoint_reads;
        return true;
    }

    [[nodiscard]] bool contains_tab(
        const st::core::ScopedTabEntityId&) const noexcept override
    {
        ++endpoint_reads;
        return true;
    }

    [[nodiscard]] bool contains_audio(
        const st::core::ScopedAudioEntityId&) const noexcept override
    {
        ++endpoint_reads;
        return true;
    }

    [[nodiscard]] bool contains_link(
        const st::core::EventProjectionLinkCandidate&) const noexcept override
    {
        ++link_reads;
        return false;
    }

    void reset() const noexcept
    {
        project_id_reads = 0;
        endpoint_reads = 0;
        link_reads = 0;
    }

    st::core::ProjectId project_id_;
    mutable int project_id_reads{0};
    mutable int endpoint_reads{0};
    mutable int link_reads{0};
};

} // namespace

int main()
{
    using namespace st::application;
    using namespace st::core;

    const auto project_a = require_id<ProjectId>("00000000000000000000000000000011");
    const auto project_b = require_id<ProjectId>("00000000000000000000000000000012");
    const auto event_local = require_id<MusicalEventId>("00000000000000000000000000000101");
    const auto score_local = require_id<ScoreEntityId>("00000000000000000000000000000201");

    const ScopedMusicalEventId event_a{project_a, event_local};
    const ScopedMusicalEventId event_b{project_b, event_local};
    const ScopedScoreEntityId score_a{project_a, score_local};
    const ScopedScoreEntityId score_b{project_b, score_local};

    auto aggregate = ProjectAggregate::initial(project_a);
    CountingValidationView view{project_a};

    const auto wrong_event = execute_add_event_projection_link_command(
        aggregate,
        AddEventProjectionLinkCommand{
            aggregate.snapshot(),
            EventProjectionLinkCandidate{event_b, score_a},
        },
        view);

    check(!wrong_event, "cross-project event candidate is rejected");
    check(
        wrong_event.error == AddEventProjectionLinkCommandError::preparation_failed &&
            wrong_event.preparation_error == EventProjectionMutationPreparationError::event_wrong_project,
        "cross-project event preserves the exact failure reason");
    check(
        view.project_id_reads == 0 && view.endpoint_reads == 0 && view.link_reads == 0,
        "cross-project event is rejected before every external validation callback");
    check(
        aggregate.snapshot().revision() == ProjectRevision::initial() &&
            aggregate.event_projection_relations().links().empty(),
        "cross-project event rejection leaves authoritative Project state unchanged");

    view.reset();

    const auto wrong_projection = execute_add_event_projection_link_command(
        aggregate,
        AddEventProjectionLinkCommand{
            aggregate.snapshot(),
            EventProjectionLinkCandidate{event_a, score_b},
        },
        view);

    check(!wrong_projection, "cross-project projection candidate is rejected");
    check(
        wrong_projection.error == AddEventProjectionLinkCommandError::preparation_failed &&
            wrong_projection.preparation_error == EventProjectionMutationPreparationError::projection_wrong_project,
        "cross-project projection preserves the exact failure reason");
    check(
        view.project_id_reads == 0 && view.endpoint_reads == 0 && view.link_reads == 0,
        "cross-project projection is rejected before every external validation callback");
    check(
        aggregate.snapshot().revision() == ProjectRevision::initial() &&
            aggregate.event_projection_relations().links().empty(),
        "cross-project projection rejection leaves authoritative Project state unchanged");

    return failures == 0 ? 0 : 1;
}
