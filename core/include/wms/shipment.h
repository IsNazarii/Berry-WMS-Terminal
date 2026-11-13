#pragma once

#include "wms/cargo.h"
#include "wms/operator.h"
#include "wms/transaction.h"
#include "wms/truck.h"

#include <optional>
#include <string>

namespace wms {

// Berries taken out of the warehouse into a truck of the current session.
// No link to a receipt: in the warehouse boxes of one crop are no longer told
// apart by supplier.
class StockShipment final : public Transaction {
public:
    // shippedBy: the operator who loaded the truck; nullopt only for records from
    // before operators were kept. Throws std::invalid_argument for an id <= 0.
    StockShipment(Uuid id, TimePoint createdAt, std::string cropCode, Cargo cargo, TruckId truckId,
                  SyncStatus syncStatus = SyncStatus::Pending, std::optional<OperatorId> shippedBy = std::nullopt);

    // Prefer Journal::shipmentOf(), which splits the weight from the stock.
    [[nodiscard]] static StockShipment create(std::string cropCode, Cargo cargo, TruckId truckId,
                                              std::optional<OperatorId> by = std::nullopt);

    [[nodiscard]] const std::string& cropCode() const noexcept { return cropCode_; }
    [[nodiscard]] const Cargo& cargo() const noexcept { return cargo_; }
    [[nodiscard]] const TruckId& truckId() const noexcept { return truckId_; }
    [[nodiscard]] std::optional<OperatorId> shippedBy() const noexcept { return shippedBy_; }

    [[nodiscard]] TransactionKind kind() const noexcept override { return TransactionKind::Shipment; }
    // The supplier was settled with when the berries came in.
    [[nodiscard]] Money balanceEffect() const noexcept override { return Money{}; }
    [[nodiscard]] std::vector<std::string> validate() const override;
    [[nodiscard]] std::string describe() const override;

private:
    std::string cropCode_;
    Cargo cargo_;
    TruckId truckId_;
    std::optional<OperatorId> shippedBy_;
};

} // namespace wms
