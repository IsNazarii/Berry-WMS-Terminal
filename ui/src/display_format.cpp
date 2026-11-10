#include "display_format.h"

#include "wms/display_format.h"

namespace wms::ui {

QString kilogramsText(Weight weight) { return QString::fromStdString(displayKilograms(weight)); }

QString hryvniasText(Money money) { return QString::fromStdString(displayHryvnias(money)); }

QString palletsText(int count) {
    const int lastTwo = count % 100;
    const int last = count % 10;
    const char* word = "палет";
    if (last == 1 && lastTwo != 11) {
        word = "палета";
    } else if (last >= 2 && last <= 4 && (lastTwo < 12 || lastTwo > 14)) {
        word = "палети";
    }
    return QString("%1 %2").arg(count).arg(QString::fromUtf8(word));
}

} // namespace wms::ui
