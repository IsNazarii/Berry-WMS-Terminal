import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T

// A navigation pill in the app bar: icon, name, the key that opens the screen
// and a counter of what waits there. The current screen is filled with the soft accent.
T.TabButton {
    id: control

    // The key that opens the tab ("F6"), shown muted after the name.
    property string hint
    // Things waiting on that screen; 0 hides the badge.
    property int badge: 0
    // A narrow window keeps the icon and the name only.
    property bool compact: false

    // An explicit width stops TabBar from stretching every tab to the same size.
    width: implicitWidth
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: Theme.spacingS
    horizontalPadding: Theme.spacingM
    spacing: Theme.spacingS
    font.weight: control.checked ? Font.DemiBold : Font.Medium
    icon.width: Theme.iconSize
    icon.height: Theme.iconSize
    icon.color: control.checked ? Theme.accent : control.hovered ? Theme.text : Theme.textMuted

    contentItem: Row {
        spacing: control.spacing

        IconImage {
            anchors.verticalCenter: parent.verticalCenter
            visible: control.icon.source.toString() !== ""
            source: control.icon.source
            sourceSize: Qt.size(control.icon.width, control.icon.height)
            color: control.icon.color
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            font: control.font
            color: control.icon.color
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: control.hint !== "" && !control.compact
            text: control.hint
            font.family: control.font.family
            font.pixelSize: Theme.fontSizeCaption
            color: Theme.textMuted
        }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            visible: control.badge > 0
            width: Math.max(Theme.badgeSize, badgeText.implicitWidth + Theme.spacingS)
            height: Theme.badgeSize
            radius: height / 2
            color: Theme.warn

            Text {
                id: badgeText
                anchors.centerIn: parent
                text: control.badge
                font.family: control.font.family
                font.pixelSize: Theme.fontSizeCaption
                font.weight: Font.Bold
                color: Theme.textOnAccent
            }
        }
    }

    background: Rectangle {
        implicitHeight: Theme.controlHeight
        radius: Theme.radiusM
        color: control.checked ? Theme.accentSoft
             : control.down ? Theme.border
             : control.hovered ? Theme.surfaceAlt : "transparent"
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
    }
}
