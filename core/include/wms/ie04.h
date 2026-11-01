#pragma once

#include "wms/scale.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace wms {

// IE-04 weight indicator ("Promprylad"), protocol 1: the scale streams ~10 frames
// per second on its own. Frame: '<' address, six display characters, status, '>'.
// See docs/hardware/scale-ie04.md.
inline constexpr std::size_t kIe04FrameSize = 10;

// One frame, exactly kIe04FrameSize bytes. std::nullopt when the frame is malformed
// or the display does not show a number (error codes, blank display).
[[nodiscard]] std::optional<ScaleReading> parseIe04Frame(std::string_view frame);

// Turns arbitrary chunks read from the serial port into readings. A chunk may start
// mid-frame and end mid-frame; the cut-off tail is kept for the next chunk.
class Ie04FrameReader {
public:
    // Reading from the latest complete frame in the data so far, or std::nullopt
    // when there is none or it is not a number. Older frames are dropped: only the
    // current state of the scale matters.
    [[nodiscard]] std::optional<ScaleReading> push(std::string_view chunk);

    [[nodiscard]] std::size_t buffered() const noexcept { return pending_.size(); }

private:
    std::string pending_;
};

} // namespace wms
