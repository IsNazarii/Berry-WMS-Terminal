#pragma once

#include <string>
#include <string_view>

namespace wms {

// Lowercases ASCII and Ukrainian Cyrillic letters in a UTF-8 string.
// Other characters are copied unchanged. Used for case-insensitive search.
[[nodiscard]] std::string foldCase(std::string_view utf8);

// Trims the ends and turns every run of whitespace into one space: names typed
// by hand ("  Петренко   Іван") are stored and compared in one form.
[[nodiscard]] std::string squeezeSpaces(std::string_view text);

// Case-insensitive substring search (see foldCase).
[[nodiscard]] bool containsIgnoreCase(std::string_view haystack, std::string_view needle);

} // namespace wms
