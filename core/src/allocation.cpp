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

Allocation Allocation::wholeWeighing(const Receipt& receipt, std::size_t weighingIndex,
                                     Destination destination) {
    const Weighing& weighing = receipt.weighings().at(weighingIndex);
    return Allocation{Uuid::generateV7(), Clock::now(), receipt.id(), destination,
                      weighing.cropCode,  weighing.cargo(), weighingIndex};
}

Allocation Allocation::boxesOf(const Receipt& receipt, std::string_view cropCode, int boxes,
                               Destination destination) {
    return boxesOf(receipt, cropCode, boxes, destination, receipt.cargoOf(cropCode));
}

Allocation Allocation::boxesOf(const Receipt& receipt, std::string_view cropCode, int boxes,
                               Destination destination, const Cargo& available) {
    if (receipt.cargoOf(cropCode).boxes <= 0) throw std::invalid_argument("Receipt has no boxes of the requested crop");

    const Weight weight =
        available.boxes > 0 ? grams(divideRounded(available.weight.raw() * boxes, available.boxes)) : Weight{};
    return Allocation{Uuid::generateV7(), Clock::now(), receipt.id(), destination, std::string{cropCode},
                      {.boxes = boxes, .weight = weight}, std::nullopt};
}

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
