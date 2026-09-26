#include "wms/journal.h"

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

bool Journal::add(CrossDockAllocation allocation) {
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

} // namespace wms
