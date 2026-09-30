import QtQuick
import QtQuick.Templates as T

// A panel or a card: surface color, hairline border, the larger radius.
T.Frame {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: Theme.spacingM

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusM
        border.color: Theme.border
    }
}
