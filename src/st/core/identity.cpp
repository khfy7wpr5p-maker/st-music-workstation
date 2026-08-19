#include "st/core/identity.hpp"

#include <algorithm>
#include <cstddef>

namespace st::core {
namespace {

[[nodiscard]] constexpr int decode_lower_hex(char character) noexcept
{
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return 10 + (character - 'a');
    }
    return -1;
}

[[nodiscard]] bool is_all_zero(const OpaqueId128::Bytes& bytes) noexcept
{
    return std::all_of(bytes.begin(), bytes.end(), [](std::uint8_t value) {
        return value == 0;
    });
}

} // namespace

ParseResult<OpaqueId128> OpaqueId128::parse(std::string_view text) noexcept
{
    if (text.size() != 32U) {
        return {std::nullopt, IdParseError::wrong_length};
    }

    Bytes bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const auto high = decode_lower_hex(text[index * 2U]);
        const auto low = decode_lower_hex(text[index * 2U + 1U]);
        if (high < 0 || low < 0) {
            return {std::nullopt, IdParseError::non_canonical_character};
        }
        bytes[index] = static_cast<std::uint8_t>((high << 4) | low);
    }

    if (is_all_zero(bytes)) {
        return {std::nullopt, IdParseError::all_zero};
    }

    return {OpaqueId128{bytes}, IdParseError::none};
}

std::optional<OpaqueId128> OpaqueId128::from_candidate_bytes(Bytes bytes) noexcept
{
    if (is_all_zero(bytes)) {
        return std::nullopt;
    }
    return OpaqueId128{bytes};
}

std::string OpaqueId128::to_string() const
{
    static constexpr char kHex[] = "0123456789abcdef";

    std::string result(32U, '0');
    for (std::size_t index = 0; index < bytes_.size(); ++index) {
        const auto value = bytes_[index];
        result[index * 2U] = kHex[(value >> 4U) & 0x0FU];
        result[index * 2U + 1U] = kHex[value & 0x0FU];
    }
    return result;
}

} // namespace st::core
