#include "wms/crop.h"

#include "wms/journal.h"
#include "wms/text.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace wms {

Crop::Crop(std::string code, std::string name, bool archived)
    : code_{std::move(code)}, name_{std::move(name)}, archived_{archived} {
    if (code_.empty()) throw std::invalid_argument("Crop code must not be empty");
    if (name_.empty()) throw std::invalid_argument("Crop name must not be empty");
}

std::vector<std::string> validateNewCrop(std::string_view name, const std::vector<Crop>& existing) {
    const std::string code = cropCodeFor(name);
    if (code.empty()) return {"Вкажіть назву культури"};

    const Crop* same = findCropNamed(name, existing);
    if (same != nullptr && !same->archived()) return {"Культура «" + same->name() + "» уже є"};
    return {};
}

std::string cropCodeFor(std::string_view name) {
    return foldCase(squeezeSpaces(name));
}

const Crop* findCropNamed(std::string_view name, const std::vector<Crop>& existing) {
    const std::string code = cropCodeFor(name);
    // The code is checked too: an older crop may own it under another name.
    const auto same = std::ranges::find_if(existing, [&code](const Crop& c) {
        return c.code() == code || cropCodeFor(c.name()) == code;
    });
    return same != existing.end() ? &*same : nullptr;
}

std::vector<std::string> validateRemoveCrop(std::string_view code, const Journal& journal) {
    if (const Cargo held = journal.stockOf(code); held.boxes != 0) {
        return {"На складі ще " + std::to_string(held.boxes) + " ящ. цієї культури — спершу відвантажте їх"};
    }
    return {};
}

} // namespace wms
