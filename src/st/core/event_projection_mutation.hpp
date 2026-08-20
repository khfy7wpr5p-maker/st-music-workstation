#pragma once

#include "st/core/event_projection_link.hpp"
#include "st/core/project_revision.hpp"

#include <cstdint>
#include <optional>

namespace st::core {

enum class EventProjectionMutationPreparationError : std::uint8_t {
    none = 0,
    stale_expected_revision,
    event_wrong_project,
    projection_wrong_project,
    event_missing,
    projection_missing,
    duplicate_link,
    revision_overflow,
    relation_validation_failure,
};

struct EventProjectionMutationPreparationResult;

class PreparedEventProjectionLinkAddition final {
public:
    [[nodiscard]] const ProjectSnapshotToken& base_snapshot() const noexcept
    {
        return base_snapshot_;
    }

    [[nodiscard]] ProjectRevision next_revision() const noexcept
    {
        return next_revision_;
    }

    [[nodiscard]] const EventProjectionLinkCandidate& link() const noexcept
    {
        return link_;
    }

private:
    friend EventProjectionMutationPreparationResult
    prepare_event_projection_link_addition(
        const EventProjectionLinkCandidate& candidate,
        const EventProjectionValidationView& view,
        ProjectRevision current_revision,
        ProjectRevision expected_revision);

    PreparedEventProjectionLinkAddition(
        ProjectSnapshotToken base_snapshot,
        ProjectRevision next_revision,
        EventProjectionLinkCandidate link) noexcept
        : base_snapshot_(base_snapshot)
        , next_revision_(next_revision)
        , link_(link)
    {
    }

    ProjectSnapshotToken base_snapshot_;
    ProjectRevision next_revision_;
    EventProjectionLinkCandidate link_;
};

struct EventProjectionMutationPreparationResult final {
    std::optional<PreparedEventProjectionLinkAddition> value;
    EventProjectionMutationPreparationError error{
        EventProjectionMutationPreparationError::none};

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return value.has_value();
    }
};

[[nodiscard]] inline EventProjectionMutationPreparationError map_projection_validation_error(
    EventProjectionValidationError error) noexcept
{
    switch (error) {
    case EventProjectionValidationError::none:
        return EventProjectionMutationPreparationError::none;
    case EventProjectionValidationError::event_wrong_project:
        return EventProjectionMutationPreparationError::event_wrong_project;
    case EventProjectionValidationError::projection_wrong_project:
        return EventProjectionMutationPreparationError::projection_wrong_project;
    case EventProjectionValidationError::event_missing:
        return EventProjectionMutationPreparationError::event_missing;
    case EventProjectionValidationError::projection_missing:
        return EventProjectionMutationPreparationError::projection_missing;
    case EventProjectionValidationError::duplicate_link:
        return EventProjectionMutationPreparationError::duplicate_link;
    }

    return EventProjectionMutationPreparationError::relation_validation_failure;
}

[[nodiscard]] inline EventProjectionMutationPreparationResult
prepare_event_projection_link_addition(
    const EventProjectionLinkCandidate& candidate,
    const EventProjectionValidationView& view,
    ProjectRevision current_revision,
    ProjectRevision expected_revision)
{
    if (!(current_revision == expected_revision)) {
        return {
            std::nullopt,
            EventProjectionMutationPreparationError::stale_expected_revision,
        };
    }

    const auto relation_validation =
        validate_event_projection_link_candidate(candidate, view);
    if (!relation_validation) {
        return {
            std::nullopt,
            map_projection_validation_error(relation_validation.error),
        };
    }

    const auto revision_advance =
        prepare_revision_advance(current_revision, current_revision);
    if (!revision_advance) {
        return {
            std::nullopt,
            EventProjectionMutationPreparationError::revision_overflow,
        };
    }

    return {
        PreparedEventProjectionLinkAddition{
            ProjectSnapshotToken{candidate.event_id().project_id(), current_revision},
            *revision_advance.next_revision,
            candidate,
        },
        EventProjectionMutationPreparationError::none,
    };
}

} // namespace st::core
