import QtQuick
// Unused to qmllint, but Theme lives in the style that this import resolves to.
import QtQuick.Controls // qmllint disable unused-imports

// One row of a table: lit under the pointer, a faint line under it. The cells
// are the screen's own (usually a RowLayout filling the row).
Rectangle {
    implicitHeight: 48
    color: hover.hovered ? Theme.surfaceAlt : "transparent"

    HoverHandler { id: hover }

    // Above the cells, so a field reaching the bottom does not hide it.
    Rectangle {
        z: 1
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.surfaceAlt
    }
}
