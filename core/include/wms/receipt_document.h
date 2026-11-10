#pragma once

#include "wms/cargo.h"
#include "wms/crop.h"
#include "wms/payment.h"
#include "wms/receipt.h"
#include "wms/supplier.h"
#include "wms/transaction.h"
#include "wms/uuid.h"

#include <optional>
#include <string>
#include <vector>

namespace wms {

struct ReceiptLine {
    int number = 0; // 1, 2, 3… on paper; a cancelled line leaves no gap
    std::string crop;
    int boxes = 0;
    Weight gross;
    Weight tare;
    Weight net;
    Money pricePerKg;
    Money amount;
};

struct ReceiptCropTotal {
    std::string crop;
    Cargo cargo;
    Money amount;
};

struct ReceiptPaymentInfo {
    PaymentMethod method = PaymentMethod::Cash;
    Money amount;
    TimePoint at;
    std::string by;
};

struct ReceiptPrintInfo {
    TimePoint at;
    std::string by;
    // A reprint says so: one receipt must not look like two.
    bool copy = false;
};

// What the printed receipt says, in the words of the paper: crop names instead of
// codes, no cancelled lines, nothing about how a price was set. The rules of what
// goes on paper live here and are tested without a printer; drawing it is infra's job.
struct ReceiptDocument {
    Uuid receiptId;
    TimePoint receivedAt;
    SupplierId supplierId = 0;
    std::string supplierName;
    Settlement settlement = Settlement::OnSite;
    std::vector<ReceiptLine> lines;
    std::vector<ReceiptCropTotal> cropTotals;
    Cargo total;
    Money amount;
    std::optional<ReceiptPaymentInfo> payment;
    ReceiptPrintInfo printed;

    // Wholesale is weight only: its price is settled outside the terminal.
    [[nodiscard]] bool priced() const noexcept { return settlement == Settlement::OnSite; }
};

// crops: the catalogue, archived ones included; a crop not in it shows its code.
[[nodiscard]] ReceiptDocument makeReceiptDocument(const Receipt& receipt, const Supplier& supplier,
                                                  const std::vector<Crop>& crops,
                                                  const std::optional<ReceiptPaymentInfo>& payment,
                                                  const ReceiptPrintInfo& printed);

} // namespace wms
