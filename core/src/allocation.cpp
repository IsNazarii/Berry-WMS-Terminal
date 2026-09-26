#include "wms/allocation.h"

#include <format>
#include <stdexcept>
#include <utility>

namespace wms {

Allocation::Allocation(Uuid id, TimePoint createdAt, Uuid receiptId, TruckId truckId,
                       std::string cropCode, Cargo cargo,
                       std::optional<std::size_t> weighingIndex, SyncStatus syncStatus)
    : Transaction{id, createdAt, syncStatus}, receiptId_{receiptId}, truckId_{truckId},
      cropCode_{std::move(cropCode)}, cargo_{cargo}, weighingIndex_{weighingIndex} {}

Allocation Allocation::wholeWeighing(const Receipt& receipt, std::size_t weighingIndex,
                                     TruckId truckId) {
    const Weighing& weighing = receipt.weighings().at(weighingIndex);
    return Allocation{Uuid::generateV7(), Clock::now(), receipt.id(), truckId,
                      weighing.cropCode,  weighing.cargo(), weighingIndex};
}

Allocation Allocation::boxesOf(const Receipt& receipt, std::string_view cropCode, int boxes,
                               TruckId truckId) {
    return boxesOf(receipt, cropCode, boxes, truckId, receipt.cargoOf(cropCode));
}

Allocation Allocation::boxesOf(const Receipt& receipt, std::string_view cropCode, int boxes,
                               TruckId truckId, const Cargo& available) {
    if (receipt.cargoOf(cropCode).boxes <= 0) throw std::invalid_argument("Receipt has no boxes of the requested crop");

    const Weight weight =
        available.boxes > 0 ? grams(divideRounded(available.weight.raw() * boxes, available.boxes)) : Weight{};
    return Allocation{Uuid::generateV7(), Clock::now(), receipt.id(), truckId, std::string{cropCode},
                      {.boxes = boxes, .weight = weight}, std::nullopt};
}

std::vector<std::string> Allocation::validate() const {
    std::vector<std::string> errors;
    if (receiptId_.isNil()) errors.emplace_back("Не обрано чек");
    if (truckId_.isNil()) errors.emplace_back("Не обрано фуру");
    if (cropCode_.empty()) errors.emplace_back("Не обрано культуру");
    if (cargo_.boxes <= 0) errors.emplace_back("Кількість ящиків має бути більшою за нуль");
    if (!cargo_.weight.isPositive()) errors.emplace_back("Вага має бути більшою за нуль");
    return errors;
}

std::string Allocation::describe() const {
    return std::format("Розподіл {}: {} ящ., {} → фура {}{}", cropCode_, cargo_.boxes, formatWeight(cargo_.weight),
                       truckId_.toString(), weighingIndex_ ? std::format(" (зважування {})", *weighingIndex_ + 1) : "");
}

} // namespace wms
