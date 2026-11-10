#include "wms/receipt_document.h"

#include <algorithm>
#include <iterator>
#include <string_view>
#include <utility>

namespace wms {
namespace {

std::string nameOf(std::string_view code, const std::vector<Crop>& crops) {
    const auto crop = std::ranges::find(crops, code, &Crop::code);
    return crop != crops.end() ? crop->name() : std::string{code};
}

} // namespace

ReceiptDocument makeReceiptDocument(const Receipt& receipt, const Supplier& supplier, const std::vector<Crop>& crops,
                                    const std::optional<ReceiptPaymentInfo>& payment,
                                    const ReceiptPrintInfo& printed) {
    ReceiptDocument doc{
        .receiptId = receipt.id(),
        .receivedAt = receipt.createdAt(),
        .supplierId = supplier.id(),
        .supplierName = supplier.fullName(),
        .settlement = receipt.settlement(),
        .lines = {},
        .cropTotals = {},
        .total = receipt.total(),
        .amount = receipt.amount(),
        .payment = payment,
        .printed = printed,
    };
    for (const Weighing& w : receipt.weighings()) {
        if (w.cancelled) continue;
        std::string name = nameOf(w.cropCode, crops);
        doc.lines.push_back({
            .number = static_cast<int>(doc.lines.size()) + 1,
            .crop = name,
            .boxes = w.boxCount,
            .gross = w.gross,
            .tare = w.tare(),
            .net = w.net(),
            .pricePerKg = w.pricePerKg,
            .amount = w.amount(),
        });
        auto total = std::ranges::find(doc.cropTotals, name, &ReceiptCropTotal::crop);
        if (total == doc.cropTotals.end()) {
            doc.cropTotals.push_back({.crop = std::move(name), .cargo = {}, .amount = {}});
            total = std::prev(doc.cropTotals.end());
        }
        total->cargo += w.cargo();
        total->amount += w.amount();
    }
    return doc;
}

} // namespace wms
