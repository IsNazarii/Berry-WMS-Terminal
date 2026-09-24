#pragma once

#include "wms/supplier.h"
#include "wms/transaction.h"

#include <string>

namespace wms {

enum class WeightSource { Scale, Manual };

struct ReceiptInput {
    SupplierId supplierId = 0;
    std::string cropCode; // e.g. "raspberry"
    Weight gross;
    int boxCount = 0;
    Weight tarePerBox;
    Money pricePerKg;         // snapshot of the price at the moment of reception
    int priceListVersion = 0; // which price list the snapshot came from
    WeightSource weightSource = WeightSource::Scale;
};

// One reception of berries from a supplier.
class Receipt final : public Transaction {
public:
    Receipt(Uuid id, TimePoint createdAt, ReceiptInput input, SyncStatus syncStatus = SyncStatus::Pending);

    // New receipt with a fresh UUIDv7 and the current time.
    [[nodiscard]] static Receipt create(ReceiptInput input);

    [[nodiscard]] const ReceiptInput& input() const noexcept { return input_; }

    [[nodiscard]] Weight tare() const noexcept;
    [[nodiscard]] Weight net() const noexcept;
    [[nodiscard]] Money amount() const noexcept;

    [[nodiscard]] TransactionKind kind() const noexcept override { return TransactionKind::Receipt; }
    [[nodiscard]] Money balanceEffect() const noexcept override;
    [[nodiscard]] std::vector<std::string> validate() const override;
    [[nodiscard]] std::string describe() const override;

private:
    ReceiptInput input_;
};

} // namespace wms
