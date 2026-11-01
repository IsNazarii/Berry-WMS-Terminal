#include "wms/ie04.h"

#include <algorithm>
#include <cstdint>

namespace wms {

namespace {

constexpr char kStart = '<';
constexpr char kEnd = '>';

// Status byte: bits 0-1 = digits after the decimal point, then the lamps.
constexpr unsigned kPointMask = 0x03;
constexpr unsigned kStable = 0x04;
constexpr unsigned kZero = 0x08;
constexpr unsigned kTare = 0x10;
constexpr unsigned kMinus = 0x20;

bool isDigit(char c) noexcept { return c >= '0' && c <= '9'; }

} // namespace

std::optional<ScaleReading> parseIe04Frame(std::string_view frame) {
    if (frame.size() != kIe04FrameSize || frame.front() != kStart || frame.back() != kEnd) return std::nullopt;

    // Right-aligned: leading spaces, an optional '-', then at least one digit.
    std::string_view display = frame.substr(2, 6);
    display.remove_prefix(std::min(display.find_first_not_of(' '), display.size()));
    const bool minusShown = !display.empty() && display.front() == '-';
    if (minusShown) display.remove_prefix(1);
    if (display.empty()) return std::nullopt;

    std::int64_t digits = 0;
    for (const char c : display) {
        if (!isDigit(c)) return std::nullopt;
        digits = digits * 10 + (c - '0');
    }

    const unsigned status = static_cast<unsigned char>(frame[8]);
    std::int64_t gramsPerUnit = 1'000;
    for (unsigned point = status & kPointMask; point > 0; --point) gramsPerUnit /= 10;

    // Near zero the '-' and the minus bit may disagree for one frame; either means negative.
    const bool negative = minusShown || (status & kMinus) != 0;
    return ScaleReading{
        .weight = grams(negative ? -digits * gramsPerUnit : digits * gramsPerUnit),
        .stable = (status & kStable) != 0,
        .zero = (status & kZero) != 0,
        .tare = (status & kTare) != 0,
    };
}

std::optional<ScaleReading> Ie04FrameReader::push(std::string_view chunk) {
    pending_.append(chunk);

    // Latest complete frame: a '>' with a '<' nine bytes before it. The status byte
    // may itself equal '>', so a '>' without a matching '<' is skipped.
    std::optional<ScaleReading> latest;
    std::size_t consumed = 0;
    for (std::size_t end = pending_.rfind(kEnd); end != std::string::npos && end >= kIe04FrameSize - 1;
         end = pending_.rfind(kEnd, end - 1)) {
        const std::size_t start = end + 1 - kIe04FrameSize;
        if (pending_[start] == kStart) {
            latest = parseIe04Frame(std::string_view{pending_}.substr(start, kIe04FrameSize));
            consumed = end + 1;
            break;
        }
    }

    // Keep only what may still be the beginning of the next frame.
    if (consumed > 0) pending_.erase(0, consumed);
    if (pending_.size() >= kIe04FrameSize) pending_.erase(0, pending_.size() - (kIe04FrameSize - 1));
    return latest;
}

} // namespace wms
