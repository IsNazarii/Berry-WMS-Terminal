#pragma once

#include "wms/quantity.h"

#include <QString>

namespace wms::ui {

// Numbers as the operator reads them on screen: decimal comma, thin spaces
// between thousands, no unit (the column header or label carries it).
// The core's formatMoney/formatWeight stay for messages and logs.

// Kilograms with as many decimals as the grams need, at least one: "142,6", "16,45".
[[nodiscard]] QString kilogramsText(Weight weight);
// Hryvnias with kopecks: "8 823,00".
[[nodiscard]] QString hryvniasText(Money money);
// "1 палета", "3 палети", "5 палет": Ukrainian plural by the last digits.
[[nodiscard]] QString palletsText(int count);

} // namespace wms::ui
