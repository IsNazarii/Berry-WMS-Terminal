#include "wms/operator.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace wms {
namespace {

bool isValidPin(std::string_view pin) {
    return pin.size() >= 4 && pin.size() <= 6 && std::ranges::all_of(pin, [](char c) { return c >= '0' && c <= '9'; });
}

} // namespace

Operator::Operator(OperatorId id, std::string name, std::string pin)
    : id_{id}, name_{std::move(name)}, pin_{std::move(pin)} {
    if (id_ <= 0) throw std::invalid_argument("Operator id must be positive");
    if (name_.empty()) throw std::invalid_argument("Operator name must not be empty");
    if (!isValidPin(pin_)) throw std::invalid_argument("PIN must be 4-6 digits");
}

} // namespace wms
