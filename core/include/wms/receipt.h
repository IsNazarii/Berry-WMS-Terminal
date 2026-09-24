#pragma once

#include "wms/cargo.h"
#include "wms/supplier.h"
#include "wms/transaction.h"

#include <string>
#include <string_view>
#include <vector>

namespace wms {

enum class WeightSource { Scale, Manual };

// One weighing on the scale, usually one pallet. Different crops are always
// weighed separately, so a weighing holds exactly one crop.
struct Weighing {
    std::string cropCode; // e.g. "raspberry"
    Weight gross;
    int boxCount = 0;
    Weight tarePerBox;
    Weight palletTare;        // entered by the operator; zero when weighed without a pallet
    Money pricePerKg;         // snapshot of the price at the moment of reception
    int priceListVersion = 0; // which price list the snapshot came from
    WeightSource weightSource = WeightSource::Scale;

    [[nodiscard]] Weight tare() const noexcept;
    [[nodiscard]] Weight net() const noexcept;
    // Rounded to whole kopecks per weighing, as printed on the receipt line.
    [[nodiscard]] Money amount() const noexcept;
    [[nodiscard]] Cargo cargo() const noexcept { return {.boxes = boxCount, .weight = net()}; }
    [[nodiscard]] std::vector<std::string> validate() const;
};

// Everything one supplier brought in one visit: several pallets, possibly of
// different crops, paid by a single receipt.
class Receipt final : public Transaction {
public:
    Receipt(Uuid id, TimePoint createdAt, SupplierId supplierId, std::vector<Weighing> weighings,
            SyncStatus syncStatus = SyncStatus::Pending);

    // New receipt with a fresh UUIDv7 and the current time.
    [[nodiscard]] static Receipt create(SupplierId supplierId, std::vector<Weighing> weighings);

    [[nodiscard]] SupplierId supplierId() const noexcept { return supplierId_; }
    [[nodiscard]] const std::vector<Weighing>& weighings() const noexcept { return weighings_; }

    [[nodiscard]] Cargo total() const noexcept;
    [[nodiscard]] Cargo cargoOf(std::string_view cropCode) const noexcept;
    [[nodiscard]] Weight net() const noexcept { return total().weight; }
    [[nodiscard]] Money amount() const noexcept;

    [[nodiscard]] TransactionKind kind() const noexcept override { return TransactionKind::Receipt; }
    [[nodiscard]] Money balanceEffect() const noexcept override { return amount(); }
    [[nodiscard]] std::vector<std::string> validate() const override;
    [[nodiscard]] std::string describe() const override;

private:
    SupplierId supplierId_;
    std::vector<Weighing> weighings_;
};

} // namespace wms
