import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Where the next pallet or boxes go: picked once in the list on the right,
// repeated here next to the buttons that send them.
Label {
    id: banner

    // A place is picked: the strip turns accent.
    property bool active: false
    // Something stops sending at all (no session): unlike "pick a place", a real obstacle.
    property bool warning: false

    font.weight: active ? Font.DemiBold : Font.Normal
    color: warning ? Theme.warn : active ? Theme.accent : Theme.textMuted
    elide: Text.ElideRight
    padding: Theme.spacingS
    Layout.fillWidth: true
    background: Rectangle {
        radius: Theme.radiusS
        color: banner.active ? Theme.accentSoft : Theme.surfaceAlt
    }
}
