#include "wms/crop.h"

#include "wms/text.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace wms {

Crop::Crop(std::string code, std::string name) : code_{std::move(code)}, name_{std::move(name)} {
    if (code_.empty()) throw std::invalid_argument("Crop code must not be empty");
    if (name_.empty()) throw std::invalid_argument("Crop name must not be empty");
}

std::vector<std::string> validateNewCrop(std::string_view name, const std::vector<Crop>& existing) {
    const std::string code = cropCodeFor(name);
    if (code.empty()) return {"Вкажіть назву культури"};

    // The code is checked too: an older crop may own it under another name.
    const auto same = std::ranges::find_if(existing, [&code](const Crop& c) {
        return c.code() == code || cropCodeFor(c.name()) == code;
    });
    if (same != existing.end()) return {"Культура «" + same->name() + "» уже є"};
    return {};
}

std::string cropCodeFor(std::string_view name) {
    return foldCase(squeezeSpaces(name));
}

} // namespace wms
