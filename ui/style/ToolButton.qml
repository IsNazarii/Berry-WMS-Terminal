import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T

// Small button for bars and row actions (✕, the app bar menu): text, an icon or both.
T.ToolButton {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: Theme.spacingS
    spacing: Theme.spacingS
    font.weight: Font.Medium
    opacity: control.enabled ? 1 : Theme.disabledOpacity
    icon.width: Theme.iconSize
    icon.height: Theme.iconSize
    icon.color: control.hovered || control.checked ? Theme.text : Theme.textMuted

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        icon: control.icon
        text: control.text
        font: control.font
        color: control.icon.color
    }

    background: Rectangle {
        implicitWidth: 32
        implicitHeight: 32
        radius: Theme.radiusS
        color: control.down ? Theme.border : control.hovered || control.checked ? Theme.surfaceAlt : "transparent"
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
    }
}
