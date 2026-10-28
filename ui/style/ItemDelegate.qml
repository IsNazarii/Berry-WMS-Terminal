import QtQuick
import QtQuick.Templates as T

// A row of a list: open receipts, suppliers, debtors. `highlighted` marks the
// selected row with the soft accent and an accent bar on the left, so the
// active receipt is found at a glance.
T.ItemDelegate {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: Theme.spacingS
    horizontalPadding: Theme.spacingM
    opacity: control.enabled ? 1 : Theme.disabledOpacity

    contentItem: Text {
        text: control.text
        font: control.font
        color: Theme.text
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 100
        implicitHeight: Theme.controlHeight
        radius: Theme.radiusS
        color: control.highlighted ? Theme.accentSoft
             : control.down || control.hovered || control.visualFocus ? Theme.surfaceAlt : "transparent"

        Rectangle {
            visible: control.highlighted
            width: Theme.selectionBarWidth
            height: parent.height
            radius: width / 2
            color: Theme.accent
        }
    }
}
