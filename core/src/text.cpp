#include "wms/text.h"

namespace wms {
namespace {

char32_t lowerCyrillic(char32_t cp) noexcept {
    if (cp >= U'А' && cp <= U'Я') return cp + (U'а' - U'А');
    switch (cp) {
    case U'Є': return U'є';
    case U'І': return U'і';
    case U'Ї': return U'ї';
    case U'Ґ': return U'ґ';
    default: return cp;
    }
}

} // namespace

std::string foldCase(std::string_view utf8) {
    std::string out;
    out.reserve(utf8.size());

    for (std::size_t i = 0; i < utf8.size(); ++i) {
        const auto lead = static_cast<unsigned char>(utf8[i]);

        if (lead < 0x80) {
            out.push_back(lead >= 'A' && lead <= 'Z' ? static_cast<char>(lead + ('a' - 'A')) : utf8[i]);
            continue;
        }

        // All Ukrainian letters are 2-byte sequences (U+0400..U+04FF); anything else is copied as-is.
        const bool twoByteSequence = (lead & 0xE0) == 0xC0 && i + 1 < utf8.size() &&
                                     (static_cast<unsigned char>(utf8[i + 1]) & 0xC0) == 0x80;
        if (!twoByteSequence) {
            out.push_back(utf8[i]);
            continue;
        }

        const auto trail = static_cast<unsigned char>(utf8[i + 1]);
        const char32_t cp = lowerCyrillic(static_cast<char32_t>(((lead & 0x1F) << 6) | (trail & 0x3F)));
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        ++i;
    }
    return out;
}

bool containsIgnoreCase(std::string_view haystack, std::string_view needle) {
    return foldCase(haystack).find(foldCase(needle)) != std::string::npos;
}

} // namespace wms
