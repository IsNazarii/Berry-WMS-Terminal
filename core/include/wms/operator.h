#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace wms {

using OperatorId = std::int64_t;

// A person working at the terminal. The PIN confirms who did something, it is
// not a security barrier: 4-6 digits are brute-forced instantly, so hashing
// would only look safe. It is still kept private and only checked, never shown.
class Operator {
public:
    // Throws std::invalid_argument unless the PIN is 4-6 digits.
    Operator(OperatorId id, std::string name, std::string pin);

    [[nodiscard]] OperatorId id() const noexcept { return id_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] bool checkPin(std::string_view pin) const noexcept { return pin == pin_; }
    // For the repository only; the UI asks checkPin() and never shows the PIN.
    [[nodiscard]] const std::string& pinForStorage() const noexcept { return pin_; }

private:
    OperatorId id_;
    std::string name_;
    std::string pin_;
};

} // namespace wms
