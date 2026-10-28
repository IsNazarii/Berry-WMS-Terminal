#include "wms/payment.h"

#include <format>
#include <stdexcept>

namespace wms {

Payment::Payment(Uuid id, TimePoint createdAt, SupplierId supplierId, Money amount, PaymentMethod method,
                 OperatorId paidBy, SyncStatus syncStatus, std::optional<Uuid> sessionId)
    : Transaction{id, createdAt, syncStatus},
      supplierId_{supplierId},
      amount_{amount},
      method_{method},
      paidBy_{paidBy},
      sessionId_{sessionId} {
    if (paidBy_ <= 0) throw std::invalid_argument("Payment must name the operator who paid");
}

Payment Payment::create(SupplierId supplierId, Money amount, PaymentMethod method, OperatorId paidBy,
                        std::optional<Uuid> sessionId) {
    return Payment{Uuid::generateV7(), Clock::now(), supplierId, amount, method, paidBy, SyncStatus::Pending, sessionId};
}

std::vector<std::string> Payment::validate() const {
    std::vector<std::string> errors;
    if (supplierId_ <= 0) errors.emplace_back("Не обрано постачальника");
    if (!amount_.isPositive()) errors.emplace_back("Сума виплати має бути більшою за нуль");
    return errors;
}

std::string Payment::describe() const {
    return std::format("Виплата {}: {}", method_ == PaymentMethod::Cash ? "готівкою" : "переказом",
                       formatMoney(amount_));
}

} // namespace wms
