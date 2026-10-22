#include "wms/crop_keys.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace wms {

CropKeys::CropKeys(std::size_t size, std::vector<std::string> keys, std::vector<std::string> recent)
    : size_{size}, keys_{std::move(keys)}, recent_{std::move(recent)} {
    if (size_ == 0) throw std::invalid_argument("CropKeys needs at least one key");
}

void CropKeys::fill(const std::vector<std::string>& catalogue) {
    const auto gone = [&catalogue](const std::string& code) {
        return std::ranges::find(catalogue, code) == catalogue.end();
    };
    std::erase_if(keys_, gone);
    std::erase_if(recent_, gone);
    for (const std::string& code : catalogue) {
        if (keys_.size() >= size_) break;
        if (std::ranges::find(keys_, code) == keys_.end()) keys_.push_back(code);
    }
}

void CropKeys::use(std::string_view code) {
    std::erase(recent_, code);
    recent_.insert(recent_.begin(), std::string{code});

    if (std::ranges::find(keys_, code) != keys_.end()) return;
    if (keys_.size() < size_) {
        keys_.emplace_back(code);
        return;
    }
    // A key never used ranks past every used one; ">=" makes the rightmost lose a tie.
    const auto age = [this](const std::string& key) {
        return std::ranges::find(recent_, key) - recent_.begin();
    };
    auto oldest = keys_.begin();
    for (auto key = keys_.begin(); key != keys_.end(); ++key) {
        if (age(*key) >= age(*oldest)) oldest = key;
    }
    *oldest = std::string{code};
}

} // namespace wms
