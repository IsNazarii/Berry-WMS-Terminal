#include "wms/payment.h"

#include <format>

namespace wms {

Payment::Payment(Uuid id, TimePoint createdAt, SupplierId supplierId, Money amount, PaymentMethod method,
                 SyncStatus syncStatus)
    : Transaction{id, createdAt, syncStatus}, supplierId_{supplierId}, amount_{amount}, method_{method} {}

Payment Payment::create(SupplierId supplierId, Money amount, PaymentMethod method) {
    return Payment{Uuid::generateV7(), Clock::now(), supplierId, amount, method};
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
