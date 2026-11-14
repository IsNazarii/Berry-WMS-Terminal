import QtQuick
import QtQuick.Controls

// An operator's initial in a circle: who is logged in on the bar, who to log in on the login cards.
Rectangle {
    id: avatar

    property string name
    property int size: Theme.avatarSize
    // Filled: the operator in charge (logged in, or picked on the login screen).
    property bool filled: true

    implicitWidth: size
    implicitHeight: size
    radius: size / 2
    color: filled ? Theme.accent : Theme.surfaceAlt

    Label {
        anchors.centerIn: parent
        text: avatar.name.charAt(0)
        font.pixelSize: avatar.size >= Theme.avatarSizeLarge ? Theme.fontSizeLarge : Theme.fontSizeBody
        font.weight: Font.DemiBold
        color: avatar.filled ? Theme.textOnAccent : Theme.accent
    }
}
