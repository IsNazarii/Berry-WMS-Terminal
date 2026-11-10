#pragma once

#include <string>
#include <vector>

namespace wms {

// A QR symbol as a square of modules; how to draw it is up to the caller.
// Wraps nayuki qrcodegen, so the rest of the program does not depend on its types.
class QrMatrix {
public:
    // Encodes UTF-8 text at error correction level M, which survives about 15% damage:
    // enough for a crease or a smudge on paper, without growing the symbol as much as Q or H.
    // Throws qrcodegen::data_too_long for text that does not fit any QR version.
    explicit QrMatrix(const std::string& text);

    // Modules per side.
    [[nodiscard]] int size() const noexcept { return size_; }
    // x and y must be within [0, size); true = a dark module.
    [[nodiscard]] bool dark(int x, int y) const;

private:
    int size_;
    std::vector<char> modules_;
};

} // namespace wms
