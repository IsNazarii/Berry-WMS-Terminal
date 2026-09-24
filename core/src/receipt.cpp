#include "wms/receipt.h"

#include <format>
#include <utility>

namespace wms {

Weight Weighing::tare() const noexcept { return tarePerBox * boxCount + palletTare; }

Weight Weighing::net() const noexcept { return gross - tare(); }

Money Weighing::amount() const noexcept { return costOf(net(), pricePerKg); }

std::vector<std::string> Weighing::validate() const {
    std::vector<std::string> errors;
    if (cropCode.empty()) errors.emplace_back("Не обрано культуру");
    if (!gross.isPositive()) errors.emplace_back("Вага брутто має бути більшою за нуль");
    if (boxCount <= 0) errors.emplace_back("Кількість ящиків має бути більшою за нуль");
    if (tarePerBox.isNegative()) errors.emplace_back("Вага тари не може бути від'ємною");
    if (palletTare.isNegative()) errors.emplace_back("Вага палети не може бути від'ємною");
    if (!net().isPositive()) errors.emplace_back("Вага нетто має бути більшою за нуль");
    if (!pricePerKg.isPositive()) errors.emplace_back("Ціна за кг має бути більшою за нуль");
    return errors;
}

Receipt::Receipt(Uuid id, TimePoint createdAt, SupplierId supplierId, std::vector<Weighing> weighings,
                 SyncStatus syncStatus)
    : Transaction{id, createdAt, syncStatus}, supplierId_{supplierId}, weighings_{std::move(weighings)} {}

Receipt Receipt::create(SupplierId supplierId, std::vector<Weighing> weighings) {
    return Receipt{Uuid::generateV7(), Clock::now(), supplierId, std::move(weighings)};
}

Cargo Receipt::total() const noexcept {
    Cargo sum;
    for (const auto& w : weighings_) sum += w.cargo();
    return sum;
}

Cargo Receipt::cargoOf(std::string_view cropCode) const noexcept {
    Cargo sum;
    for (const auto& w : weighings_) {
        if (w.cropCode == cropCode) sum += w.cargo();
    }
    return sum;
}

Money Receipt::amount() const noexcept {
    // Sum of already rounded lines, so the total always matches the printed lines.
    Money sum;
    for (const auto& w : weighings_) sum += w.amount();
    return sum;
}

std::vector<std::string> Receipt::validate() const {
    std::vector<std::string> errors;
    if (supplierId_ <= 0) errors.emplace_back("Не обрано постачальника");
    if (weighings_.empty()) errors.emplace_back("Чек не містить жодного зважування");
    for (std::size_t i = 0; i < weighings_.size(); ++i) {
        for (const auto& error : weighings_[i].validate()) {
            errors.push_back(std::format("Зважування {}: {}", i + 1, error));
        }
    }
    return errors;
}

std::string Receipt::describe() const {
    std::string text = std::format("Прийом: {} зваж., {} ящ., нетто {} = {}", weighings_.size(), total().boxes,
                                   formatWeight(net()), formatMoney(amount()));
    for (const auto& w : weighings_) {
        text += std::format("\n  {}: {} ящ., нетто {} × {}/кг = {}{}", w.cropCode, w.boxCount, formatWeight(w.net()),
                            formatMoney(w.pricePerKg), formatMoney(w.amount()),
                            w.weightSource == WeightSource::Manual ? " [ручне введення ваги]" : "");
    }
    return text;
}

} // namespace wms
