#pragma once

#include "../ui/src/terminal.h"

#include <QString>

// The same day for both variants: yesterday's carried-over truck, a shipment from
// the warehouse, two pallets of a new receipt and one of them cancelled.
inline void playDemo(wms::ui::Terminal& t) {
    t.login(1, "1111");
    t.openSession();
    const QString truck = t.historyPlaces()[1].toMap()["placeId"].toString();
    t.shipFromStock("strawberry", "5", truck);
    t.setReceivingTarget(truck);
    t.startReceipt(43);
    t.recordWeighing("raspberry", "20", "0,3", "25", "70", true);
    t.recordWeighing("raspberry", "10", "0,3", "0", "30", true);
    t.cancelWeighing(1);
}
