pragma ComponentBehavior: Bound

import QtQuick
// Unused to qmllint, but Theme lives in the style that this import resolves to.
import QtQuick.Controls // qmllint disable unused-imports
import QtQuick.Layouts

// A table's column titles over a line. The screen describes the columns once and
// lays its rows out with the same widths, so a title never drifts from its column.
ColumnLayout {
    id: header

    // [{title, width, align, visible}]: width 0 or none takes the rest of the row,
    // align "right" for numbers, visible false hides the column.
    property var columns: []
    // A table with many narrow columns sets it tighter; its rows use the same.
    property int columnSpacing: Theme.spacingM

    function shown(column) { return column.visible === undefined || column.visible }
    function align(column) { return column.align === "right" ? Text.AlignRight : Text.AlignLeft }

    spacing: 0
    Layout.fillWidth: true

    RowLayout {
        spacing: header.columnSpacing
        Layout.fillWidth: true
        Layout.preferredHeight: 28

        Repeater {
            model: header.columns

            ColumnHeader {
                required property var modelData
                visible: header.shown(modelData)
                text: modelData.title
                horizontalAlignment: header.align(modelData)
                // Fixed columns never stretch: a wide cell would push the row away from the header.
                Layout.fillWidth: !modelData.width
                Layout.preferredWidth: modelData.width || 0
                Layout.maximumWidth: modelData.width || Number.POSITIVE_INFINITY
            }
        }
    }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 1
        color: Theme.border
    }
}
