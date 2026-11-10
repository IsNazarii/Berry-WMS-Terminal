#pragma once

#include "wms/quantity.h"

#include <string>

namespace wms {

// Numbers as people read them, on screen and on the printed receipt: decimal
// comma, narrow no-break spaces between thousands, a true minus, no unit (the
// column header or label carries it). UTF-8. formatMoney/formatWeight stay for
// messages and logs.

// Kilograms with as many decimals as the grams need, at least one: "142,6", "16,45".
[[nodiscard]] std::string displayKilograms(Weight weight);
// Hryvnias with kopecks: "8 823,00".
[[nodiscard]] std::string displayHryvnias(Money money);

} // namespace wms
