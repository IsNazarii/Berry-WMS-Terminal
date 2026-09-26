#pragma once

#include "wms/cargo.h"
#include "wms/receipt.h"
#include "wms/transaction.h"
#include "wms/truck.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace wms {

// Records that part of a receipt went into a truck (cross-docking: berries are
// not stored, they are moved from the supplier straight into outgoing trucks).
// Loading facts are kept as separate records rather than a counter on Truck,
// so two offline terminals loading the same truck never lose each other's data.
class CrossDockAllocation final : public Transaction {
public:
    CrossDockAllocation(Uuid id, TimePoint createdAt, Uuid receiptId, TruckId truckId, std::string cropCode,
                        Cargo cargo, std::optional<std::size_t> weighingIndex,
                        SyncStatus syncStatus = SyncStatus::Pending);

    // A whole weighing (usually a pallet) goes into the truck unchanged.
    // Throws std::out_of_range for an index outside the receipt.
    [[nodiscard]] static CrossDockAllocation wholeWeighing(const Receipt& receipt, std::size_t weighingIndex,
                                                           TruckId truckId);
    // `boxes` boxes of a crop from anywhere in the receipt; their weight is the
    // crop's net weight split proportionally by boxes.
    // Throws std::invalid_argument when the receipt has no such crop.
    [[nodiscard]] static CrossDockAllocation boxesOf(const Receipt& receipt, std::string_view cropCode, int boxes,
                                                     TruckId truckId);
    // Same, but the weight is split from what is still `available` of the crop
    // (see Journal::unallocatedOf), so taking the last boxes takes exactly the
    // remaining weight and rounding never loses or invents grams.
    // With nothing available the weight is zero and validate() reports it.
    [[nodiscard]] static CrossDockAllocation boxesOf(const Receipt& receipt, std::string_view cropCode, int boxes,
                                                     TruckId truckId, const Cargo& available);

    [[nodiscard]] const Uuid& receiptId() const noexcept { return receiptId_; }
    [[nodiscard]] const TruckId& truckId() const noexcept { return truckId_; }
    [[nodiscard]] const std::string& cropCode() const noexcept { return cropCode_; }
    [[nodiscard]] const Cargo& cargo() const noexcept { return cargo_; }
    // Set only when a whole weighing was allocated.
    [[nodiscard]] std::optional<std::size_t> weighingIndex() const noexcept { return weighingIndex_; }

    [[nodiscard]] TransactionKind kind() const noexcept override { return TransactionKind::Allocation; }
    // Moving berries does not change what we owe the supplier.
    [[nodiscard]] Money balanceEffect() const noexcept override { return Money{}; }
    [[nodiscard]] std::vector<std::string> validate() const override;
    [[nodiscard]] std::string describe() const override;

private:
    Uuid receiptId_;
    TruckId truckId_;
    std::string cropCode_;
    Cargo cargo_;
    std::optional<std::size_t> weighingIndex_;
};

} // namespace wms
