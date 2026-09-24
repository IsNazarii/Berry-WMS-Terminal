#include "wms/receipt.h"

#include <format>
#include <utility>

namespace wms {

Receipt::Receipt(Uuid id, TimePoint createdAt, ReceiptInput input, SyncStatus syncStatus)
    : Transaction{id, createdAt, syncStatus}, input_{std::move(input)} {}

Receipt Receipt::create(ReceiptInput input) { return Receipt{Uuid::generateV7(), Clock::now(), std::move(input)}; }

Weight Receipt::tare() const noexcept { return input_.tarePerBox * input_.boxCount; }

Weight Receipt::net() const noexcept { return input_.gross - tare(); }

Money Receipt::amount() const noexcept { return costOf(net(), input_.pricePerKg); }

Money Receipt::balanceEffect() const noexcept { return amount(); }

std::vector<std::string> Receipt::validate() const {
    std::vector<std::string> errors;
    if (input_.supplierId <= 0) errors.emplace_back("Не обрано постачальника");
    if (input_.cropCode.empty()) errors.emplace_back("Не обрано культуру");
    if (!input_.gross.isPositive()) errors.emplace_back("Вага брутто має бути більшою за нуль");
    if (input_.boxCount <= 0) errors.emplace_back("Кількість ящиків має бути більшою за нуль");
    if (input_.tarePerBox.isNegative()) errors.emplace_back("Вага тари не може бути від'ємною");
    if (!net().isPositive()) errors.emplace_back("Вага нетто має бути більшою за нуль");
    if (!input_.pricePerKg.isPositive()) errors.emplace_back("Ціна за кг має бути більшою за нуль");
    return errors;
}

std::string Receipt::describe() const {
    return std::format("Прийом {}: {} ящ., нетто {} × {}/кг = {}{}", input_.cropCode, input_.boxCount,
                       formatWeight(net()), formatMoney(input_.pricePerKg), formatMoney(amount()),
                       input_.weightSource == WeightSource::Manual ? " [ручне введення ваги]" : "");
}

} // namespace wms
