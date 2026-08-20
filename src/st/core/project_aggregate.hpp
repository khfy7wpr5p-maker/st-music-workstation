#pragma once

#include "st/core/event_projection_relation_state.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>

namespace st::core {

enum class ProjectEventProjectionPublicationError : std::uint8_t {
    none = 0,
    reentrant_publication,
    validation_view_project_mismatch,
    publication_revalidation_failed,
    relation_state_transition_failed,
    internal_invariant_failure,
};

struct ProjectEventProjectionPublicationResult final {
    std::optional<ProjectSnapshotToken> published_snapshot;
    ProjectEventProjectionPublicationError error{
        ProjectEventProjectionPublicationError::none};
    EventProjectionPublicationRevalidationError revalidation_error{
        EventProjectionPublicationRevalidationError::none};
    EventProjectionRelationStateTransitionError transition_error{
        EventProjectionRelationStateTransitionError::none};

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return published_snapshot.has_value() &&
            error == ProjectEventProjectionPublicationError::none;
    }
};

class ProjectAggregate final {
public:
    ProjectAggregate(const ProjectAggregate&) = delete;
    ProjectAggregate& operator=(const ProjectAggregate&) = delete;
    ProjectAggregate(ProjectAggregate&&) = delete;
    ProjectAggregate& operator=(ProjectAggregate&&) = delete;

    [[nodiscard]] static ProjectAggregate initial(
        ProjectId project_id,
        EventProjectionRelationLimits relation_limits =
            EventProjectionRelationLimits::production_default())
    {
        return ProjectAggregate{
            ProjectSnapshotToken{project_id, ProjectRevision::initial()},
            EventProjectionRelationStateCandidate::initial(
                project_id,
                relation_limits),
        };
    }

    [[nodiscard]] const ProjectSnapshotToken& snapshot() const noexcept
    {
        return snapshot_;
    }

    [[nodiscard]] const EventProjectionRelationStateCandidate&
    event_projection_relations() const noexcept
    {
        return event_projection_relations_;
    }

    [[nodiscard]] ProjectEventProjectionPublicationResult
    publish_event_projection_link_addition(
        const PreparedEventProjectionLinkAddition& prepared,
        const EventProjectionValidationView& current_endpoint_view)
    {
        if (publication_in_progress_) {
            return {
                std::nullopt,
                ProjectEventProjectionPublicationError::reentrant_publication,
                EventProjectionPublicationRevalidationError::none,
                EventProjectionRelationStateTransitionError::none,
            };
        }

        publication_in_progress_ = true;
        class PublicationGuard final {
        public:
            explicit PublicationGuard(bool& flag) noexcept
                : flag_(flag)
            {
            }

            ~PublicationGuard()
            {
                flag_ = false;
            }

            PublicationGuard(const PublicationGuard&) = delete;
            PublicationGuard& operator=(const PublicationGuard&) = delete;

        private:
            bool& flag_;
        } guard{publication_in_progress_};

        const auto endpoint_view_project_id = current_endpoint_view.project_id();
        if (!(endpoint_view_project_id == snapshot_.project_id())) {
            return {
                std::nullopt,
                ProjectEventProjectionPublicationError::validation_view_project_mismatch,
                EventProjectionPublicationRevalidationError::none,
                EventProjectionRelationStateTransitionError::none,
            };
        }

        class OwnedRelationValidationView final : public EventProjectionValidationView {
        public:
            OwnedRelationValidationView(
                const EventProjectionValidationView& endpoint_view,
                const EventProjectionRelationStateCandidate& relation_state,
                ProjectId project_id) noexcept
                : endpoint_view_(endpoint_view)
                , relation_state_(relation_state)
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
                return endpoint_view_.contains_event(id);
            }

            [[nodiscard]] bool contains_score(
                const ScopedScoreEntityId& id) const noexcept override
            {
                return endpoint_view_.contains_score(id);
            }

            [[nodiscard]] bool contains_midi(
                const ScopedMidiEntityId& id) const noexcept override
            {
                return endpoint_view_.contains_midi(id);
            }

            [[nodiscard]] bool contains_tab(
                const ScopedTabEntityId& id) const noexcept override
            {
                return endpoint_view_.contains_tab(id);
            }

            [[nodiscard]] bool contains_audio(
                const ScopedAudioEntityId& id) const noexcept override
            {
                return endpoint_view_.contains_audio(id);
            }

            [[nodiscard]] bool contains_link(
                const EventProjectionLinkCandidate& candidate) const noexcept override
            {
                return std::any_of(
                    relation_state_.links().begin(),
                    relation_state_.links().end(),
                    [&candidate](const EventProjectionLink& existing) noexcept {
                        return existing.event_id() == candidate.event_id() &&
                            existing.projection_id() == candidate.projection_id();
                    });
            }

        private:
            const EventProjectionValidationView& endpoint_view_;
            const EventProjectionRelationStateCandidate& relation_state_;
            ProjectId project_id_;
        };

        const OwnedRelationValidationView authoritative_view{
            current_endpoint_view,
            event_projection_relations_,
            snapshot_.project_id(),
        };

        const auto revalidated = revalidate_prepared_event_projection_link_addition(
            prepared,
            authoritative_view,
            snapshot_.revision());
        if (!revalidated) {
            return {
                std::nullopt,
                ProjectEventProjectionPublicationError::publication_revalidation_failed,
                revalidated.error,
                EventProjectionRelationStateTransitionError::none,
            };
        }

        auto transition = build_event_projection_relation_state_candidate(
            event_projection_relations_,
            snapshot_,
            *revalidated.value);
        if (!transition) {
            return {
                std::nullopt,
                ProjectEventProjectionPublicationError::relation_state_transition_failed,
                EventProjectionPublicationRevalidationError::none,
                transition.error,
            };
        }

        if (!(transition.value->project_id() == snapshot_.project_id()) ||
            !(transition.next_snapshot->project_id() == snapshot_.project_id())) {
            return {
                std::nullopt,
                ProjectEventProjectionPublicationError::internal_invariant_failure,
                EventProjectionPublicationRevalidationError::none,
                EventProjectionRelationStateTransitionError::none,
            };
        }

        static_assert(
            std::is_nothrow_move_assignable_v<EventProjectionRelationStateCandidate>,
            "authoritative relation-state commit must not throw after validation");
        static_assert(
            std::is_nothrow_copy_assignable_v<ProjectSnapshotToken>,
            "authoritative snapshot commit must not throw after validation");

        event_projection_relations_ = std::move(*transition.value);
        snapshot_ = *transition.next_snapshot;

        return {
            snapshot_,
            ProjectEventProjectionPublicationError::none,
            EventProjectionPublicationRevalidationError::none,
            EventProjectionRelationStateTransitionError::none,
        };
    }

private:
    ProjectAggregate(
        ProjectSnapshotToken snapshot,
        EventProjectionRelationStateCandidate event_projection_relations) noexcept
        : snapshot_(std::move(snapshot))
        , event_projection_relations_(std::move(event_projection_relations))
    {
    }

    ProjectSnapshotToken snapshot_;
    EventProjectionRelationStateCandidate event_projection_relations_;
    bool publication_in_progress_{false};
};

} // namespace st::core
