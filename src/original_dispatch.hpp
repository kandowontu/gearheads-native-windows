#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace gh {

// The five far-function fields in each 0x7a-byte type record at DS:2354.
// All recovered targets are offsets in GEAR_EN code segment 10.
struct OriginalCallbackOffsets {
    std::uint16_t contact_filter = 0;
    std::uint16_t contact_effect = 0;
    std::uint16_t tick = 0;
    std::uint16_t expire = 0;
    std::uint16_t prepare_rectangle = 0;
};

struct OriginalTypeDispatch {
    int type = 0;
    std::string_view name;
    std::uint8_t flags = 0;
    std::uint8_t state_class = 0;
    OriginalCallbackOffsets callback;
};

inline constexpr std::uint16_t kFalseCallback = 0x1dcc;
inline constexpr std::uint16_t kNoOpCallback = 0x1dba;
inline constexpr std::uint16_t kTrueCallback = 0x1de4;
inline constexpr std::uint16_t kDefaultExpireCallback = 0x1dfc;
inline constexpr std::uint16_t kDefaultRectangleCallback = 0x0d10;

const std::array<OriginalTypeDispatch, 35>& original_type_dispatch();
const OriginalTypeDispatch& original_type_dispatch(int type);

}  // namespace gh
