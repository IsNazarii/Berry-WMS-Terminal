#pragma once

#include "wms/operator.h"
#include "wms/supplier.h"
#include "wms/transaction.h"

#include <optional>

namespace wms {

enum class PaymentMethod { Cash, BankTransfer };

// Money handed over to a supplier; reduces what we owe them. Names the
// operator who paid: when the till is short, that is the first question.
class Payment final : public Transaction {
public:
    // Throws std::invalid_argument for a non-positive paidBy: the UI passes
    // an operator it has already confirmed, so a missing one is a bug.
    // sessionId is empty for a payment made while no session is open:
    // the cash desk works without one.
    Payment(Uuid id, TimePoint createdAt, SupplierId supplierId, Money amount, PaymentMethod method,
            OperatorId paidBy, SyncStatus syncStatus = SyncStatus::Pending,
            std::optional<Uuid> sessionId = std::nullopt);

    [[nodiscard]] static Payment create(SupplierId supplierId, Money amount, PaymentMethod method, OperatorId paidBy,
                                        std::optional<Uuid> sessionId = std::nullopt);

    [[nodiscard]] SupplierId supplierId() const noexcept { return supplierId_; }
    [[nodiscard]] Money amount() const noexcept { return amount_; }
    [[nodiscard]] PaymentMethod method() const noexcept { return method_; }
    [[nodiscard]] OperatorId paidBy() const noexcept { return paidBy_; }
    [[nodiscard]] const std::optional<Uuid>& sessionId() const noexcept { return sessionId_; }

    [[nodiscard]] TransactionKind kind() const noexcept override { return TransactionKind::Payment; }
    [[nodiscard]] Money balanceEffect() const noexcept override { return -amount_; }
    [[nodiscard]] std::vector<std::string> validate() const override;
    [[nodiscard]] std::string describe() const override;

private:
    SupplierId supplierId_;
    Money amount_;
    PaymentMethod method_;
    OperatorId paidBy_;
    std::optional<Uuid> sessionId_;
};

} // namespace wms
