#pragma once

#include "wms/cargo.h"
#include "wms/supplier.h"
#include "wms/transaction.h"

#include <concepts>
#include <string>
#include <string_view>
#include <vector>

namespace wms {

enum class WeightSource { Scale, Manual };
// Manual = the operator changed the day price for this weighing (a one-off raise).
// Kept for reconciliation only; the printed receipt looks the same either way.
enum class PriceSource { DayPrice, Manual };

// Chosen by the operator when closing a receipt.
enum class Settlement {
    OnSite,     // retail: price known, paid by the operator now or later (debt stays in the journal)
    HandedOver, // wholesale: weight only, priced and paid by a responsible person outside the terminal
};

// One weighing on the scale, usually one pallet. Different crops are always
// weighed separately, so a weighing holds exactly one crop.
struct Weighing {
    std::string cropCode; // e.g. "raspberry"
    Weight gross;
    int boxCount = 0;
    Weight tarePerBox;
    Weight palletTare; // entered by the operator; zero when weighed without a pallet
    Money pricePerKg; // snapshot of the price at reception; zero for Settlement::HandedOver
    PriceSource priceSource = PriceSource::DayPrice;
    WeightSource weightSource = WeightSource::Scale;
    // A mistake (e.g. weighed into the wrong receipt). The line stays, so
    // allocations keep pointing at the right weighing by index, but it is
    // counted nowhere and not printed.
    bool cancelled = false;

    [[nodiscard]] Weight tare() const noexcept;
    [[nodiscard]] Weight net() const noexcept;
    // Rounded to whole kopecks per weighing, as printed on the receipt line.
    [[nodiscard]] Money amount() const noexcept;
    [[nodiscard]] Cargo cargo() const noexcept { return {.boxes = boxCount, .weight = net()}; }
    // Weight checks only; the price rule depends on the receipt's Settlement.
    [[nodiscard]] std::vector<std::string> validate() const;
};

// Everything one supplier brought in one visit: several pallets, possibly of
// different crops, paid by a single receipt.
class Receipt final : public Transaction {
public:
    Receipt(Uuid id, TimePoint createdAt, SupplierId supplierId, Settlement settlement,
            std::vector<Weighing> weighings, SyncStatus syncStatus = SyncStatus::Pending);

    // New receipt with a fresh UUIDv7 and the current time. Settlement has no
    // default on purpose: it decides whether the receipt creates a debt.
    [[nodiscard]] static Receipt create(SupplierId supplierId, Settlement settlement, std::vector<Weighing> weighings);

    [[nodiscard]] SupplierId supplierId() const noexcept { return supplierId_; }
    [[nodiscard]] Settlement settlement() const noexcept { return settlement_; }
    // All lines, cancelled ones included; totals below skip the cancelled.
    [[nodiscard]] const std::vector<Weighing>& weighings() const noexcept { return weighings_; }

    [[nodiscard]] Cargo total() const noexcept;
    [[nodiscard]] Cargo cargoOf(std::string_view cropCode) const noexcept;
    [[nodiscard]] Weight net() const noexcept { return total().weight; }
    // Zero for Settlement::HandedOver: the terminal does not know the price.
    [[nodiscard]] Money amount() const noexcept;

    [[nodiscard]] TransactionKind kind() const noexcept override { return TransactionKind::Receipt; }
    [[nodiscard]] Money balanceEffect() const noexcept override { return amount(); }
    [[nodiscard]] std::vector<std::string> validate() const override;
    [[nodiscard]] std::string describe() const override;

private:
    SupplierId supplierId_;
    Settlement settlement_;
    std::vector<Weighing> weighings_;
};

// Weighed berries with a final id: a closed Receipt or one still open.
// Allocations work on both, since pallets go into trucks while unloading goes on.
template <class R>
concept WeighedReceipt = requires(const R& receipt, std::string_view cropCode) {
    { receipt.id() } -> std::convertible_to<const Uuid&>;
    { receipt.weighings() } -> std::convertible_to<const std::vector<Weighing>&>;
    { receipt.cargoOf(cropCode) } -> std::same_as<Cargo>;
};

} // namespace wms
