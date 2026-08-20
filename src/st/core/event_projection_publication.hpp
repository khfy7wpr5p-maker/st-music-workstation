#pragma once

#include "st/core/event_projection_mutation.hpp"

#include <cstdint>
#include <optional>

namespace st::core {

enum class EventProjectionPublicationRevalidationError : std::uint8_t {
    none = 0,
    prepared_project_mismatch,
    stale_prepared_revision,
    invalid_prepared_transition,
    event_wrong_project,
    projection_wrong_project,
    event_missing,
    projection_missing,
    duplicate_link,
    relation_validation_failure,
};

struct EventProjectionPublicationRevalidationResult;

class RevalidatedEventProjectionLinkAddition final {
public:
    const ProjectSnapshotToken base_snapshot;
    const ProjectSnapshotToken next_snapshot;
    const EventProjectionLinkCandidate link;

private:
    friend EventProjectionPublicationRevalidationResult
    revalidate_prepared_event_projection_link_addition(
        const PreparedEventProjectionLinkAddition& prepared,
        const EventProjectionValidationView& current_view,
        ProjectRevision current_revision);

    RevalidatedEventProjectionLinkAddition(
        ProjectSnapshotToken base_snapshot_value,
        ProjectSnapshotToken next_snapshot_value,
        EventProjectionLinkCandidate link_value) noexcept
        : base_snapshot(base_snapshot_value)
        , next_snapshot(next_snapshot_value)
        , link(link_value)
    {
    }
};

struct EventProjectionPublicationRevalidationResult final {
    std::optional<RevalidatedEventProjectionLinkAddition> value;
    EventProjectionPublicationRevalidationError error{
        EventProjectionPublicationRevalidationError::none};

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return value.has_value();
    }
};

[[nodiscard]] inline EventProjectionPublicationRevalidationError
map_projection_publication_validation_error(
    EventProjectionValidationError error) noexcept
{
    switch (error) {
    case EventProjectionValidationError::none:
        return EventProjectionPublicationRevalidationError::none;
    case EventProjectionValidationError::event_wrong_project:
        return EventProjectionPublicationRevalidationError::event_wrong_project;
    case EventProjectionValidationError::projection_wrong_project:
        return EventProjectionPublicationRevalidationError::projection_wrong_project;
    case EventProjectionValidationError::event_missing:
        return EventProjectionPublicationRevalidationError::event_missing;
    case EventProjectionValidationError::projection_missing:
        return EventProjectionPublicationRevalidationError::projection_missing;
    case EventProjectionValidationError::duplicate_link:
        return EventProjectionPublicationRevalidationError::duplicate_link;
    }

    return EventProjectionPublicationRevalidationError::relation_validation_failure;
}

[[nodiscard]] inline EventProjectionPublicationRevalidationResult
revalidate_prepared_event_projection_link_addition(
    const PreparedEventProjectionLinkAddition& prepared,
    const EventProjectionValidationView& current_view,
    ProjectRevision current_revision)
{
    const auto current_project_id = current_view.project_id();

    if (!(prepared.base_snapshot.project_id() == current_project_id)) {
        return {
            std::nullopt,
            EventProjectionPublicationRevalidationError::prepared_project_mismatch,
        };
    }

    if (!(prepared.base_snapshot.revision() == current_revision)) {
        return {
            std::nullopt,
            EventProjectionPublicationRevalidationError::stale_prepared_revision,
        };
    }

    const auto expected_next_revision = current_revision.next();
    if (!expected_next_revision || !(prepared.next_revision == *expected_next_revision)) {
        return {
            std::nullopt,
            EventProjectionPublicationRevalidationError::invalid_prepared_transition,
        };
    }

    class PinnedProjectValidationView final : public EventProjectionValidationView {
    public:
        PinnedProjectValidationView(
            const EventProjectionValidationView& source,
            ProjectId project_id) noexcept
            : source_(source)
            , project_id_(project_id)
        {
        }

        [[nodiscard]] ProjectId project_id() const noexcept override
        {
            return project_id_;
        }

        [[nodiscard]] bool contains_event(
            const ScopedMusicalEventId& id) const noexcept override
        {
            return source_.contains_event(id);
        }

        [[nodiscard]] bool contains_score(
            const ScopedScoreEntityId& id) const noexcept override
        {
            return source_.contains_score(id);
        }

        [[nodiscard]] bool contains_midi(
            const ScopedMidiEntityId& id) const noexcept override
        {
            return source_.contains_midi(id);
        }

        [[nodiscard]] bool contains_tab(
            const ScopedTabEntityId& id) const noexcept override
        {
            return source_.contains_tab(id);
        }

        [[nodiscard]] bool contains_audio(
            const ScopedAudioEntityId& id) const noexcept override
        {
            return source_.contains_audio(id);
        }

        [[nodiscard]] bool contains_link(
            const EventProjectionLinkCandidate& link) const noexcept override
        {
            return source_.contains_link(link);
        }

    private:
        const EventProjectionValidationView& source_;
        ProjectId project_id_;
    };

    const PinnedProjectValidationView pinned_view{
        current_view,
        current_project_id,
    };
    const auto relation_validation =
        validate_event_projection_link_candidate(prepared.link, pinned_view);
    if (!relation_validation) {
        return {
            std::nullopt,
            map_projection_publication_validation_error(relation_validation.error),
        };
    }

    return {
        RevalidatedEventProjectionLinkAddition{
            ProjectSnapshotToken{current_project_id, current_revision},
            ProjectSnapshotToken{current_project_id, prepared.next_revision},
            prepared.link,
        },
        EventProjectionPublicationRevalidationError::none,
    };
}

} // namespace st::core
