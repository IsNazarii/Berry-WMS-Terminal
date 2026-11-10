#pragma once

#include "wms/quantity.h"

#include <QString>

namespace wms::ui {

// The core's displayKilograms/displayHryvnias as QStrings for the screens.
[[nodiscard]] QString kilogramsText(Weight weight);
[[nodiscard]] QString hryvniasText(Money money);
// "1 палета", "3 палети", "5 палет": Ukrainian plural by the last digits.
[[nodiscard]] QString palletsText(int count);

} // namespace wms::ui
