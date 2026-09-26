#include "wms/journal.h"

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

Cargo Journal::loadOf(TruckId truckId) const noexcept {
    Cargo load;
    for (const auto& a : allocations_) {
        if (a.truckId() == truckId) load += a.cargo();
    }
    return load;
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
    for (const auto& a : allocations_) {
        if (a.receiptId() == receiptId && a.cropCode() == cropCode) {
            left.boxes -= a.cargo().boxes;
            left.weight -= a.cargo().weight;
        }
    }
    return left;
}

Allocation Journal::boxesOf(const Uuid& receiptId, std::string_view cropCode, int boxes,
                            TruckId truckId) const {
    const Receipt* receipt = findReceipt(receiptId);
    if (receipt == nullptr) throw std::invalid_argument("Receipt is not in the journal");
    return Allocation::boxesOf(*receipt, cropCode, boxes, truckId, unallocatedOf(receiptId, cropCode));
}

std::vector<std::string> Journal::validateAllocation(const Allocation& allocation) const {
    std::vector<std::string> errors = allocation.validate();
    if (findReceipt(allocation.receiptId()) == nullptr) {
        errors.emplace_back("Чек не знайдено в журналі");
        return errors;
    }

    if (const auto index = allocation.weighingIndex()) {
        for (const auto& a : allocations_) {
            if (a.receiptId() == allocation.receiptId() && a.weighingIndex() == index) {
                errors.push_back(std::format("Зважування {} вже розподілено", *index + 1));
                break;
            }
        }
    }

    // Boxes are what the operator counts; the weight follows from them, so a
    // separate weight check would only report rounding noise.
    const Cargo left = unallocatedOf(allocation.receiptId(), allocation.cropCode());
    if (allocation.cargo().boxes > left.boxes) {
        errors.push_back(std::format("Залишилось лише {} ящ. культури {}", left.boxes, allocation.cropCode()));
    }
    return errors;
}

} // namespace wms
