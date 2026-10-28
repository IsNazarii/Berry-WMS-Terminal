#include "wms/session_summary.h"

#include <algorithm>

namespace wms {

SessionSummary::SessionSummary(const WorkDay& day, const Journal& journal) {
    for (const Receipt* receipt : journal.receiptsOf(day.id())) {
        count(*receipt, journal);
        ++closedReceipts_;
    }
    for (const OpenReceipt& receipt : day.openReceipts()) {
        count(receipt, journal);
        ++openReceipts_;
    }
    std::ranges::stable_sort(byCrop_, [](const CropTotal& a, const CropTotal& b) {
        return a.cargo.weight > b.cargo.weight;
    });

    for (const Payment* payment : journal.paymentsOf(day.id())) {
        (payment->method() == PaymentMethod::Cash ? paidCash_ : paidTransfer_) += payment->amount();
    }
}

template <WeighedReceipt R>
void SessionSummary::count(const R& receipt, const Journal& journal) {
    for (const Weighing& w : receipt.weighings()) {
        if (w.cancelled) continue;
        received_ += w.cargo();
        auto crop = std::ranges::find(byCrop_, w.cropCode, &CropTotal::cropCode);
        if (crop == byCrop_.end()) crop = byCrop_.insert(byCrop_.end(), {.cropCode = w.cropCode, .cargo = {}});
        crop->cargo += w.cargo();
    }
    const Placement placement = journal.placementOf(receipt.id());
    placed_.inTrucks += placement.inTrucks;
    placed_.inWarehouse += placement.inWarehouse;
}

Cargo SessionSummary::waiting() const noexcept {
    Cargo left = received_;
    left -= placed_.inTrucks;
    left -= placed_.inWarehouse;
    return left;
}

} // namespace wms
