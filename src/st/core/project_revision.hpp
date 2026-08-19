#pragma once

#include "st/core/identity.hpp"

#include <cstdint>
#include <limits>
#include <optional>

namespace st::core {

class ProjectRevision final {
public:
    using Value = std::uint64_t;

    [[nodiscard]] static constexpr ProjectRevision initial() noexcept
    {
        return ProjectRevision{0U};
    }

    [[nodiscard]] static constexpr ProjectRevision from_persisted(Value value) noexcept
    {
        return ProjectRevision{value};
    }

    [[nodiscard]] constexpr Value value() const noexcept
    {
        return value_;
    }

    [[nodiscard]] constexpr std::optional<ProjectRevision> next() const noexcept
    {
        if (value_ == std::numeric_limits<Value>::max()) {
            return std::nullopt;
        }
        return ProjectRevision{value_ + 1U};
    }

    friend constexpr bool operator==(ProjectRevision, ProjectRevision) = default;

private:
    explicit constexpr ProjectRevision(Value value) noexcept
        : value_(value)
    {
    }

    Value value_{0U};
};

enum class RevisionAdvanceError : std::uint8_t {
    none = 0,
    stale_expected_revision,
    overflow,
};

struct RevisionAdvanceResult final {
    std::optional<ProjectRevision> next_revision;
    RevisionAdvanceError error{RevisionAdvanceError::none};

    [[nodiscard]] explicit constexpr operator bool() const noexcept
    {
        return next_revision.has_value();
    }
};

[[nodiscard]] constexpr RevisionAdvanceResult prepare_revision_advance(
    ProjectRevision current,
    ProjectRevision expected) noexcept
{
    if (!(current == expected)) {
        return {std::nullopt, RevisionAdvanceError::stale_expected_revision};
    }

    const auto next = current.next();
    if (!next.has_value()) {
        return {std::nullopt, RevisionAdvanceError::overflow};
    }

    return {next, RevisionAdvanceError::none};
}

class ProjectSnapshotToken final {
public:
    ProjectSnapshotToken(
        ProjectId project_id,
        ProjectRevision revision) noexcept
        : project_id_(project_id)
        , revision_(revision)
    {
    }

    [[nodiscard]] const ProjectId& project_id() const noexcept
    {
        return project_id_;
    }

    [[nodiscard]] ProjectRevision revision() const noexcept
    {
        return revision_;
    }

    [[nodiscard]] bool matches(
        const ProjectId& current_project_id,
        ProjectRevision current_revision) const noexcept
    {
        return project_id_ == current_project_id && revision_ == current_revision;
    }

    friend bool operator==(const ProjectSnapshotToken&, const ProjectSnapshotToken&) = default;

private:
    ProjectId project_id_;
    ProjectRevision revision_;
};

} // namespace st::core
