#include "wms/loading_history.h"

#include <algorithm>
#include <iterator>
#include <ranges>
#include <variant>

namespace wms {

namespace {

// What the history needs to know about the receipt an allocation took from.
struct Source {
    SupplierId supplierId;
    std::optional<Uuid> sessionId;
};

template <WeighedReceipt R>
Source sourceOf(const R& receipt) {
    return {.supplierId = receipt.supplierId(), .sessionId = receipt.sessionId()};
}

} // namespace

LoadingHistory::LoadingHistory(const WorkDay& day, const Journal& journal,
                               std::span<const OpenReceipt> otherOpenReceipts)
    : closedAt_{day.closedAt()} {
    const auto isDayTruck = [&day](const Destination& to) {
        const TruckId* truck = std::get_if<TruckId>(&to);
        return truck != nullptr && day.findTruck(*truck) != nullptr;
    };
    // An allocation may come from a receipt closed long ago, from one still
    // being weighed, or (after a sync) from one not here at all.
    const auto findSource = [&](const Uuid& receiptId) -> std::optional<Source> {
        if (const Receipt* closed = journal.findReceipt(receiptId)) return sourceOf(*closed);
        if (const OpenReceipt* open = day.findReceipt(receiptId)) return sourceOf(*open);
        const auto other = std::ranges::find(otherOpenReceipts, receiptId, &OpenReceipt::id);
        if (other != otherOpenReceipts.end()) return sourceOf(*other);
        return std::nullopt;
    };

    for (const Allocation& a : journal.allocations()) {
        const std::optional<Source> source = findSource(a.receiptId());
        // Into the warehouse counts for the day the receipt belongs to.
        const bool intoDayWarehouse = std::holds_alternative<ToWarehouse>(a.destination()) && source.has_value() &&
                                      source->sessionId == day.id();
        if (!isDayTruck(a.destination()) && !intoDayWarehouse) continue;

        const AllocationReversal* reversal = journal.reversalFor(a.id());
        entries_.push_back({
            .recordId = a.id(),
            .at = a.createdAt(),
            .to = a.destination(),
            .cropCode = a.cropCode(),
            .cargo = a.cargo(),
            .receiptId = a.receiptId(),
            .supplierId = source ? std::optional{source->supplierId} : std::nullopt,
            .weighingIndex = a.weighingIndex(),
            .by = a.allocatedBy(),
            .reversedAt = reversal ? std::optional{reversal->createdAt()} : std::nullopt,
            .reversedBy = reversal ? reversal->reversedBy() : std::nullopt,
        });
    }
    for (const StockShipment& s : journal.shipments()) {
        if (!isDayTruck(s.truckId())) continue;
        entries_.push_back({
            .recordId = s.id(),
            .at = s.createdAt(),
            .to = s.truckId(),
            .cropCode = s.cropCode(),
            .cargo = s.cargo(),
            .receiptId = std::nullopt,
            .supplierId = std::nullopt,
            .weighingIndex = std::nullopt,
            .by = s.shippedBy(),
            .reversedAt = std::nullopt,
            .reversedBy = std::nullopt,
        });
    }
    // Stable: records of the same instant keep the journal's order.
    std::ranges::stable_sort(entries_, {}, &LoadingEntry::at);
}

std::vector<LoadingEntry> LoadingHistory::intoTruck(const TruckId& truckId) const {
    std::vector<LoadingEntry> found;
    std::ranges::copy_if(entries_, std::back_inserter(found),
                         [&truckId](const LoadingEntry& e) { return e.to == Destination{truckId}; });
    return found;
}

std::vector<LoadingEntry> LoadingHistory::intoTrucks() const {
    std::vector<LoadingEntry> found;
    std::ranges::copy_if(entries_, std::back_inserter(found),
                         [](const LoadingEntry& e) { return std::holds_alternative<TruckId>(e.to); });
    return found;
}

std::vector<LoadingEntry> LoadingHistory::warehouse() const {
    // In: allocations to the warehouse. Out: shipments, the only entries without a receipt.
    std::vector<LoadingEntry> found;
    std::ranges::copy_if(entries_, std::back_inserter(found), [this](const LoadingEntry& e) {
        if (std::holds_alternative<ToWarehouse>(e.to)) return true;
        return e.fromWarehouse() && (!closedAt_ || e.at <= *closedAt_);
    });
    return found;
}

} // namespace wms
