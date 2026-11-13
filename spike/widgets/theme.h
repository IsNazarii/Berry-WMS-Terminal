#pragma once

#include <QColor>
#include <QPixmap>
#include <QString>

// Shared by every Widgets screen: the same tokens as ui/style/Theme.qml, the
// stylesheet built from them, the Onest font and tinted icons.
namespace spike {

struct Theme {
    QColor bg, surface, surfaceAlt, border, text, textMuted, accent, accentSoft, warn;

    [[nodiscard]] static Theme light();
    [[nodiscard]] static Theme dark();
};

inline constexpr int kSpacingXs = 4;
inline constexpr int kSpacingS = 8;
inline constexpr int kSpacingM = 12;
inline constexpr int kSpacingXl = 24;
inline constexpr int kRadiusS = 4;
inline constexpr int kRadiusM = 8;
inline constexpr int kSelectionBarWidth = 3;
inline constexpr int kFontCaption = 12;
inline constexpr int kFontBody = 14;
inline constexpr int kFontTitle = 18;
inline constexpr int kFontLarge = 20;
inline constexpr int kEmptyBadgeSize = 72;
inline constexpr int kIconSizeLarge = 34;

// The current theme; delegates read it while painting, so a switch needs only a repaint.
[[nodiscard]] const Theme& theme();
// Applies the tokens and the stylesheet to the whole application.
void setTheme(bool dark);
void loadFonts();
// A monochrome icon from ui/style/icons, painted in `color`.
[[nodiscard]] QPixmap tintedIcon(const QString& name, int size, const QColor& color);

} // namespace spike
