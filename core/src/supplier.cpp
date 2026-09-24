#include "wms/supplier.h"

#include "wms/text.h"

#include <charconv>
#include <stdexcept>
#include <utility>

namespace wms {

Supplier::Supplier(SupplierId id, std::string fullName, std::string priceCategory, std::string phone)
    : id_{id}, fullName_{std::move(fullName)}, priceCategory_{std::move(priceCategory)}, phone_{std::move(phone)} {
    if (id_ <= 0) throw std::invalid_argument("Supplier id must be positive");
    if (fullName_.empty()) throw std::invalid_argument("Supplier name must not be empty");
}

bool Supplier::matches(std::string_view query) const {
    if (query.empty()) return false;

    SupplierId parsedId = 0;
    const auto [end, error] = std::from_chars(query.data(), query.data() + query.size(), parsedId);
    if (error == std::errc{} && end == query.data() + query.size()) {
        return parsedId == id_;
    }
    return containsIgnoreCase(fullName_, query);
}

} // namespace wms
