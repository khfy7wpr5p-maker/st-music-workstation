#pragma once

#include "st/core/identity.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace st::core {

enum class EntropyReadStatus : std::uint8_t {
    success = 0,
    failure,
};

struct EntropyReadResult final {
    EntropyReadStatus status{EntropyReadStatus::failure};
    std::size_t bytes_written{0};
};

class IdentityEntropySource {
public:
    virtual ~IdentityEntropySource() = default;

    [[nodiscard]] virtual EntropyReadResult read(
        std::span<std::uint8_t> destination) noexcept = 0;
};

enum class IdAllocationError : std::uint8_t {
    none = 0,
    entropy_failure,
    incomplete_entropy,
    candidate_exhausted,
};

template <typename Id>
class IdentityCollisionView {
public:
    virtual ~IdentityCollisionView() = default;

    [[nodiscard]] virtual bool contains(const Id& candidate) const noexcept = 0;
};

template <typename Id>
struct IdAllocationResult final {
    std::optional<Id> value;
    IdAllocationError error{IdAllocationError::none};
    std::size_t attempts{0};

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return value.has_value();
    }
};

template <typename Tag>
class Random128IdAllocator final {
public:
    using Id = StrongId<Tag>;
    static constexpr std::size_t max_attempts = 16U;

    [[nodiscard]] IdAllocationResult<Id> allocate_candidate(
        IdentityEntropySource& entropy_source,
        const IdentityCollisionView<Id>& collision_view) const noexcept
    {
        for (std::size_t attempt = 1U; attempt <= max_attempts; ++attempt) {
            detail::IdBytes bytes{};
            const auto entropy_result = entropy_source.read(
                std::span<std::uint8_t>{bytes.data(), bytes.size()});

            if (entropy_result.status != EntropyReadStatus::success) {
                return {std::nullopt, IdAllocationError::entropy_failure, attempt};
            }

            if (entropy_result.bytes_written != bytes.size()) {
                return {std::nullopt, IdAllocationError::incomplete_entropy, attempt};
            }

            const bool all_zero = std::all_of(
                bytes.begin(),
                bytes.end(),
                [](std::uint8_t value) { return value == 0U; });
            if (all_zero) {
                continue;
            }

            const Id candidate{bytes};
            if (collision_view.contains(candidate)) {
                continue;
            }

            return {candidate, IdAllocationError::none, attempt};
        }

        return {
            std::nullopt,
            IdAllocationError::candidate_exhausted,
            max_attempts,
        };
    }
};

using ProjectIdAllocator = Random128IdAllocator<ProjectIdTag>;
using TrackIdAllocator = Random128IdAllocator<TrackIdTag>;
using ClipIdAllocator = Random128IdAllocator<ClipIdTag>;
using MusicalEventIdAllocator = Random128IdAllocator<MusicalEventIdTag>;
using ScoreEntityIdAllocator = Random128IdAllocator<ScoreEntityIdTag>;
using MidiEntityIdAllocator = Random128IdAllocator<MidiEntityIdTag>;
using TabEntityIdAllocator = Random128IdAllocator<TabEntityIdTag>;
using AudioEntityIdAllocator = Random128IdAllocator<AudioEntityIdTag>;

} // namespace st::core
