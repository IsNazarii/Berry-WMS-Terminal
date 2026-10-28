pragma ComponentBehavior: Bound

import QtQuick
import BerryWms.Ui

// Three berries: the app's mark, drawn from the theme so it follows light and dark.
// On a dark ground pass a light `color`; the other two berries are shades of it.
Item {
    id: mark

    property color color: Theme.accent
    property real size: Theme.brandMarkSize
    readonly property real berry: size * 0.42

    implicitWidth: size
    implicitHeight: size

    component Berry: Rectangle {
        width: mark.berry
        height: mark.berry
        radius: width / 2
    }
    Berry { x: mark.size * 0.08; y: mark.size * 0.12; color: mark.color }
    Berry { x: mark.size * 0.5; y: mark.size * 0.12; color: Qt.lighter(mark.color, 1.35) }
    Berry { x: mark.size * 0.29; y: mark.size * 0.48; color: Qt.darker(mark.color, 1.25) }
}
