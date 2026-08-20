#pragma once

#include "st/core/event_projection_publication.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <utility>
#include <vector>

namespace st::core {

inline constexpr std::size_t kAbsoluteMaxEventProjectionLinks = 1U << 20U;

class EventProjectionRelationLimits final {
public:
    [[nodiscard]] static constexpr std::optional<EventProjectionRelationLimits> create(
        std::size_t max_links) noexcept
    {
        if (max_links == 0U || max_links > kAbsoluteMaxEventProjectionLinks) {
            return std::nullopt;
        }
        return EventProjectionRelationLimits{max_links};
    }

    [[nodiscard]] static constexpr EventProjectionRelationLimits production_default() noexcept
    {
        return EventProjectionRelationLimits{kAbsoluteMaxEventProjectionLinks};
    }

    [[nodiscard]] constexpr std::size_t max_links() const noexcept
    {
        return max_links_;
    }

    friend constexpr bool operator==(
        EventProjectionRelationLimits,
        EventProjectionRelationLimits) = default;

private:
    explicit constexpr EventProjectionRelationLimits(std::size_t max_links) noexcept
        : max_links_(max_links)
    {
    }

    std::size_t max_links_;
};

class EventProjectionLink final {
public:
    [[nodiscard]] const ScopedMusicalEventId& event_id() const noexcept
    {
        return candidate_.event_id();
    }

    [[nodiscard]] const ProjectionScopedId& projection_id() const noexcept
    {
        return candidate_.projection_id();
    }

    [[nodiscard]] ProjectionKind projection_kind() const noexcept
    {
        return candidate_.projection_kind();
    }

    [[nodiscard]] ProjectId projection_project_id() const noexcept
    {
        return candidate_.projection_project_id();
    }

    friend bool operator==(const EventProjectionLink&, const EventProjectionLink&) = default;

private:
    friend class EventProjectionRelationStateCandidate;
    friend struct EventProjectionRelationStateTransitionResult;
    friend EventProjectionRelationStateTransitionResult
    build_event_projection_relation_state_candidate(
        const EventProjectionRelationStateCandidate& current,
        const RevalidatedEventProjectionLinkAddition& addition);

    explicit EventProjectionLink(EventProjectionLinkCandidate candidate) noexcept
        : candidate_(std::move(candidate))
    {
    }

    EventProjectionLinkCandidate candidate_;
};

class EventProjectionRelationStateCandidate final {
public:
    [[nodiscard]] static EventProjectionRelationStateCandidate initial(
        ProjectId project_id,
        EventProjectionRelationLimits limits =
            EventProjectionRelationLimits::production_default())
    {
        return EventProjectionRelationStateCandidate{
            ProjectSnapshotToken{project_id, ProjectRevision::initial()},
            limits,
            {},
        };
    }

    [[nodiscard]] const ProjectSnapshotToken& snapshot() const noexcept
    {
        return snapshot_;
    }

    [[nodiscard]] EventProjectionRelationLimits limits() const noexcept
    {
        return limits_;
    }

    [[nodiscard]] const std::vector<EventProjectionLink>& links() const noexcept
    {
        return links_;
    }

private:
    friend struct EventProjectionRelationStateTransitionResult;
    friend EventProjectionRelationStateTransitionResult
    build_event_projection_relation_state_candidate(
        const EventProjectionRelationStateCandidate& current,
        const RevalidatedEventProjectionLinkAddition& addition);

    EventProjectionRelationStateCandidate(
        ProjectSnapshotToken snapshot,
        EventProjectionRelationLimits limits,
        std::vector<EventProjectionLink> links) noexcept
        : snapshot_(std::move(snapshot))
        , limits_(limits)
        , links_(std::move(links))
    {
    }

    ProjectSnapshotToken snapshot_;
    EventProjectionRelationLimits limits_;
    std::vector<EventProjectionLink> links_;
};

enum class EventProjectionRelationStateTransitionError : std::uint8_t {
    none = 0,
    base_snapshot_mismatch,
    invalid_next_snapshot,
    link_wrong_project,
    duplicate_link,
    relation_limit_exceeded,
    allocation_failure,
};

struct EventProjectionRelationStateTransitionResult final {
    std::optional<EventProjectionRelationStateCandidate> value;
    EventProjectionRelationStateTransitionError error{
        EventProjectionRelationStateTransitionError::none};

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return value.has_value();
    }
};

[[nodiscard]] inline EventProjectionRelationStateTransitionResult
build_event_projection_relation_state_candidate(
    const EventProjectionRelationStateCandidate& current,
    const RevalidatedEventProjectionLinkAddition& addition)
{
    if (!(current.snapshot() == addition.base_snapshot)) {
        return {
            std::nullopt,
            EventProjectionRelationStateTransitionError::base_snapshot_mismatch,
        };
    }

    const auto expected_next_revision = current.snapshot().revision().next();
    if (!expected_next_revision ||
        !(addition.next_snapshot.project_id() == current.snapshot().project_id()) ||
        !(addition.next_snapshot.revision() == *expected_next_revision)) {
        return {
            std::nullopt,
            EventProjectionRelationStateTransitionError::invalid_next_snapshot,
        };
    }

    if (!(addition.link.event_id().project_id() == current.snapshot().project_id()) ||
        !(addition.link.projection_project_id() == current.snapshot().project_id())) {
        return {
            std::nullopt,
            EventProjectionRelationStateTransitionError::link_wrong_project,
        };
    }

    const auto duplicate = std::find_if(
        current.links().begin(),
        current.links().end(),
        [&addition](const EventProjectionLink& existing) noexcept {
            return existing.event_id() == addition.link.event_id() &&
                existing.projection_id() == addition.link.projection_id();
        });
    if (duplicate != current.links().end()) {
        return {
            std::nullopt,
            EventProjectionRelationStateTransitionError::duplicate_link,
        };
    }

    if (current.links().size() >= current.limits().max_links()) {
        return {
            std::nullopt,
            EventProjectionRelationStateTransitionError::relation_limit_exceeded,
        };
    }

    try {
        auto next_links = current.links();
        next_links.reserve(current.links().size() + 1U);
        next_links.emplace_back(EventProjectionLink{addition.link});

        return {
            EventProjectionRelationStateCandidate{
                addition.next_snapshot,
                current.limits(),
                std::move(next_links),
            },
            EventProjectionRelationStateTransitionError::none,
        };
    } catch (const std::bad_alloc&) {
        return {
            std::nullopt,
            EventProjectionRelationStateTransitionError::allocation_failure,
        };
    }
}

} // namespace st::core
