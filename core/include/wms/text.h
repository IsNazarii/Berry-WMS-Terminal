#pragma once

#include <string>
#include <string_view>

namespace wms {

// Lowercases ASCII and Ukrainian Cyrillic letters in a UTF-8 string.
// Other characters are copied unchanged. Used for case-insensitive search.
[[nodiscard]] std::string foldCase(std::string_view utf8);

// Case-insensitive substring search (see foldCase).
[[nodiscard]] bool containsIgnoreCase(std::string_view haystack, std::string_view needle);

} // namespace wms
