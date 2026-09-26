#pragma once

#include "wms/cargo.h"
#include "wms/receipt.h"
#include "wms/transaction.h"
#include "wms/truck.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace wms {

// The point has one warehouse (cold storage). When freezers are built this
// becomes a warehouse id; Destination keeps every place spelled out by type.
struct ToWarehouse {
    bool operator==(const ToWarehouse&) const noexcept = default;
};

// Where berries from a receipt go: straight into a truck (cross-docking) or
// into the warehouse to be shipped out later. A variant instead of a special
// "warehouse id" makes the compiler check that every place is handled.
using Destination = std::variant<TruckId, ToWarehouse>;

// Records that part of a receipt went into a truck or the warehouse.
// Loading facts are kept as separate records rather than a counter on Truck,
// so two offline terminals loading the same truck never lose each other's data.
class Allocation final : public Transaction {
public:
    Allocation(Uuid id, TimePoint createdAt, Uuid receiptId, Destination destination, std::string cropCode,
               Cargo cargo, std::optional<std::size_t> weighingIndex,
               SyncStatus syncStatus = SyncStatus::Pending);

    // A whole weighing (usually a pallet) goes to the destination unchanged.
    // Throws std::out_of_range for an index outside the receipt.
    [[nodiscard]] static Allocation wholeWeighing(const Receipt& receipt, std::size_t weighingIndex,
                                                  Destination destination);
    // `boxes` boxes of a crop from anywhere in the receipt; their weight is the
    // crop's net weight split proportionally by boxes.
    // Throws std::invalid_argument when the receipt has no such crop.
    [[nodiscard]] static Allocation boxesOf(const Receipt& receipt, std::string_view cropCode, int boxes,
                                            Destination destination);
    // Same, but the weight is split from what is still `available` of the crop
    // (see Journal::unallocatedOf), so taking the last boxes takes exactly the
    // remaining weight and rounding never loses or invents grams.
    // With nothing available the weight is zero and validate() reports it.
    [[nodiscard]] static Allocation boxesOf(const Receipt& receipt, std::string_view cropCode, int boxes,
                                            Destination destination, const Cargo& available);

    [[nodiscard]] const Uuid& receiptId() const noexcept { return receiptId_; }
    [[nodiscard]] const Destination& destination() const noexcept { return destination_; }
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
    Destination destination_;
    std::string cropCode_;
    Cargo cargo_;
    std::optional<std::size_t> weighingIndex_;
};

} // namespace wms
