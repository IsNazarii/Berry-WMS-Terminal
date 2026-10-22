#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wms {

using SupplierId = std::int64_t;

class Supplier {
public:
    Supplier(SupplierId id, std::string fullName, std::string phone = {});

    [[nodiscard]] SupplierId id() const noexcept { return id_; }
    [[nodiscard]] const std::string& fullName() const noexcept { return fullName_; }
    [[nodiscard]] const std::string& phone() const noexcept { return phone_; }

    // Operator search: exact id ("42") or case-insensitive part of the name ("шевч").
    [[nodiscard]] bool matches(std::string_view query) const;

private:
    SupplierId id_;
    std::string fullName_;
    std::string phone_;
};

// A supplier the operator adds at the scale. The phone is optional; a name
// already taken (ignoring case and spaces) is refused, so one person never
// ends up with two debts — a namesake gets a distinction ("…, Лосятин").
[[nodiscard]] std::vector<std::string> validateNewSupplier(std::string_view fullName, std::string_view phone,
                                                           const std::vector<Supplier>& existing);

// The id for a supplier added on this terminal. Two terminals adding offline
// can collide; that is for the sync to settle.
[[nodiscard]] SupplierId nextSupplierId(const std::vector<Supplier>& existing) noexcept;

} // namespace wms
