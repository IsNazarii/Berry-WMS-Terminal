#pragma once

#include <array>
#include <chrono>
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace wms {

// RFC 9562 UUID. New ids are version 7: the first 48 bits are a Unix timestamp
// in milliseconds, so ids sort by creation time (good for DB index locality
// and for ordering the offline sync queue).
class Uuid {
public:
    using Bytes = std::array<std::uint8_t, 16>;

    constexpr Uuid() noexcept = default; // nil UUID
    explicit constexpr Uuid(const Bytes& bytes) noexcept : bytes_{bytes} {}

    [[nodiscard]] static Uuid generateV7();
    // Deterministic variant for tests: only the low 12 bits of randA are used.
    [[nodiscard]] static Uuid generateV7(std::chrono::system_clock::time_point at, std::uint64_t randA,
                                         std::uint64_t randB) noexcept;

    // Accepts the canonical 8-4-4-4-12 hex form, case-insensitive.
    [[nodiscard]] static std::optional<Uuid> parse(std::string_view text);

    [[nodiscard]] std::string toString() const;
    [[nodiscard]] int version() const noexcept { return bytes_[6] >> 4; }
    [[nodiscard]] bool isNil() const noexcept { return *this == Uuid{}; }
    // Only meaningful for version 7.
    [[nodiscard]] std::chrono::system_clock::time_point timestamp() const noexcept;
    [[nodiscard]] const Bytes& bytes() const noexcept { return bytes_; }

    auto operator<=>(const Uuid&) const noexcept = default;

private:
    Bytes bytes_{};
};

} // namespace wms
