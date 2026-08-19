#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace st::core {

enum class IdParseError : std::uint8_t {
    none = 0,
    wrong_length,
    non_canonical_character,
    all_zero,
};

template <typename T>
struct ParseResult final {
    std::optional<T> value;
    IdParseError error{IdParseError::none};

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return value.has_value();
    }
};

namespace detail {

using IdBytes = std::array<std::uint8_t, 16>;

struct IdBytesParseResult final {
    IdBytes bytes{};
    IdParseError error{IdParseError::none};
    bool valid{false};
};

[[nodiscard]] IdBytesParseResult parse_id_bytes(std::string_view text) noexcept;
[[nodiscard]] std::string id_bytes_to_string(const IdBytes& bytes);

} // namespace detail

template <typename Tag>
class StrongId final {
public:
    [[nodiscard]] static ParseResult<StrongId> parse(std::string_view text) noexcept
    {
        const auto parsed = detail::parse_id_bytes(text);
        if (!parsed.valid) {
            return {std::nullopt, parsed.error};
        }
        return {StrongId{parsed.bytes}, IdParseError::none};
    }

    [[nodiscard]] std::string to_string() const
    {
        return detail::id_bytes_to_string(bytes_);
    }

    friend bool operator==(const StrongId&, const StrongId&) = default;

private:
    using Bytes = detail::IdBytes;

    explicit StrongId(Bytes bytes) noexcept
        : bytes_(bytes)
    {
    }

    Bytes bytes_{};
};

struct ProjectIdTag final {};
struct TrackIdTag final {};
struct ClipIdTag final {};
struct MusicalEventIdTag final {};
struct ScoreEntityIdTag final {};
struct MidiEntityIdTag final {};
struct TabEntityIdTag final {};
struct AudioEntityIdTag final {};

using ProjectId = StrongId<ProjectIdTag>;
using TrackId = StrongId<TrackIdTag>;
using ClipId = StrongId<ClipIdTag>;
using MusicalEventId = StrongId<MusicalEventIdTag>;
using ScoreEntityId = StrongId<ScoreEntityIdTag>;
using MidiEntityId = StrongId<MidiEntityIdTag>;
using TabEntityId = StrongId<TabEntityIdTag>;
using AudioEntityId = StrongId<AudioEntityIdTag>;

template <typename T>
inline constexpr bool is_project_local_id_v =
    std::is_same_v<T, TrackId> ||
    std::is_same_v<T, ClipId> ||
    std::is_same_v<T, MusicalEventId> ||
    std::is_same_v<T, ScoreEntityId> ||
    std::is_same_v<T, MidiEntityId> ||
    std::is_same_v<T, TabEntityId> ||
    std::is_same_v<T, AudioEntityId>;

template <typename LocalId>
    requires is_project_local_id_v<LocalId>
class ProjectScopedId final {
public:
    ProjectScopedId(ProjectId project_id, LocalId local_id) noexcept
        : project_id_(project_id)
        , local_id_(local_id)
    {
    }

    [[nodiscard]] const ProjectId& project_id() const noexcept
    {
        return project_id_;
    }

    [[nodiscard]] const LocalId& local_id() const noexcept
    {
        return local_id_;
    }

    friend bool operator==(const ProjectScopedId&, const ProjectScopedId&) = default;

private:
    ProjectId project_id_;
    LocalId local_id_;
};

using ScopedTrackId = ProjectScopedId<TrackId>;
using ScopedClipId = ProjectScopedId<ClipId>;
using ScopedMusicalEventId = ProjectScopedId<MusicalEventId>;
using ScopedScoreEntityId = ProjectScopedId<ScoreEntityId>;
using ScopedMidiEntityId = ProjectScopedId<MidiEntityId>;
using ScopedTabEntityId = ProjectScopedId<TabEntityId>;
using ScopedAudioEntityId = ProjectScopedId<AudioEntityId>;

} // namespace st::core
