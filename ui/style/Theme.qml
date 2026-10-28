pragma Singleton

import QtQuick

// Design tokens: the only place with literal colors and sizes. Screens and
// controls refer to these names, so a restyle is an edit of this file only.
// Values come from the HTML mockup of the receiving screen.
QtObject {
    // Follows the system until the operator picks a theme explicitly.
    property bool dark: Application.styleHints.colorScheme === Qt.ColorScheme.Dark

    // Surfaces: cool greys leaning to the blueberry accent.
    readonly property color bg: dark ? "#14141C" : "#F3F2F7"
    readonly property color surface: dark ? "#1D1D28" : "#FFFFFF"
    readonly property color surfaceAlt: dark ? "#262634" : "#E9E8F1"
    readonly property color border: dark ? "#353547" : "#D3D1E0"
    readonly property color text: dark ? "#E9E8F2" : "#1C1B2B"
    readonly property color textMuted: dark ? "#9C9AB3" : "#626079"

    // The one accent: primary actions and selection.
    readonly property color accent: dark ? "#8F95F2" : "#3A3F94"
    readonly property color accentSoft: dark ? "#2B2D52" : "#E2E3F6"
    readonly property color textOnAccent: dark ? "#12132A" : "#FFFFFF"

    // State colors are separate from the accent: good / attention / error.
    readonly property color ok: dark ? "#5CCB8A" : "#1E7A45"
    readonly property color warn: dark ? "#F0A63C" : "#B45F06"
    readonly property color danger: dark ? "#F2807A" : "#B3261E"
    readonly property color okSoft: dark ? "#1E3A2A" : "#DDF1E5"
    readonly property color warnSoft: dark ? "#3A2E1C" : "#FDF3E6"
    readonly property color dangerSoft: dark ? "#3A1F22" : "#FBE7E5"

    // Dims the window behind a modal dialog.
    // The brand panel of the login screen: deep blueberry in both themes.
    readonly property color brandPanel: dark ? "#23254A" : "#2A2E70"
    readonly property color brandPanelText: dark ? "#E9E8F2" : "#FFFFFF"
    readonly property color brandPanelMuted: dark ? "#9C9AB3" : "#C9CBF2"
    // Left, right and bottom berry of the mark on the brand panel: light shades of the accent.
    readonly property list<color> brandBerriesOnPanel: ["#8F95F2", "#B9BCF7", "#E2E3F6"]
    readonly property color brandStemOnPanel: "#7CF0B0"

    readonly property color overlay: dark ? "#99000000" : "#661C1B2B"

    // The scale indicator is an instrument, dark in both themes.
    readonly property color scaleBg: dark ? "#0B0B10" : "#15161F"
    readonly property color scaleDigit: "#7CF0B0"
    readonly property color scaleDigitUnstable: "#F5B94A"
    readonly property color scaleDim: dark ? "#2A2C3A" : "#3A3D4F"

    // A cancelled weighing stays visible, faded.
    readonly property real cancelledOpacity: 0.45
    readonly property real disabledOpacity: 0.4

    readonly property int spacingXs: 4
    readonly property int spacingS: 8
    readonly property int spacingM: 12
    readonly property int spacingL: 16
    readonly property int spacingXl: 24

    readonly property int radiusS: 4
    readonly property int radiusM: 8

    readonly property int controlHeight: 40
    readonly property int controlHeightLarge: 48
    readonly property int controlHeightXl: 56
    // A pickable card (an operator on the login screen).
    readonly property int tileHeight: 80
    // A lone form in the middle of a screen (login) does not stretch wider than this.
    readonly property int formMaxWidth: 520
    readonly property int loginPanelMinWidth: 340

    // The app bar over every screen: brand, navigation, session and operator.
    readonly property int appBarHeight: 56
    readonly property int iconSize: 18
    readonly property int iconSizeSmall: 14
    readonly property int badgeSize: 18
    readonly property int avatarSize: 32
    readonly property int avatarSizeLarge: 48
    readonly property int statusDotSize: 8
    // The accent bar on the left of a selected list row.
    readonly property int selectionBarWidth: 3
    readonly property int brandMarkSize: 28
    readonly property int brandMarkSizeLarge: 40
    // The round badge of an empty state and the icon in it.
    readonly property int emptyBadgeSize: 72
    readonly property int iconSizeLarge: 34

    // A monochrome icon from ui/style/icons; controls tint it with their text color.
    function icon(name: string): url {
        return Qt.resolvedUrl("icons/" + name + ".svg")
    }

    // Onest ships with the app (ui/style/fonts): the terminal must look the same on
    // any laptop. One loader per weight, all under the family "Onest".
    readonly property FontLoader onestRegular: FontLoader { source: "fonts/Onest-Regular.ttf" }
    readonly property FontLoader onestMedium: FontLoader { source: "fonts/Onest-Medium.ttf" }
    readonly property FontLoader onestSemiBold: FontLoader { source: "fonts/Onest-SemiBold.ttf" }
    readonly property FontLoader onestBold: FontLoader { source: "fonts/Onest-Bold.ttf" }
    readonly property string fontUi: onestRegular.font.family
    // Tabular figures: every digit is equally wide, so weights and sums line up
    // in columns and the scale readout does not jitter.
    readonly property var fontFeatures: ({ "tnum": 1 })

    readonly property int fontSizeCaption: 12
    readonly property int fontSizeBody: 14
    readonly property int fontSizeField: 16
    readonly property int fontSizeTitle: 18
    readonly property int fontSizeLarge: 20
    readonly property int fontSizeHeading: 24
    readonly property int fontSizeDisplay: 36
    readonly property int fontSizeScale: 72
    readonly property int fontSizeScaleCompact: 48
}
