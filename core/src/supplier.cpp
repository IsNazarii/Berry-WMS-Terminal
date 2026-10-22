#include "wms/supplier.h"

#include "wms/text.h"

#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <utility>

namespace wms {

Supplier::Supplier(SupplierId id, std::string fullName, std::string phone)
    : id_{id}, fullName_{std::move(fullName)}, phone_{std::move(phone)} {
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

std::vector<std::string> validateNewSupplier(std::string_view fullName, std::string_view phone,
                                             const std::vector<Supplier>& existing) {
    std::vector<std::string> errors;

    const std::string name = squeezeSpaces(fullName);
    if (name.empty()) {
        errors.emplace_back("Вкажіть прізвище та ім'я постачальника");
    } else {
        const std::string key = foldCase(name);
        const auto same = std::ranges::find_if(
            existing, [&key](const Supplier& s) { return foldCase(squeezeSpaces(s.fullName())) == key; });
        if (same != existing.end()) {
            errors.push_back("Постачальник «" + same->fullName() + "» уже є (#" + std::to_string(same->id()) +
                             "); якщо це інша людина — допишіть, чим відрізняється (село, по батькові)");
        }
    }

    // Typed by hand in any of the usual forms: +38 (067) 111-22-33.
    if (!phone.empty()) {
        const bool phoneCharsOnly = std::ranges::all_of(phone, [](char c) {
            return (c >= '0' && c <= '9') || c == '+' || c == ' ' || c == '-' || c == '(' || c == ')';
        });
        const auto digits = std::ranges::count_if(phone, [](char c) { return c >= '0' && c <= '9'; });
        if (!phoneCharsOnly || digits < 10) {
            errors.emplace_back("Телефон: щонайменше 10 цифр; можна +, пробіли, дужки й дефіс");
        }
    }
    return errors;
}

SupplierId nextSupplierId(const std::vector<Supplier>& existing) noexcept {
    SupplierId largest = 0;
    for (const Supplier& s : existing) largest = std::max(largest, s.id());
    return largest + 1;
}

} // namespace wms
