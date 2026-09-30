import QtQuick
import QtQuick.Templates as T

// Inverted colors, so a hint never blends into the panel under it.
T.ToolTip {
    id: control

    x: parent ? (parent.width - implicitWidth) / 2 : 0
    y: -implicitHeight - Theme.spacingXs
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)
    margins: Theme.spacingS
    padding: Theme.spacingXs
    horizontalPadding: Theme.spacingS
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent | T.Popup.CloseOnReleaseOutsideParent

    contentItem: Text {
        text: control.text
        font.family: control.font.family
        font.pixelSize: Theme.fontSizeCaption
        wrapMode: Text.Wrap
        color: Theme.bg
    }

    background: Rectangle {
        color: Theme.text
        radius: Theme.radiusS
    }
}
