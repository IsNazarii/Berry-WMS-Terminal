#include "wms/allocation.h"

#include <format>
#include <stdexcept>
#include <utility>

namespace wms {
namespace {

// An operator id comes from a confirmed login, so a bad one is a bug.
std::optional<OperatorId> checkedOperator(std::optional<OperatorId> by) {
    if (by && *by <= 0) throw std::invalid_argument("Operator id must be positive");
    return by;
}

// Builds one visitor out of several lambdas, one per variant alternative.
template <class... Lambdas>
struct Overloaded : Lambdas... {
    using Lambdas::operator()...;
};

} // namespace

Allocation::Allocation(Uuid id, TimePoint createdAt, Uuid receiptId, Destination destination,
                       std::string cropCode, Cargo cargo,
                       std::optional<std::size_t> weighingIndex, SyncStatus syncStatus,
                       std::optional<OperatorId> allocatedBy)
    : Transaction{id, createdAt, syncStatus}, receiptId_{receiptId}, destination_{destination},
      cropCode_{std::move(cropCode)}, cargo_{cargo}, weighingIndex_{weighingIndex},
      allocatedBy_{checkedOperator(allocatedBy)} {}

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

AllocationReversal::AllocationReversal(Uuid id, TimePoint createdAt, Uuid allocationId, SyncStatus syncStatus,
                                       std::optional<OperatorId> reversedBy)
    : Transaction{id, createdAt, syncStatus}, allocationId_{allocationId}, reversedBy_{checkedOperator(reversedBy)} {}

AllocationReversal AllocationReversal::of(const Allocation& allocation, std::optional<OperatorId> by) {
    return AllocationReversal{Uuid::generateV7(), Clock::now(), allocation.id(), SyncStatus::Pending, by};
}

std::vector<std::string> AllocationReversal::validate() const {
    if (allocationId_.isNil()) return {"Не обрано розподіл"};
    return {};
}

std::string AllocationReversal::describe() const {
    return std::format("Скасування розподілу {}", allocationId_.toString());
}

} // namespace wms
