#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace wms {

using SupplierId = std::int64_t;

class Supplier {
public:
    Supplier(SupplierId id, std::string fullName, std::string priceCategory, std::string phone = {});

    [[nodiscard]] SupplierId id() const noexcept { return id_; }
    [[nodiscard]] const std::string& fullName() const noexcept { return fullName_; }
    [[nodiscard]] const std::string& priceCategory() const noexcept { return priceCategory_; }
    [[nodiscard]] const std::string& phone() const noexcept { return phone_; }

    // Operator search: exact id ("42") or case-insensitive part of the name ("шевч").
    [[nodiscard]] bool matches(std::string_view query) const;

private:
    SupplierId id_;
    std::string fullName_;
    std::string priceCategory_;
    std::string phone_;
};

} // namespace wms
