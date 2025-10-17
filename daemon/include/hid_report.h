#pragma once
#include <array>
#include <cstdint>
#include <vector>

struct HidReport8 {
    // Keyboard report: [mods][0x00][k0][k1][k2][k3][k4][k5]
    std::array<uint8_t,8> bytes{};
};

inline HidReport8 build_hid_report(const std::vector<uint8_t>& pressed_usages, uint8_t modifiers) {
    HidReport8 r{};
    r.bytes[0] = modifiers;
    r.bytes[1] = 0x00;
    for (size_t i = 0; i < 6 && i < pressed_usages.size(); ++i) {
        r.bytes[2 + i] = pressed_usages[i];
    }
    return r;
}
