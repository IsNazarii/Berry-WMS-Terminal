import QtQuick
import QtQuick.Templates as T

T.Popup {
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

    T.Overlay.modal: Rectangle { color: Theme.overlay }
    T.Overlay.modeless: Rectangle { color: "transparent" }
}
