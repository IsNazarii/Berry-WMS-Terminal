import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Why the last action was refused, on a soft red background; hidden while empty.
Label {
    visible: text !== ""
    color: Theme.danger
    wrapMode: Text.WordWrap
    padding: Theme.spacingS
    Layout.fillWidth: true
    background: Rectangle {
        color: Theme.dangerSoft
        radius: Theme.radiusS
    }
}
