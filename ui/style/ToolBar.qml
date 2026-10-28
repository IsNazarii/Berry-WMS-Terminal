import QtQuick
import QtQuick.Templates as T

// The app bar over the screens: the surface color, split off by a hairline.
T.ToolBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    leftPadding: Theme.spacingL
    rightPadding: Theme.spacingL

    background: Rectangle {
        implicitHeight: Theme.appBarHeight
        color: Theme.surface

        Rectangle {
            width: parent.width
            height: 1
            y: control.position === T.ToolBar.Footer ? 0 : parent.height - 1
            color: Theme.border
        }
    }
}
