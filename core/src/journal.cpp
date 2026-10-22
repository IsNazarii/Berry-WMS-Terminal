#include "wms/journal.h"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

namespace wms {

bool Journal::registerId(const Uuid& id) { return ids_.insert(id).second; }

bool Journal::add(Receipt receipt) {
    if (!registerId(receipt.id())) return false;
    receipts_.push_back(std::move(receipt));
    return true;
}

bool Journal::add(Payment payment) {
    if (!registerId(payment.id())) return false;
    payments_.push_back(std::move(payment));
    return true;
}

bool Journal::add(Allocation allocation) {
    if (!registerId(allocation.id())) return false;
    allocations_.push_back(std::move(allocation));
    return true;
}

bool Journal::add(StockShipment shipment) {
    if (!registerId(shipment.id())) return false;
    shipments_.push_back(std::move(shipment));
    return true;
}

Money Journal::balanceOf(SupplierId supplierId) const noexcept {
    // balanceEffect() is virtual: each record type knows its own sign.
    Money balance;
    for (const auto& r : receipts_) {
        if (r.supplierId() == supplierId) balance += r.balanceEffect();
    }
    for (const auto& p : payments_) {
        if (p.supplierId() == supplierId) balance += p.balanceEffect();
    }
    return balance;
}

std::vector<const Transaction*> Journal::historyOf(SupplierId supplierId) const {
    std::vector<const Transaction*> history;
    for (const auto& r : receipts_) {
        if (r.supplierId() == supplierId) history.push_back(&r);
    }
    for (const auto& p : payments_) {
        if (p.supplierId() == supplierId) history.push_back(&p);
    }
    // Stable: records made in the same instant keep receipts before payments.
    std::ranges::stable_sort(history, {}, &Transaction::createdAt);
    return history;
}

Cargo Journal::loadOf(const TruckId& truckId) const noexcept {
    Cargo load;
    for (const auto& a : allocations_) {
        if (a.destination() == Destination{truckId}) load += a.cargo();
    }
    for (const auto& s : shipments_) {
        if (s.truckId() == truckId) load += s.cargo();
    }
    return load;
}

Cargo Journal::stockOf(std::string_view cropCode) const noexcept {
    Cargo stock;
    for (const auto& a : allocations_) {
        if (std::holds_alternative<ToWarehouse>(a.destination()) && a.cropCode() == cropCode) stock += a.cargo();
    }
    for (const auto& s : shipments_) {
        if (s.cropCode() == cropCode) stock -= s.cargo();
    }
    return stock;
}

bool Journal::mentionsCrop(std::string_view cropCode) const noexcept {
    // Allocations and shipments come from receipts, but a sync may bring them first.
    const auto inReceipt = [cropCode](const Receipt& r) {
        return std::ranges::any_of(r.weighings(), [cropCode](const Weighing& w) { return w.cropCode == cropCode; });
    };
    return std::ranges::any_of(receipts_, inReceipt) ||
           std::ranges::any_of(allocations_, [cropCode](const Allocation& a) { return a.cropCode() == cropCode; }) ||
           std::ranges::any_of(shipments_, [cropCode](const StockShipment& s) { return s.cropCode() == cropCode; });
}

const Receipt* Journal::findReceipt(const Uuid& id) const noexcept {
    for (const auto& r : receipts_) {
        if (r.id() == id) return &r;
    }
    return nullptr;
}

Cargo Journal::unallocatedOf(const Uuid& receiptId, std::string_view cropCode) const {
    const Receipt* receipt = findReceipt(receiptId);
    if (receipt == nullptr) throw std::invalid_argument("Receipt is not in the journal");
    Cargo left = receipt->cargoOf(cropCode);
    left -= allocatedOf(receiptId, cropCode);
    return left;
}

Cargo Journal::unallocatedOf(const OpenReceipt& receipt, std::string_view cropCode) const noexcept {
    Cargo left = receipt.cargoOf(cropCode);
    left -= allocatedOf(receipt.id(), cropCode);
    return left;
}

Cargo Journal::allocatedOf(const Uuid& receiptId, std::string_view cropCode) const noexcept {
    Cargo taken;
    for (const auto& a : allocations_) {
        if (a.receiptId() == receiptId && a.cropCode() == cropCode) taken += a.cargo();
    }
    return taken;
}

std::optional<Destination> Journal::destinationOf(const Uuid& receiptId, std::size_t weighingIndex) const noexcept {
    for (const auto& a : allocations_) {
        if (a.receiptId() == receiptId && a.weighingIndex() == weighingIndex) return a.destination();
    }
    return std::nullopt;
}

std::vector<const Receipt*> Journal::waitingForAllocation() const {
    std::vector<const Receipt*> waiting;
    for (const auto& r : receipts_) {
        // Box counts decide, as in checkAllocation(): weight may differ by rounding.
        const bool anyLeft = std::ranges::any_of(r.weighings(), [&](const Weighing& w) {
            return !w.cancelled && unallocatedOf(r.id(), w.cropCode).boxes > 0;
        });
        if (anyLeft) waiting.push_back(&r);
    }
    return waiting;
}

Allocation Journal::boxesOf(const Uuid& receiptId, std::string_view cropCode, int boxes,
                            Destination destination) const {
    const Receipt* receipt = findReceipt(receiptId);
    if (receipt == nullptr) throw std::invalid_argument("Receipt is not in the journal");
    return Allocation::boxesOf(*receipt, cropCode, boxes, destination, unallocatedOf(receiptId, cropCode));
}

Allocation Journal::boxesOf(const OpenReceipt& receipt, std::string_view cropCode, int boxes,
                            Destination destination) const {
    return Allocation::boxesOf(receipt, cropCode, boxes, destination, unallocatedOf(receipt, cropCode));
}

std::vector<std::string> Journal::validateAllocation(const Allocation& allocation) const {
    const Receipt* receipt = findReceipt(allocation.receiptId());
    if (receipt == nullptr) {
        std::vector<std::string> errors = allocation.validate();
        errors.emplace_back("Чек не знайдено в журналі");
        return errors;
    }
    return checkAllocation(allocation, *receipt);
}

std::vector<std::string> Journal::validateAllocation(const Allocation& allocation,
                                                     const OpenReceipt& receipt) const {
    if (allocation.receiptId() != receipt.id()) throw std::invalid_argument("Allocation is from another receipt");
    return checkAllocation(allocation, receipt);
}

std::vector<std::string> Journal::validateCancel(const OpenReceipt& receipt, std::size_t index) const {
    const Weighing& weighing = receipt.weighings().at(index);
    if (weighing.cancelled) return {std::format("Зважування {} вже анульовано", index + 1)};

    for (const auto& a : allocations_) {
        if (a.receiptId() == receipt.id() && a.weighingIndex() == index) {
            return {std::format("Зважування {} вже розподілено", index + 1)};
        }
    }
    // Allocations by box count are not tied to a pallet: whatever is left of
    // the crop after cancelling must still cover them.
    const int left = receipt.cargoOf(weighing.cropCode).boxes - weighing.boxCount;
    const int allocated = allocatedOf(receipt.id(), weighing.cropCode).boxes;
    if (left < allocated) {
        return {std::format("Без зважування {} лишиться {} ящ. культури {}, а розподілено вже {}", index + 1, left,
                            weighing.cropCode, allocated)};
    }
    return {};
}

// Defined here and only used here, so the header does not need it.
template <WeighedReceipt R>
std::vector<std::string> Journal::checkAllocation(const Allocation& allocation, const R& receipt) const {
    std::vector<std::string> errors = allocation.validate();
    if (const auto index = allocation.weighingIndex()) {
        if (receipt.weighings().at(*index).cancelled) {
            errors.push_back(std::format("Зважування {} анульовано", *index + 1));
        }
        for (const auto& a : allocations_) {
            if (a.receiptId() == allocation.receiptId() && a.weighingIndex() == index) {
                errors.push_back(std::format("Зважування {} вже розподілено", *index + 1));
                break;
            }
        }
    }

    // Boxes are what the operator counts; the weight follows from them, so a
    // separate weight check would only report rounding noise.
    Cargo left = receipt.cargoOf(allocation.cropCode());
    left -= allocatedOf(allocation.receiptId(), allocation.cropCode());
    if (allocation.cargo().boxes > left.boxes) {
        errors.push_back(std::format("Залишилось лише {} ящ. культури {}", left.boxes, allocation.cropCode()));
    }
    return errors;
}

StockShipment Journal::shipmentOf(std::string_view cropCode, int boxes, const TruckId& truckId) const {
    const Cargo stock = stockOf(cropCode);
    const Weight weight = stock.boxes > 0 ? grams(divideRounded(stock.weight.raw() * boxes, stock.boxes)) : Weight{};
    return StockShipment::create(std::string{cropCode}, {.boxes = boxes, .weight = weight}, truckId);
}

std::vector<std::string> Journal::validateShipment(const StockShipment& shipment) const {
    std::vector<std::string> errors = shipment.validate();
    const Cargo stock = stockOf(shipment.cropCode());
    if (shipment.cargo().boxes > stock.boxes) {
        errors.push_back(std::format("На складі лише {} ящ. культури {}", stock.boxes, shipment.cropCode()));
    }
    return errors;
}

} // namespace wms
