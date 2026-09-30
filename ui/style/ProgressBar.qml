import QtQuick
import QtQuick.Templates as T

// A truck's load. Full is not an error by itself, so the bar stays accent;
// screens say "overloaded" in words and color.
T.ProgressBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    contentItem: Item {
        implicitWidth: 120
        implicitHeight: 6

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: height / 2
            color: Theme.accent
        }
    }

    background: Rectangle {
        implicitWidth: 120
        implicitHeight: 6
        y: (control.height - height) / 2
        height: 6
        radius: height / 2
        color: Theme.surfaceAlt
    }
}
