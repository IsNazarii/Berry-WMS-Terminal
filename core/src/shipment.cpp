#include "wms/shipment.h"

#include <format>
#include <utility>

namespace wms {

StockShipment::StockShipment(Uuid id, TimePoint createdAt, std::string cropCode, Cargo cargo, TruckId truckId,
                             SyncStatus syncStatus)
    : Transaction{id, createdAt, syncStatus}, cropCode_{std::move(cropCode)}, cargo_{cargo}, truckId_{truckId} {}

StockShipment StockShipment::create(std::string cropCode, Cargo cargo, TruckId truckId) {
    return StockShipment{Uuid::generateV7(), Clock::now(), std::move(cropCode), cargo, truckId};
}

std::vector<std::string> StockShipment::validate() const {
    std::vector<std::string> errors;
    if (cropCode_.empty()) errors.emplace_back("Не обрано культуру");
    if (truckId_.isNil()) errors.emplace_back("Не обрано фуру");
    if (cargo_.boxes <= 0) errors.emplace_back("Кількість ящиків має бути більшою за нуль");
    if (!cargo_.weight.isPositive()) errors.emplace_back("Вага має бути більшою за нуль");
    return errors;
}

std::string StockShipment::describe() const {
    return std::format("Відвантаження зі складу {}: {} ящ., {} → фура {}", cropCode_, cargo_.boxes,
                       formatWeight(cargo_.weight), truckId_.toString());
}

} // namespace wms
