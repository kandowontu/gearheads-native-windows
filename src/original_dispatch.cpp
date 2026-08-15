#include "original_dispatch.hpp"

#include <stdexcept>

namespace gh {
namespace {

constexpr OriginalCallbackOffsets defaults() {
    return {
        kFalseCallback,
        kFalseCallback,
        kNoOpCallback,
        kNoOpCallback,
        kDefaultRectangleCallback,
    };
}

constexpr OriginalCallbackOffsets ordinary_toy() {
    OriginalCallbackOffsets result = defaults();
    result.contact_effect = kTrueCallback;
    result.tick = 0x2638;
    result.expire = kDefaultExpireCallback;
    return result;
}

constexpr OriginalTypeDispatch entry(
    int type,
    std::string_view name,
    std::uint8_t flags,
    std::uint8_t state_class,
    OriginalCallbackOffsets callback
) {
    return {type, name, flags, state_class, callback};
}

constexpr std::array<OriginalTypeDispatch, 35> kDispatch = [] {
    std::array<OriginalTypeDispatch, 35> result{};
    result[0] = entry(0, "art", 0x04, 2, defaults());
    result[1] = entry(1, "arrow", 0x04, 2, defaults());
    result[2] = entry(2, "boxer", 0x04, 2, defaults());
    result[3] = entry(3, "gauge", 0x04, 2, defaults());
    result[4] = entry(4, "digit", 0x04, 2, defaults());

    for (int type = 5; type <= 14; ++type) {
        OriginalCallbackOffsets callback = defaults();
        callback.tick = 0x20c2;
        result[static_cast<std::size_t>(type)] =
            entry(type, "", type == 14 ? 0x08 : 0x00, 2, callback);
    }
    result[5].name = "puup";
    result[5].state_class = 0;
    result[5].callback.contact_filter = 0x3bf0;
    result[5].callback.tick = 0x260a;
    result[5].callback.expire = kDefaultExpireCallback;
    result[6].name = "dtrd";
    result[6].state_class = 0;
    result[6].callback.contact_filter = 0x3baa;
    result[7].name = "utrd";
    result[7].state_class = 0;
    result[7].callback.contact_filter = 0x3b64;
    result[8].name = "htrd";
    result[8].state_class = 0;
    result[8].callback.contact_filter = 0x3b08;
    result[9].name = "crak";
    result[9].state_class = 0;
    result[9].callback.contact_filter = 0x36fe;
    result[9].callback.tick = 0x3670;
    result[10].name = "tele";
    result[10].state_class = 0;
    result[10].callback.contact_filter = 0x3898;
    result[11].name = "mudd";
    result[11].state_class = 0;
    result[11].callback.contact_filter = 0x3824;
    result[12].name = "oily";
    // The initializer writes DS:28b5 twice, leaving Oily's +23 byte at 2.
    result[12].callback.contact_filter = 0x39ea;
    result[13].name = "glue";
    result[13].state_class = 0;
    result[13].callback.contact_filter = 0x3824;
    result[14].name = "rock";
    result[14].callback.contact_filter = 0x3a64;
    result[14].callback.tick = 0x2040;

    for (int type = 15; type <= 34; ++type) {
        result[static_cast<std::size_t>(type)] = entry(
            type,
            "",
            static_cast<std::uint8_t>(type <= 28 ? 0x03 : 0x02),
            2,
            ordinary_toy()
        );
    }
    result[15].name = "roach";
    result[15].state_class = 3;
    result[15].callback.contact_effect = 0x276a;
    result[15].callback.tick = 0x27d8;
    result[16].name = "bomby";
    result[16].callback.tick = 0x2832;
    result[16].callback.expire = 0x1e8c;
    result[17].name = "cluck";
    result[17].state_class = 1;
    result[17].callback.tick = 0x2870;
    result[18].name = "zappa";
    result[18].state_class = 3;
    result[18].callback.contact_filter = 0x2ac4;
    result[19].name = "kanga";
    result[19].state_class = 1;
    result[19].callback.contact_effect = 0x2bca;
    result[20].name = "bigal";
    result[21].name = "destr";
    result[21].state_class = 3;
    result[21].callback.contact_filter = 0x2cc8;
    result[21].callback.tick = 0x2df6;
    result[22].name = "stick";
    result[22].state_class = 1;
    result[22].callback.tick = 0x2ea6;
    result[23].name = "goril";
    result[23].callback.tick = 0x2fd8;
    result[24].name = "skull";
    result[24].callback.contact_filter = 0x3198;
    result[24].callback.tick = 0x329e;
    result[25].name = "magnt";
    result[25].state_class = 1;
    result[25].callback.contact_filter = 0x3366;
    result[25].callback.contact_effect = 0x32ba;
    result[26].name = "handy";
    result[26].state_class = 1;
    result[26].callback.contact_filter = 0x33b2;
    result[27].name = "small";
    result[27].state_class = 1;
    result[27].callback.contact_filter = 0x3506;
    result[27].callback.tick = 0x3560;
    result[28].name = "roket";
    result[28].state_class = 3;
    result[28].callback.tick = 0x3600;
    result[29].name = "buggy";
    result[29].callback.contact_filter = 0x3e60;
    result[29].callback.tick = 0x4066;
    result[30].name = "block";
    result[30].callback.contact_filter = 0x3aea;
    result[30].callback.tick = 0x2040;
    result[31].name = "wall1";
    result[31].flags = 0x12;
    result[31].callback.tick = 0x43c8;
    result[32].name = "wall2";
    result[32].flags = 0x12;
    result[32].callback.tick = 0x43c8;
    result[33].name = "wall3";
    result[33].flags = 0x12;
    result[33].callback.tick = 0x43c8;
    result[34].name = "wall4";
    result[34].flags = 0x12;
    result[34].callback.tick = 0x43c8;
    return result;
}();

static_assert(kDispatch.size() == 35);

}  // namespace

const std::array<OriginalTypeDispatch, 35>& original_type_dispatch() {
    return kDispatch;
}

const OriginalTypeDispatch& original_type_dispatch(int type) {
    if (type < 0 || type >= static_cast<int>(kDispatch.size())) {
        throw std::out_of_range("Original Gearheads type is outside 0..34");
    }
    return kDispatch[static_cast<std::size_t>(type)];
}

}  // namespace gh
