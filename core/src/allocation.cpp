#include "wms/allocation.h"

#include <format>
#include <stdexcept>
#include <utility>

namespace wms {
namespace {

// Builds one visitor out of several lambdas, one per variant alternative.
template <class... Lambdas>
struct Overloaded : Lambdas... {
    using Lambdas::operator()...;
};

} // namespace

Allocation::Allocation(Uuid id, TimePoint createdAt, Uuid receiptId, Destination destination,
                       std::string cropCode, Cargo cargo,
                       std::optional<std::size_t> weighingIndex, SyncStatus syncStatus)
    : Transaction{id, createdAt, syncStatus}, receiptId_{receiptId}, destination_{destination},
      cropCode_{std::move(cropCode)}, cargo_{cargo}, weighingIndex_{weighingIndex} {}

std::vector<std::string> Allocation::validate() const {
    std::vector<std::string> errors;
    if (receiptId_.isNil()) errors.emplace_back("Не обрано чек");
    if (const auto* truckId = std::get_if<TruckId>(&destination_); truckId && truckId->isNil()) {
        errors.emplace_back("Не обрано фуру");
    }
    if (cropCode_.empty()) errors.emplace_back("Не обрано культуру");
    if (cargo_.boxes <= 0) errors.emplace_back("Кількість ящиків має бути більшою за нуль");
    if (!cargo_.weight.isPositive()) errors.emplace_back("Вага має бути більшою за нуль");
    return errors;
}

std::string Allocation::describe() const {
    const Overloaded where{
        [](const TruckId& truckId) { return "фура " + truckId.toString(); },
        [](ToWarehouse) { return std::string{"склад"}; },
    };
    return std::format("Розподіл {}: {} ящ., {} → {}{}", cropCode_, cargo_.boxes, formatWeight(cargo_.weight),
                       std::visit(where, destination_),
                       weighingIndex_ ? std::format(" (зважування {})", *weighingIndex_ + 1) : "");
}

} // namespace wms
