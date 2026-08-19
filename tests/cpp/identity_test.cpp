#include "st/core/identity.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>

namespace {

int failures = 0;

void check(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

} // namespace

int main()
{
    using namespace st::core;

    static_assert(!std::is_same_v<ProjectId, TrackId>);
    static_assert(!std::is_same_v<TrackId, ClipId>);
    static_assert(!std::is_convertible_v<ProjectId, TrackId>);
    static_assert(!std::is_convertible_v<TrackId, ProjectId>);

    constexpr std::string_view canonical = "00112233445566778899aabbccddeeff";
    const auto project = ProjectId::parse(canonical);
    check(static_cast<bool>(project), "canonical ProjectId parses");
    check(project.error == IdParseError::none, "canonical ProjectId has no parse error");
    check(project.value->to_string() == canonical, "canonical ProjectId round-trips exactly");

    const auto track_same_payload = TrackId::parse(canonical);
    check(static_cast<bool>(track_same_payload), "same payload may exist in a distinct nominal namespace");
    check(track_same_payload.value->to_string() == canonical, "nominal type does not rewrite payload");

    const auto short_id = ProjectId::parse("00112233445566778899aabbccddee");
    check(!short_id, "short identifier is rejected");
    check(short_id.error == IdParseError::wrong_length, "short identifier reports wrong length");

    const auto long_id = ProjectId::parse("00112233445566778899aabbccddeeff00");
    check(!long_id, "long identifier is rejected");
    check(long_id.error == IdParseError::wrong_length, "long identifier reports wrong length");

    const auto uppercase = ProjectId::parse("00112233445566778899AABBCCDDEEFF");
    check(!uppercase, "uppercase hex is rejected rather than normalized");
    check(uppercase.error == IdParseError::non_canonical_character, "uppercase reports canonical-character error");

    const auto separated = ProjectId::parse("00112233-4455-6677-8899-aabbccddeeff");
    check(!separated, "separator form is rejected");
    check(separated.error == IdParseError::wrong_length, "separator form fails fixed-length boundary");

    const auto invalid_hex = ProjectId::parse("00112233445566778899aabbccddeezz");
    check(!invalid_hex, "non-hex characters are rejected");
    check(invalid_hex.error == IdParseError::non_canonical_character, "invalid hex reports canonical-character error");

    const auto all_zero = ProjectId::parse("00000000000000000000000000000000");
    check(!all_zero, "all-zero identifier is reserved and rejected");
    check(all_zero.error == IdParseError::all_zero, "all-zero identifier reports all-zero error");

    const auto whitespace = ProjectId::parse(" 00112233445566778899aabbccddeeff");
    check(!whitespace, "leading whitespace is not silently trimmed");

    const auto maximum_payload = ProjectId::parse("ffffffffffffffffffffffffffffffff");
    check(static_cast<bool>(maximum_payload), "all-ones payload is valid");

    std::string oversized(100000U, 'a');
    const auto oversized_result = ProjectId::parse(oversized);
    check(!oversized_result, "oversized identifier is rejected");
    check(oversized_result.error == IdParseError::wrong_length, "oversized identifier fails at the length boundary");

    ProjectId::Bytes zero_bytes{};
    check(!ProjectId::from_candidate_bytes(zero_bytes).has_value(), "all-zero allocation candidate is rejected");

    ProjectId::Bytes project_a_bytes{};
    project_a_bytes[15] = 1U;
    ProjectId::Bytes project_b_bytes{};
    project_b_bytes[15] = 2U;
    TrackId::Bytes local_bytes{};
    local_bytes[15] = 9U;

    const auto project_a = ProjectId::from_candidate_bytes(project_a_bytes);
    const auto project_b = ProjectId::from_candidate_bytes(project_b_bytes);
    const auto local_track = TrackId::from_candidate_bytes(local_bytes);
    check(project_a.has_value() && project_b.has_value() && local_track.has_value(), "non-zero allocation candidates are accepted");

    if (project_a && project_b && local_track) {
        const ScopedTrackId scoped_a{*project_a, *local_track};
        const ScopedTrackId scoped_a_copy{*project_a, *local_track};
        const ScopedTrackId scoped_b{*project_b, *local_track};
        check(scoped_a == scoped_a_copy, "same project plus same local ID is equal");
        check(!(scoped_a == scoped_b), "same local ID under different ProjectId is not equal");
    }

    for (int iteration = 0; iteration < 1000; ++iteration) {
        const auto repeated = ProjectId::parse(canonical);
        check(static_cast<bool>(repeated), "repeated canonical parse remains valid");
        if (repeated) {
            check(repeated.value->to_string() == canonical, "repeated parse is deterministic");
        }
    }

    return failures == 0 ? 0 : 1;
}
