import QtQuick
import QtQuick.Templates as T

// Three looks: outlined (default), accent fill (`highlighted` — the one primary
// action of a panel) and text-only (`flat` — secondary, rarely used actions).
// WarningButton adds a fourth: outlined in the warning color.
T.Button {
    id: control

    // Set only by WarningButton; screens use that type instead.
    property bool warning: false

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: Theme.spacingS
    horizontalPadding: control.flat ? Theme.spacingS : Theme.spacingL
    font.weight: control.flat ? Font.Medium : Font.DemiBold
    // A disabled primary keeps full opacity and turns neutral: a washed-out accent looks broken.
    readonly property bool primaryDisabled: control.highlighted && !control.enabled
    opacity: control.enabled || control.primaryDisabled ? 1 : Theme.disabledOpacity

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.primaryDisabled ? Theme.textMuted
             : control.highlighted ? Theme.textOnAccent : control.warning ? Theme.warn
             : control.flat ? Theme.textMuted : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 64
        implicitHeight: control.flat ? 32 : Theme.controlHeight
        radius: Theme.radiusS
        color: {
            if (control.primaryDisabled)
                return Theme.surfaceAlt
            if (control.highlighted)
                return control.down ? Qt.darker(Theme.accent, 1.15)
                     : control.hovered ? Qt.lighter(Theme.accent, 1.1) : Theme.accent
            if (control.down)
                return Theme.border
            if (control.hovered || control.checked)
                return Theme.surfaceAlt
            return control.flat ? "transparent" : Theme.surface
        }
        // Keyboard focus must be visible: the operator works with Tab and Enter.
        border.width: control.visualFocus ? 2 : (control.flat || control.highlighted ? 0 : 1)
        border.color: control.visualFocus ? (control.highlighted ? Theme.text : Theme.accent)
                    : control.warning ? Theme.warn : Theme.border
    }
}
