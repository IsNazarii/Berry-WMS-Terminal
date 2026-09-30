import QtQuick
import QtQuick.Templates as T

// Small text-only button for bars and row actions (✕, «Вийти»).
T.ToolButton {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: Theme.spacingS
    font.weight: Font.Medium
    opacity: control.enabled ? 1 : Theme.disabledOpacity

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.hovered || control.checked ? Theme.text : Theme.textMuted
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
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
