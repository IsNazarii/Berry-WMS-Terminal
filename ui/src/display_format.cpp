#include "display_format.h"

namespace wms::ui {
namespace {

// Groups the whole part by thousands with a narrow no-break space, the Ukrainian convention.
QString groupThousands(std::int64_t whole) {
    QString digits = QString::number(whole);
    for (auto i = digits.size() - 3; i > 0; i -= 3) digits.insert(i, QChar(0x202F));
    return digits;
}

QString fixed(std::int64_t raw, std::int64_t scale, int decimals, int minDecimals) {
    const bool negative = raw < 0;
    const std::int64_t value = negative ? -raw : raw;
    QString fraction = QString::number(value % scale).rightJustified(decimals, QChar('0'));
    while (fraction.size() > minDecimals && fraction.endsWith(QChar('0'))) fraction.chop(1);
    QString text = groupThousands(value / scale);
    if (!fraction.isEmpty()) text += QChar(',') + fraction;
    return negative ? QChar(0x2212) + text : text;
}

} // namespace

QString kilogramsText(Weight weight) { return fixed(weight.raw(), 1000, 3, 1); }

QString hryvniasText(Money money) { return fixed(money.raw(), 100, 2, 2); }

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
