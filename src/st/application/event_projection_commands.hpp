#pragma once

#include "st/core/project_aggregate.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>

namespace st::application {

class AddEventProjectionLinkCommand final {
public:
    AddEventProjectionLinkCommand(
        st::core::ProjectSnapshotToken expected_snapshot,
        st::core::EventProjectionLinkCandidate candidate) noexcept
        : expected_snapshot_(expected_snapshot)
        , candidate_(std::move(candidate))
    {
    }

    [[nodiscard]] const st::core::ProjectSnapshotToken& expected_snapshot() const noexcept
    {
        return expected_snapshot_;
    }

    [[nodiscard]] const st::core::EventProjectionLinkCandidate& candidate() const noexcept
    {
        return candidate_;
    }

private:
    st::core::ProjectSnapshotToken expected_snapshot_;
    st::core::EventProjectionLinkCandidate candidate_;
};

enum class AddEventProjectionLinkCommandError : std::uint8_t {
    none = 0,
    command_project_mismatch,
    stale_project_snapshot,
    validation_view_project_mismatch,
    preparation_failed,
    publication_failed,
};

struct AddEventProjectionLinkCommandResult final {
    std::optional<st::core::ProjectSnapshotToken> published_snapshot;
    AddEventProjectionLinkCommandError error{
        AddEventProjectionLinkCommandError::none};
    st::core::EventProjectionMutationPreparationError preparation_error{
        st::core::EventProjectionMutationPreparationError::none};
    st::core::ProjectEventProjectionPublicationError publication_error{
        st::core::ProjectEventProjectionPublicationError::none};
    st::core::EventProjectionPublicationRevalidationError revalidation_error{
        st::core::EventProjectionPublicationRevalidationError::none};
    st::core::EventProjectionRelationStateTransitionError transition_error{
        st::core::EventProjectionRelationStateTransitionError::none};

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return published_snapshot.has_value() &&
            error == AddEventProjectionLinkCommandError::none;
    }
};

[[nodiscard]] inline AddEventProjectionLinkCommandResult
execute_add_event_projection_link_command(
    st::core::ProjectAggregate& aggregate,
    const AddEventProjectionLinkCommand& command,
    const st::core::EventProjectionValidationView& current_endpoint_view)
{
    const auto base_snapshot = aggregate.snapshot();

    if (!(command.expected_snapshot().project_id() == base_snapshot.project_id())) {
        return {
            std::nullopt,
            AddEventProjectionLinkCommandError::command_project_mismatch,
        };
    }

    if (!(command.expected_snapshot() == base_snapshot)) {
        return {
            std::nullopt,
            AddEventProjectionLinkCommandError::stale_project_snapshot,
        };
    }

    if (!(command.candidate().event_id().project_id() == base_snapshot.project_id())) {
        return {
            std::nullopt,
            AddEventProjectionLinkCommandError::preparation_failed,
            st::core::EventProjectionMutationPreparationError::event_wrong_project,
        };
    }

    if (!(command.candidate().projection_project_id() == base_snapshot.project_id())) {
        return {
            std::nullopt,
            AddEventProjectionLinkCommandError::preparation_failed,
            st::core::EventProjectionMutationPreparationError::projection_wrong_project,
        };
    }

    const auto endpoint_view_project_id = current_endpoint_view.project_id();
    if (!(endpoint_view_project_id == base_snapshot.project_id())) {
        return {
            std::nullopt,
            AddEventProjectionLinkCommandError::validation_view_project_mismatch,
        };
    }

    if (!(aggregate.snapshot() == base_snapshot)) {
        return {
            std::nullopt,
            AddEventProjectionLinkCommandError::stale_project_snapshot,
        };
    }

    class AuthoritativePreparationView final
        : public st::core::EventProjectionValidationView {
    public:
        AuthoritativePreparationView(
            const st::core::EventProjectionValidationView& endpoint_view,
            const st::core::EventProjectionRelationStateCandidate& relation_state,
            st::core::ProjectId project_id) noexcept
            : endpoint_view_(endpoint_view)
            , relation_state_(relation_state)
            , project_id_(project_id)
        {
        }

        [[nodiscard]] st::core::ProjectId project_id() const noexcept override
        {
            return project_id_;
        }

        [[nodiscard]] bool contains_event(
            const st::core::ScopedMusicalEventId& id) const noexcept override
        {
            return endpoint_view_.contains_event(id);
        }

        [[nodiscard]] bool contains_score(
            const st::core::ScopedScoreEntityId& id) const noexcept override
        {
            return endpoint_view_.contains_score(id);
        }

        [[nodiscard]] bool contains_midi(
            const st::core::ScopedMidiEntityId& id) const noexcept override
        {
            return endpoint_view_.contains_midi(id);
        }

        [[nodiscard]] bool contains_tab(
            const st::core::ScopedTabEntityId& id) const noexcept override
        {
            return endpoint_view_.contains_tab(id);
        }

        [[nodiscard]] bool contains_audio(
            const st::core::ScopedAudioEntityId& id) const noexcept override
        {
            return endpoint_view_.contains_audio(id);
        }

        [[nodiscard]] bool contains_link(
            const st::core::EventProjectionLinkCandidate& candidate) const noexcept override
        {
            return std::any_of(
                relation_state_.links().begin(),
                relation_state_.links().end(),
                [&candidate](const st::core::EventProjectionLink& existing) noexcept {
                    return existing.event_id() == candidate.event_id() &&
                        existing.projection_id() == candidate.projection_id();
                });
        }

    private:
        const st::core::EventProjectionValidationView& endpoint_view_;
        const st::core::EventProjectionRelationStateCandidate& relation_state_;
        st::core::ProjectId project_id_;
    };

    const AuthoritativePreparationView preparation_view{
        current_endpoint_view,
        aggregate.event_projection_relations(),
        base_snapshot.project_id(),
    };

    const auto prepared = st::core::prepare_event_projection_link_addition(
        command.candidate(),
        preparation_view,
        base_snapshot.revision(),
        command.expected_snapshot().revision());

    if (!(aggregate.snapshot() == base_snapshot)) {
        return {
            std::nullopt,
            AddEventProjectionLinkCommandError::stale_project_snapshot,
        };
    }

    if (!prepared) {
        return {
            std::nullopt,
            AddEventProjectionLinkCommandError::preparation_failed,
            prepared.error,
        };
    }

    const auto published = aggregate.publish_event_projection_link_addition(
        *prepared.value,
        current_endpoint_view);
    if (!published) {
        return {
            std::nullopt,
            AddEventProjectionLinkCommandError::publication_failed,
            st::core::EventProjectionMutationPreparationError::none,
            published.error,
            published.revalidation_error,
            published.transition_error,
        };
    }

    return {
        published.published_snapshot,
        AddEventProjectionLinkCommandError::none,
    };
}

} // namespace st::application
