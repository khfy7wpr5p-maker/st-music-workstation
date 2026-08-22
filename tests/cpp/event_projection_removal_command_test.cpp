#include "st/application/event_projection_commands.hpp"

#include <algorithm>
#include <iostream>
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

bool add_link(
    st::core::ProjectAggregate& aggregate,
    const st::core::EventProjectionLinkCandidate& link,
    const TestValidationView& view)
{
    const st::application::AddEventProjectionLinkCommand command{
        aggregate.snapshot(),
        link,
    };
    const auto result = st::application::execute_add_event_projection_link_command(
        aggregate,
        command,
        view);
    check(static_cast<bool>(result), "fixture addition command succeeds");
    return static_cast<bool>(result);
}

} // namespace

int main()
{
    using namespace st::application;
    using namespace st::core;

    const auto project_a = require_id<ProjectId>("00000000000000000000000000000011");
    const auto project_b = require_id<ProjectId>("00000000000000000000000000000012");
    const auto event_local_a = require_id<MusicalEventId>("00000000000000000000000000000101");
    const auto score_local_a = require_id<ScoreEntityId>("00000000000000000000000000000201");
    const auto score_local_b = require_id<ScoreEntityId>("00000000000000000000000000000202");
    const auto score_local_c = require_id<ScoreEntityId>("00000000000000000000000000000203");

    const ScopedMusicalEventId event_a{project_a, event_local_a};
    const ScopedMusicalEventId event_wrong_project{project_b, event_local_a};
    const ScopedScoreEntityId score_a{project_a, score_local_a};
    const ScopedScoreEntityId score_b{project_a, score_local_b};
    const ScopedScoreEntityId score_c{project_a, score_local_c};
    const ScopedScoreEntityId score_wrong_project{project_b, score_local_a};

    const EventProjectionLinkCandidate link_a{event_a, score_a};
    const EventProjectionLinkCandidate link_b{event_a, score_b};
    const EventProjectionLinkCandidate missing_link{event_a, score_c};
    const EventProjectionLinkCandidate wrong_event_project_link{
        event_wrong_project,
        score_a,
    };
    const EventProjectionLinkCandidate wrong_projection_project_link{
        event_a,
        score_wrong_project,
    };

    TestValidationView view_a{project_a};
    view_a.events_.push_back(event_a);
    view_a.scores_ = {score_a, score_b, score_c};

    auto aggregate = ProjectAggregate::initial(project_a);
    if (!add_link(aggregate, link_a, view_a) ||
        !add_link(aggregate, link_b, view_a)) {
        return 1;
    }

    check(
        aggregate.snapshot().revision().value() == 2U,
        "fixture reaches global Project revision two");
    check(
        aggregate.event_projection_relations().links().size() == 2U,
        "fixture owns two authoritative relations");

    const auto removal_snapshot = aggregate.snapshot();
    const RemoveEventProjectionLinkCommand remove_a{
        removal_snapshot,
        link_a,
    };

    view_a.scores_.clear();
    const auto removed = execute_remove_event_projection_link_command(
        aggregate,
        remove_a);
    check(static_cast<bool>(removed), "valid removal command publishes authoritatively");
    check(
        removed.error == RemoveEventProjectionLinkCommandError::none,
        "successful removal command has no top-level error");
    check(
        removed.publication_error ==
            ProjectEventProjectionRemovalPublicationError::none,
        "successful removal command has no publication error");
    check(
        removed.removal_error == EventProjectionRelationStateRemovalError::none,
        "successful removal command has no transition error");
    check(
        aggregate.snapshot().revision().value() == 3U,
        "successful removal command advances the global Project revision exactly once");
    check(
        aggregate.event_projection_relations().links().size() == 1U,
        "successful removal command removes exactly one relation");
    check(
        aggregate.event_projection_relations().links().front().projection_id() ==
            link_b.projection_id(),
        "successful removal command preserves the distinct surviving relation");

    const auto snapshot_after_removal = aggregate.snapshot();
    const auto stale = execute_remove_event_projection_link_command(
        aggregate,
        remove_a);
    check(!stale, "stale removal command fails closed");
    check(
        stale.error == RemoveEventProjectionLinkCommandError::stale_project_snapshot,
        "stale removal command has an explicit top-level error");
    check(
        aggregate.snapshot() == snapshot_after_removal,
        "stale removal command consumes no Project revision");
    check(
        aggregate.event_projection_relations().links().size() == 1U,
        "stale removal command leaves authoritative relation state unchanged");

    const RemoveEventProjectionLinkCommand wrong_command_project{
        ProjectSnapshotToken{project_b, snapshot_after_removal.revision()},
        link_b,
    };
    const auto wrong_command = execute_remove_event_projection_link_command(
        aggregate,
        wrong_command_project);
    check(!wrong_command, "removal command for another Project fails closed");
    check(
        wrong_command.error ==
            RemoveEventProjectionLinkCommandError::command_project_mismatch,
        "wrong command Project has an explicit top-level error");
    check(
        aggregate.snapshot() == snapshot_after_removal,
        "wrong command Project consumes no Project revision");

    const auto wrong_event = execute_remove_event_projection_link_command(
        aggregate,
        RemoveEventProjectionLinkCommand{
            aggregate.snapshot(),
            wrong_event_project_link,
        });
    check(!wrong_event, "wrong-event-Project removal key fails closed");
    check(
        wrong_event.error ==
            RemoveEventProjectionLinkCommandError::event_project_mismatch,
        "wrong-event-Project key has an explicit command error");
    check(
        aggregate.snapshot() == snapshot_after_removal,
        "wrong-event-Project key consumes no Project revision");

    const auto wrong_projection = execute_remove_event_projection_link_command(
        aggregate,
        RemoveEventProjectionLinkCommand{
            aggregate.snapshot(),
            wrong_projection_project_link,
        });
    check(!wrong_projection, "wrong-projection-Project removal key fails closed");
    check(
        wrong_projection.error ==
            RemoveEventProjectionLinkCommandError::projection_project_mismatch,
        "wrong-projection-Project key has an explicit command error");
    check(
        aggregate.snapshot() == snapshot_after_removal,
        "wrong-projection-Project key consumes no Project revision");

    const auto missing = execute_remove_event_projection_link_command(
        aggregate,
        RemoveEventProjectionLinkCommand{
            aggregate.snapshot(),
            missing_link,
        });
    check(!missing, "missing relation removal command fails closed");
    check(
        missing.error == RemoveEventProjectionLinkCommandError::publication_failed,
        "missing relation is classified as publication failure");
    check(
        missing.publication_error ==
            ProjectEventProjectionRemovalPublicationError::relation_state_transition_failed,
        "missing relation preserves the Project publication failure layer");
    check(
        missing.removal_error == EventProjectionRelationStateRemovalError::link_not_found,
        "missing relation preserves the exact core removal reason");
    check(
        aggregate.snapshot() == snapshot_after_removal,
        "missing relation consumes no Project revision");

    for (int iteration = 0; iteration < 200; ++iteration) {
        TestValidationView repeated_view{project_a};
        repeated_view.events_.push_back(event_a);
        repeated_view.scores_.push_back(score_a);

        auto repeated = ProjectAggregate::initial(project_a);
        if (!add_link(repeated, link_a, repeated_view)) {
            return 1;
        }

        repeated_view.scores_.clear();
        const auto repeated_result = execute_remove_event_projection_link_command(
            repeated,
            RemoveEventProjectionLinkCommand{
                repeated.snapshot(),
                link_a,
            });
        check(
            static_cast<bool>(repeated_result),
            "removal command remains deterministic across repeated runs");
        check(
            repeated.snapshot().revision().value() == 2U,
            "repeated add/remove consumes exactly two global revisions");
        check(
            repeated.event_projection_relations().links().empty(),
            "repeated removal command produces identical empty relation state");
    }

    return failures == 0 ? 0 : 1;
}
