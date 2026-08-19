#include "st/core/identity_allocator.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
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

st::core::detail::IdBytes bytes_with_last(std::uint8_t value)
{
    st::core::detail::IdBytes bytes{};
    bytes.back() = value;
    return bytes;
}

class ScriptedEntropySource final : public st::core::IdentityEntropySource {
public:
    struct Entry final {
        st::core::EntropyReadStatus status{st::core::EntropyReadStatus::success};
        std::size_t bytes_written{16U};
        st::core::detail::IdBytes bytes{};
    };

    explicit ScriptedEntropySource(std::vector<Entry> entries)
        : entries_(std::move(entries))
    {
    }

    [[nodiscard]] st::core::EntropyReadResult read(
        std::span<std::uint8_t> destination) noexcept override
    {
        ++calls_;
        all_destinations_exact_ = all_destinations_exact_ && destination.size() == 16U;

        if (next_ >= entries_.size()) {
            return {st::core::EntropyReadStatus::failure, 0U};
        }

        const auto& entry = entries_[next_++];
        if (entry.status != st::core::EntropyReadStatus::success) {
            return {entry.status, entry.bytes_written};
        }

        const auto copy_count = std::min({
            destination.size(),
            entry.bytes.size(),
            entry.bytes_written,
        });
        std::copy_n(entry.bytes.begin(), copy_count, destination.begin());
        return {entry.status, entry.bytes_written};
    }

    [[nodiscard]] std::size_t calls() const noexcept
    {
        return calls_;
    }

    [[nodiscard]] bool all_destinations_exact() const noexcept
    {
        return all_destinations_exact_;
    }

private:
    std::vector<Entry> entries_;
    std::size_t next_{0U};
    std::size_t calls_{0U};
    bool all_destinations_exact_{true};
};

template <typename Id>
class VectorUniquenessScope final : public st::core::IdentityUniquenessScope<Id> {
public:
    explicit VectorUniquenessScope(std::vector<Id> existing = {})
        : existing_(std::move(existing))
    {
    }

    [[nodiscard]] bool contains(const Id& candidate) const noexcept override
    {
        ++calls_;
        return std::find(existing_.begin(), existing_.end(), candidate) != existing_.end();
    }

    [[nodiscard]] std::size_t calls() const noexcept
    {
        return calls_;
    }

private:
    std::vector<Id> existing_;
    mutable std::size_t calls_{0U};
};

ScriptedEntropySource::Entry success_entry(std::uint8_t last_byte)
{
    return {
        st::core::EntropyReadStatus::success,
        16U,
        bytes_with_last(last_byte),
    };
}

ScriptedEntropySource::Entry zero_entry()
{
    return {
        st::core::EntropyReadStatus::success,
        16U,
        {},
    };
}

} // namespace

int main()
{
    using namespace st::core;

    static_assert(ProjectIdAllocator::max_attempts == 16U);
    static_assert(TrackIdAllocator::max_attempts == 16U);

    {
        ScriptedEntropySource source{{success_entry(1U)}};
        VectorUniquenessScope<ProjectId> scope;
        const auto result = ProjectIdAllocator{}.allocate(source, scope);
        check(static_cast<bool>(result), "first valid non-colliding candidate succeeds");
        check(result.error == IdAllocationError::none, "successful allocation has no error");
        check(result.attempts == 1U, "successful first candidate reports one attempt");
        check(result.value->to_string() == "00000000000000000000000000000001", "candidate bytes map to canonical ID");
        check(source.calls() == 1U, "successful first candidate reads entropy once");
        check(source.all_destinations_exact(), "allocator always requests exactly 16 bytes");
        check(scope.calls() == 1U, "non-zero candidate is checked for collision");
    }

    {
        ScriptedEntropySource source{{{
            EntropyReadStatus::failure,
            0U,
            {},
        }}};
        VectorUniquenessScope<ProjectId> scope;
        const auto result = ProjectIdAllocator{}.allocate(source, scope);
        check(!result, "entropy provider failure rejects allocation");
        check(result.error == IdAllocationError::entropy_failure, "entropy provider failure is explicit");
        check(result.attempts == 1U, "entropy failure reports the failing attempt");
        check(source.calls() == 1U, "entropy failure is not retried as a candidate collision");
        check(scope.calls() == 0U, "failed entropy never reaches collision lookup");
    }

    {
        ScriptedEntropySource source{{{
            EntropyReadStatus::success,
            15U,
            bytes_with_last(2U),
        }}};
        VectorUniquenessScope<ProjectId> scope;
        const auto result = ProjectIdAllocator{}.allocate(source, scope);
        check(!result, "short entropy read rejects allocation");
        check(result.error == IdAllocationError::incomplete_entropy, "short entropy read is explicit");
        check(result.attempts == 1U, "short read reports one attempt");
        check(scope.calls() == 0U, "short read never reaches collision lookup");
    }

    {
        ScriptedEntropySource source{{{
            EntropyReadStatus::success,
            17U,
            bytes_with_last(2U),
        }}};
        VectorUniquenessScope<ProjectId> scope;
        const auto result = ProjectIdAllocator{}.allocate(source, scope);
        check(!result, "over-reported entropy read rejects allocation");
        check(result.error == IdAllocationError::incomplete_entropy, "over-reported read is explicit");
        check(scope.calls() == 0U, "invalid byte count never reaches collision lookup");
    }

    {
        ScriptedEntropySource source{{zero_entry(), success_entry(3U)}};
        VectorUniquenessScope<ProjectId> scope;
        const auto result = ProjectIdAllocator{}.allocate(source, scope);
        check(static_cast<bool>(result), "all-zero candidate is retried");
        check(result.attempts == 2U, "zero then success reports two attempts");
        check(result.value->to_string() == "00000000000000000000000000000003", "retry returns the later valid candidate");
        check(source.calls() == 2U, "zero candidate consumes one bounded retry");
        check(scope.calls() == 1U, "all-zero candidate is rejected before collision lookup");
    }

    {
        const auto existing = ProjectId::parse("00000000000000000000000000000004");
        check(static_cast<bool>(existing), "collision fixture parses");
        ScriptedEntropySource source{{success_entry(4U), success_entry(5U)}};
        VectorUniquenessScope<ProjectId> scope{{*existing.value}};
        const auto result = ProjectIdAllocator{}.allocate(source, scope);
        check(static_cast<bool>(result), "collision is retried until a unique candidate appears");
        check(result.attempts == 2U, "collision then success reports two attempts");
        check(result.value->to_string() == "00000000000000000000000000000005", "collision does not overwrite existing identity");
        check(scope.calls() == 2U, "both non-zero candidates are checked for collision");
    }

    {
        std::vector<ScriptedEntropySource::Entry> entries(
            ProjectIdAllocator::max_attempts,
            zero_entry());
        ScriptedEntropySource source{std::move(entries)};
        VectorUniquenessScope<ProjectId> scope;
        const auto result = ProjectIdAllocator{}.allocate(source, scope);
        check(!result, "sixteen all-zero candidates exhaust allocation");
        check(result.error == IdAllocationError::candidate_exhausted, "all-zero exhaustion is explicit");
        check(result.attempts == ProjectIdAllocator::max_attempts, "all-zero exhaustion is bounded at sixteen attempts");
        check(source.calls() == ProjectIdAllocator::max_attempts, "no seventeenth entropy read occurs");
        check(scope.calls() == 0U, "zero candidates never reach uniqueness scope");
    }

    {
        const auto existing = ProjectId::parse("00000000000000000000000000000006");
        check(static_cast<bool>(existing), "collision exhaustion fixture parses");
        std::vector<ScriptedEntropySource::Entry> entries(
            ProjectIdAllocator::max_attempts,
            success_entry(6U));
        ScriptedEntropySource source{std::move(entries)};
        VectorUniquenessScope<ProjectId> scope{{*existing.value}};
        const auto result = ProjectIdAllocator{}.allocate(source, scope);
        check(!result, "sixteen collisions exhaust allocation");
        check(result.error == IdAllocationError::candidate_exhausted, "collision exhaustion is explicit");
        check(result.attempts == ProjectIdAllocator::max_attempts, "collision exhaustion is bounded");
        check(source.calls() == ProjectIdAllocator::max_attempts, "collision exhaustion performs no extra entropy read");
        check(scope.calls() == ProjectIdAllocator::max_attempts, "every non-zero collision candidate is checked");
    }

    {
        ScriptedEntropySource source{{
            zero_entry(),
            {EntropyReadStatus::failure, 0U, {}},
        }};
        VectorUniquenessScope<ProjectId> scope;
        const auto result = ProjectIdAllocator{}.allocate(source, scope);
        check(!result, "provider failure after a rejected candidate fails closed");
        check(result.error == IdAllocationError::entropy_failure, "later provider failure remains explicit");
        check(result.attempts == 2U, "later provider failure reports the correct attempt");
        check(source.calls() == 2U, "provider failure terminates retry loop immediately");
    }

    {
        ScriptedEntropySource source{{success_entry(7U)}};
        VectorUniquenessScope<TrackId> scope;
        const auto result = TrackIdAllocator{}.allocate(source, scope);
        check(static_cast<bool>(result), "allocator is nominal-type specific for TrackId");
        check(result.value->to_string() == "00000000000000000000000000000007", "TrackId allocator preserves candidate payload");
    }

    return failures == 0 ? 0 : 1;
}
