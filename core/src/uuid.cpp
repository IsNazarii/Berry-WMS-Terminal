#include "wms/uuid.h"

#include <random>

namespace wms {
namespace {

std::mt19937_64& randomEngine() {
    thread_local std::mt19937_64 engine{[] {
        std::random_device device;
        return (std::uint64_t{device()} << 32) ^ device();
    }()};
    return engine;
}

std::optional<std::uint8_t> hexValue(char c) noexcept {
    if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return static_cast<std::uint8_t>(c - 'A' + 10);
    return std::nullopt;
}

bool isDashPosition(std::size_t i) noexcept { return i == 8 || i == 13 || i == 18 || i == 23; }

} // namespace

Uuid Uuid::generateV7() {
    auto& engine = randomEngine();
    const std::uint64_t randA = engine();
    const std::uint64_t randB = engine();
    return generateV7(std::chrono::system_clock::now(), randA, randB);
}

Uuid Uuid::generateV7(std::chrono::system_clock::time_point at, std::uint64_t randA, std::uint64_t randB) noexcept {
    const auto ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(at.time_since_epoch()).count());

    Bytes b{};
    for (std::size_t i = 0; i < 6; ++i) {
        b[i] = static_cast<std::uint8_t>(ms >> (8 * (5 - i)));
    }
    b[6] = static_cast<std::uint8_t>(0x70 | ((randA >> 8) & 0x0F)); // version 7
    b[7] = static_cast<std::uint8_t>(randA);
    for (std::size_t i = 0; i < 8; ++i) {
        b[8 + i] = static_cast<std::uint8_t>(randB >> (8 * (7 - i)));
    }
    b[8] = static_cast<std::uint8_t>(0x80 | (b[8] & 0x3F)); // variant 10xx
    return Uuid{b};
}

std::optional<Uuid> Uuid::parse(std::string_view text) {
    if (text.size() != 36) return std::nullopt;

    Bytes b{};
    std::size_t byteIndex = 0;
    for (std::size_t i = 0; i < text.size();) {
        if (isDashPosition(i)) {
            if (text[i] != '-') return std::nullopt;
            ++i;
            continue;
        }
        const auto high = hexValue(text[i]);
        const auto low = hexValue(text[i + 1]);
        if (!high || !low) return std::nullopt;
        b[byteIndex++] = static_cast<std::uint8_t>((*high << 4) | *low);
        i += 2;
    }
    return Uuid{b};
}

std::string Uuid::toString() const {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(36);
    for (std::size_t i = 0; i < bytes_.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) out.push_back('-');
        out.push_back(digits[bytes_[i] >> 4]);
        out.push_back(digits[bytes_[i] & 0x0F]);
    }
    return out;
}

std::chrono::system_clock::time_point Uuid::timestamp() const noexcept {
    std::uint64_t ms = 0;
    for (std::size_t i = 0; i < 6; ++i) {
        ms = (ms << 8) | bytes_[i];
    }
    return std::chrono::system_clock::time_point{std::chrono::milliseconds{ms}};
}

} // namespace wms
