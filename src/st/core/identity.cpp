#include "st/core/identity.hpp"

#include <algorithm>
#include <cstddef>

namespace st::core::detail {
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

[[nodiscard]] bool is_all_zero(const IdBytes& bytes) noexcept
{
    return std::all_of(bytes.begin(), bytes.end(), [](std::uint8_t value) {
        return value == 0;
    });
}

} // namespace

IdBytesParseResult parse_id_bytes(std::string_view text) noexcept
{
    if (text.size() != 32U) {
        return {{}, IdParseError::wrong_length, false};
    }

    IdBytes bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const auto high = decode_lower_hex(text[index * 2U]);
        const auto low = decode_lower_hex(text[index * 2U + 1U]);
        if (high < 0 || low < 0) {
            return {{}, IdParseError::non_canonical_character, false};
        }
        bytes[index] = static_cast<std::uint8_t>((high << 4) | low);
    }

    if (is_all_zero(bytes)) {
        return {{}, IdParseError::all_zero, false};
    }

    return {bytes, IdParseError::none, true};
}

std::string id_bytes_to_string(const IdBytes& bytes)
{
    static constexpr char kHex[] = "0123456789abcdef";

    std::string result(32U, '0');
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const auto value = bytes[index];
        result[index * 2U] = kHex[(value >> 4U) & 0x0FU];
        result[index * 2U + 1U] = kHex[value & 0x0FU];
    }
    return result;
}

} // namespace st::core::detail
