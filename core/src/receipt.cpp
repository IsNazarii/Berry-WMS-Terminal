#include "wms/receipt.h"

#include <algorithm>
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
    return errors;
}

Receipt::Receipt(Uuid id, TimePoint createdAt, SupplierId supplierId, Settlement settlement,
                 std::vector<Weighing> weighings, SyncStatus syncStatus)
    : Transaction{id, createdAt, syncStatus}, supplierId_{supplierId}, settlement_{settlement},
      weighings_{std::move(weighings)} {}

Receipt Receipt::create(SupplierId supplierId, Settlement settlement, std::vector<Weighing> weighings) {
    return Receipt{Uuid::generateV7(), Clock::now(), supplierId, settlement, std::move(weighings)};
}

Cargo Receipt::total() const noexcept {
    Cargo sum;
    for (const auto& w : weighings_) {
        if (!w.cancelled) sum += w.cargo();
    }
    return sum;
}

Cargo Receipt::cargoOf(std::string_view cropCode) const noexcept {
    Cargo sum;
    for (const auto& w : weighings_) {
        if (!w.cancelled && w.cropCode == cropCode) sum += w.cargo();
    }
    return sum;
}

Money Receipt::amount() const noexcept {
    if (settlement_ == Settlement::HandedOver) return Money{};
    // Sum of already rounded lines, so the total always matches the printed lines.
    Money sum;
    for (const auto& w : weighings_) {
        if (!w.cancelled) sum += w.amount();
    }
    return sum;
}

std::vector<std::string> Receipt::validate() const {
    std::vector<std::string> errors;
    if (supplierId_ <= 0) errors.emplace_back("Не обрано постачальника");
    if (std::ranges::all_of(weighings_, &Weighing::cancelled)) {
        errors.emplace_back("Чек не містить жодного зважування");
    }
    for (std::size_t i = 0; i < weighings_.size(); ++i) {
        if (weighings_[i].cancelled) continue;
        auto lineErrors = weighings_[i].validate();
        const Money price = weighings_[i].pricePerKg;
        if (settlement_ == Settlement::OnSite && !price.isPositive()) {
            lineErrors.emplace_back("Ціна за кг має бути більшою за нуль");
        }
        if (settlement_ == Settlement::HandedOver && !price.isZero()) {
            lineErrors.emplace_back("Для оптового чека ціна не вказується");
        }
        for (const auto& error : lineErrors) {
            errors.push_back(std::format("Зважування {}: {}", i + 1, error));
        }
    }
    return errors;
}

std::string Receipt::describe() const {
    const bool handedOver = settlement_ == Settlement::HandedOver;
    const auto counted = std::ranges::count(weighings_, false, &Weighing::cancelled);
    std::string text = std::format("Прийом: {} зваж., {} ящ., нетто {}", counted, total().boxes,
                                   formatWeight(net()));
    text += handedOver ? " [опт, до розрахунку]" : std::format(" = {}", formatMoney(amount()));
    for (const auto& w : weighings_) {
        text += std::format("\n  {}: {} ящ., нетто {}", w.cropCode, w.boxCount, formatWeight(w.net()));
        if (!handedOver) text += std::format(" × {}/кг = {}", formatMoney(w.pricePerKg), formatMoney(w.amount()));
        if (w.weightSource == WeightSource::Manual) text += " [ручне введення ваги]";
        if (w.cancelled) text += " [анульовано]";
    }
    return text;
}

} // namespace wms
