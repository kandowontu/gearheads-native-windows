#include "original_dispatch.hpp"

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int main() {
    try {
        const auto& dispatch = gh::original_type_dispatch();
        require(dispatch.size() == 35, "wrong type-record count");
        for (std::size_t index = 0; index < dispatch.size(); ++index) {
            require(dispatch[index].type == static_cast<int>(index), "noncontiguous type number");
            require(!dispatch[index].name.empty(), "missing recovered type name");
            require(dispatch[index].callback.prepare_rectangle == 0x0d10,
                    "unexpected rectangle callback");
        }
        require(dispatch[12].state_class == 2, "original duplicate Oily write was not preserved");
        require(dispatch[15].callback.contact_effect == 0x276a, "wrong Ziggy effect");
        require(dispatch[16].callback.expire == 0x1e8c, "wrong Bomby expiry callback");
        require(dispatch[21].callback.contact_filter == 0x2cc8, "wrong Disasteroid filter");
        require(dispatch[29].flags == 0x02, "bug should remain dynamically collidable");
        require(dispatch[30].flags == 0x02, "block should remain dynamically collidable");
        require(dispatch[31].flags == 0x12 && dispatch[34].flags == 0x12,
                "wall motion-blocking flags missing");
        bool rejected = false;
        try {
            (void)gh::original_type_dispatch(35);
        } catch (const std::out_of_range&) {
            rejected = true;
        }
        require(rejected, "invalid original type accepted");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
