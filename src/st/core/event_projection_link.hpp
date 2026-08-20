#pragma once

#include "st/core/identity.hpp"

#include <cstdint>
#include <type_traits>
#include <utility>
#include <variant>

namespace st::core {

enum class ProjectionKind : std::uint8_t {
    score = 0,
    midi,
    guitar_tab,
    audio,
};

using ProjectionScopedId = std::variant<
    ScopedScoreEntityId,
    ScopedMidiEntityId,
    ScopedTabEntityId,
    ScopedAudioEntityId>;

class EventProjectionLinkCandidate final {
public:
    EventProjectionLinkCandidate(
        ScopedMusicalEventId event_id,
        ProjectionScopedId projection_id) noexcept
        : event_id_(std::move(event_id))
        , projection_id_(std::move(projection_id))
    {
    }

    [[nodiscard]] const ScopedMusicalEventId& event_id() const noexcept
    {
        return event_id_;
    }

    [[nodiscard]] const ProjectionScopedId& projection_id() const noexcept
    {
        return projection_id_;
    }

    [[nodiscard]] ProjectionKind projection_kind() const noexcept
    {
        return std::visit(
            [](const auto& id) noexcept {
                using Id = std::decay_t<decltype(id)>;
                if constexpr (std::is_same_v<Id, ScopedScoreEntityId>) {
                    return ProjectionKind::score;
                } else if constexpr (std::is_same_v<Id, ScopedMidiEntityId>) {
                    return ProjectionKind::midi;
                } else if constexpr (std::is_same_v<Id, ScopedTabEntityId>) {
                    return ProjectionKind::guitar_tab;
                } else {
                    static_assert(std::is_same_v<Id, ScopedAudioEntityId>);
                    return ProjectionKind::audio;
                }
            },
            projection_id_);
    }

    [[nodiscard]] ProjectId projection_project_id() const noexcept
    {
        return std::visit(
            [](const auto& id) noexcept {
                return id.project_id();
            },
            projection_id_);
    }

    friend bool operator==(
        const EventProjectionLinkCandidate&,
        const EventProjectionLinkCandidate&) = default;

private:
    ScopedMusicalEventId event_id_;
    ProjectionScopedId projection_id_;
};

class EventProjectionValidationView {
public:
    virtual ~EventProjectionValidationView() = default;

    [[nodiscard]] virtual ProjectId project_id() const noexcept = 0;
    [[nodiscard]] virtual bool contains_event(
        const ScopedMusicalEventId& id) const noexcept = 0;
    [[nodiscard]] virtual bool contains_score(
        const ScopedScoreEntityId& id) const noexcept = 0;
    [[nodiscard]] virtual bool contains_midi(
        const ScopedMidiEntityId& id) const noexcept = 0;
    [[nodiscard]] virtual bool contains_tab(
        const ScopedTabEntityId& id) const noexcept = 0;
    [[nodiscard]] virtual bool contains_audio(
        const ScopedAudioEntityId& id) const noexcept = 0;
    [[nodiscard]] virtual bool contains_link(
        const EventProjectionLinkCandidate& link) const noexcept = 0;
};

enum class EventProjectionValidationError : std::uint8_t {
    none = 0,
    event_wrong_project,
    projection_wrong_project,
    event_missing,
    projection_missing,
    duplicate_link,
};

struct EventProjectionValidationResult final {
    EventProjectionValidationError error{EventProjectionValidationError::none};

    [[nodiscard]] explicit constexpr operator bool() const noexcept
    {
        return error == EventProjectionValidationError::none;
    }
};

[[nodiscard]] inline EventProjectionValidationResult validate_event_projection_link_candidate(
    const EventProjectionLinkCandidate& candidate,
    const EventProjectionValidationView& view) noexcept
{
    const auto project_id = view.project_id();

    if (!(candidate.event_id().project_id() == project_id)) {
        return {EventProjectionValidationError::event_wrong_project};
    }

    if (!(candidate.projection_project_id() == project_id)) {
        return {EventProjectionValidationError::projection_wrong_project};
    }

    if (!view.contains_event(candidate.event_id())) {
        return {EventProjectionValidationError::event_missing};
    }

    const bool projection_exists = std::visit(
        [&view](const auto& projection_id) noexcept {
            using Id = std::decay_t<decltype(projection_id)>;
            if constexpr (std::is_same_v<Id, ScopedScoreEntityId>) {
                return view.contains_score(projection_id);
            } else if constexpr (std::is_same_v<Id, ScopedMidiEntityId>) {
                return view.contains_midi(projection_id);
            } else if constexpr (std::is_same_v<Id, ScopedTabEntityId>) {
                return view.contains_tab(projection_id);
            } else {
                static_assert(std::is_same_v<Id, ScopedAudioEntityId>);
                return view.contains_audio(projection_id);
            }
        },
        candidate.projection_id());

    if (!projection_exists) {
        return {EventProjectionValidationError::projection_missing};
    }

    if (view.contains_link(candidate)) {
        return {EventProjectionValidationError::duplicate_link};
    }

    return {EventProjectionValidationError::none};
}

} // namespace st::core
