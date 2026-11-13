#include "theme.h"

#include <QApplication>
#include <QFontDatabase>
#include <QIcon>
#include <QPainter>

namespace spike {

namespace {

Theme current = Theme::light();

QString hex(const QColor& c) { return c.name(QColor::HexRgb); }

// QSS has no tokens: the stylesheet is text with the colors filled in.
QString styleSheet(const Theme& t) {
    return QString(R"(
QWidget { font-family: "Onest"; font-size: %1px; color: %2; }
QWidget#window { background: %3; }
QFrame#panel { background: %4; border: 1px solid %5; border-radius: %6px; }
QLabel { background: transparent; }
QLabel[role="caption"] { font-size: %7px; font-weight: 600; color: %8; }
QLabel[role="muted"] { font-size: %7px; color: %8; }
QLabel[role="title"] { font-size: %9px; font-weight: 700; }
QLabel[role="emptyTitle"] { font-size: %10px; font-weight: 700; }
QLabel[role="emptyText"] { color: %8; }
QComboBox { background: %4; border: 1px solid %5; border-radius: %11px; padding: 0 12px; min-height: 38px; }
QComboBox:hover { background: %12; }
QComboBox:focus, QComboBox:on { border: 2px solid %13; }
QComboBox::drop-down { border: none; width: 28px; }
QComboBox::down-arrow { image: url(%14); }
QComboBox QAbstractItemView { background: %4; border: 1px solid %5; border-radius: %6px; padding: 4px;
                              selection-background-color: %15; selection-color: %2; outline: 0; }
QListView, QTableView { background: transparent; border: none; outline: 0; }
QHeaderView { background: transparent; }
QHeaderView::section { background: transparent; border: none; border-bottom: 1px solid %5; color: %8;
                       font-size: 11px; font-weight: 600; padding: 0 6px; }
QScrollBar:vertical { background: transparent; width: 8px; }
QScrollBar::handle:vertical { background: %5; border-radius: 4px; min-height: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
)")
        .arg(kFontBody).arg(hex(t.text), hex(t.bg), hex(t.surface), hex(t.border)).arg(kRadiusM).arg(kFontCaption)
        .arg(hex(t.textMuted)).arg(kFontTitle).arg(kFontLarge).arg(kRadiusS)
        .arg(hex(t.surfaceAlt), hex(t.accent), QStringLiteral(WMS_SPIKE_DIR "/down.svg"), hex(t.accentSoft));
}

} // namespace

Theme Theme::light() {
    return {.bg = "#F3F2F7", .surface = "#FFFFFF", .surfaceAlt = "#E9E8F1", .border = "#D3D1E0",
            .text = "#1C1B2B", .textMuted = "#626079", .accent = "#3A3F94", .accentSoft = "#E2E3F6",
            .warn = "#B45F06"};
}

Theme Theme::dark() {
    return {.bg = "#14141C", .surface = "#1D1D28", .surfaceAlt = "#262634", .border = "#353547",
            .text = "#E9E8F2", .textMuted = "#9C9AB3", .accent = "#8F95F2", .accentSoft = "#2B2D52",
            .warn = "#F0A63C"};
}

const Theme& theme() { return current; }

void setTheme(bool dark) {
    current = dark ? Theme::dark() : Theme::light();
    qApp->setStyleSheet(styleSheet(current));
}

void loadFonts() {
    for (const char* face : {"Regular", "Medium", "SemiBold", "Bold"})
        QFontDatabase::addApplicationFont(QString(WMS_FONT_DIR "/Onest-%1.ttf").arg(face));
}

QPixmap tintedIcon(const QString& name, int size, const QColor& color) {
    QPixmap pixmap = QIcon(QString(WMS_ICON_DIR "/%1.svg").arg(name)).pixmap(size, size);
    // The icons are drawn black; keep their shape, take the color.
    QPainter painter{&pixmap};
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);
    return pixmap;
}

} // namespace spike
