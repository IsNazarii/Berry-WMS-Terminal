import QtQuick
import QtQuick.Templates as T

// The current screen is marked by an accent line under the tab, not by a fill.
T.TabButton {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: Theme.spacingS
    horizontalPadding: Theme.spacingXl
    font.weight: control.checked ? Font.DemiBold : Font.Medium

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.checked || control.hovered ? Theme.text : Theme.textMuted
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitHeight: Theme.controlHeightLarge
        color: control.down ? Theme.surfaceAlt : "transparent"
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent

        Rectangle {
            width: parent.width
            height: 3
            anchors.bottom: parent.bottom
            visible: control.checked
            color: Theme.accent
        }
    }
}
