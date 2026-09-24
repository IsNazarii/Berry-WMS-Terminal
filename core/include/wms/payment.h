#pragma once

#include "wms/supplier.h"
#include "wms/transaction.h"

namespace wms {

enum class PaymentMethod { Cash, BankTransfer };

// Money handed over to a supplier; reduces what we owe them.
class Payment final : public Transaction {
public:
    Payment(Uuid id, TimePoint createdAt, SupplierId supplierId, Money amount, PaymentMethod method,
            SyncStatus syncStatus = SyncStatus::Pending);

    [[nodiscard]] static Payment create(SupplierId supplierId, Money amount, PaymentMethod method);

    [[nodiscard]] SupplierId supplierId() const noexcept { return supplierId_; }
    [[nodiscard]] Money amount() const noexcept { return amount_; }
    [[nodiscard]] PaymentMethod method() const noexcept { return method_; }

    [[nodiscard]] TransactionKind kind() const noexcept override { return TransactionKind::Payment; }
    [[nodiscard]] Money balanceEffect() const noexcept override { return -amount_; }
    [[nodiscard]] std::vector<std::string> validate() const override;
    [[nodiscard]] std::string describe() const override;

private:
    SupplierId supplierId_;
    Money amount_;
    PaymentMethod method_;
};

} // namespace wms
