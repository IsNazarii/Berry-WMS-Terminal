pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A read-only table: a header and a scrolling list whose columns line up with it.
// The screen describes the columns and what each row shows; widths and alignment
// live in one place, so a header can never drift from its column.
ColumnLayout {
    id: table

    // [{title, width, align, visible}]: width 0 or none takes the rest of the row,
    // align "right" for numbers, visible false hides the column.
    property var columns: []
    property alias model: list.model
    // One cell per column for a row: {text, muted, struck, bold, note}. `note` is a
    // small warning line under the text; `struck` crosses the text out and fades it.
    property var cellsOf: row => []
    readonly property alias count: list.count
    // Shown instead of the rows while there are none (an EmptyState).
    default property alias placeholder: placeholderArea.data

    spacing: 0

    function shown(column) { return column.visible === undefined || column.visible }
    function align(column) { return column.align === "right" ? Text.AlignRight : Text.AlignLeft }

    RowLayout {
        visible: list.count > 0
        spacing: Theme.spacingM
        Layout.fillWidth: true
        Layout.preferredHeight: 28

        Repeater {
            model: table.columns

            ColumnHeader {
                required property var modelData
                visible: table.shown(modelData)
                text: modelData.title
                horizontalAlignment: table.align(modelData)
                // Fixed columns never stretch: a wide cell would push the row away from the header.
                Layout.fillWidth: !modelData.width
                Layout.preferredWidth: modelData.width || 0
                Layout.maximumWidth: modelData.width || Number.POSITIVE_INFINITY
            }
        }
    }
    Rectangle {
        visible: list.count > 0
        Layout.fillWidth: true
        implicitHeight: 1
        color: Theme.border
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        ListView {
            id: list
            anchors.fill: parent
            clip: true
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: row
                required property var modelData
                readonly property var cells: table.cellsOf(modelData)
                width: ListView.view.width
                implicitHeight: 48
                color: hover.hovered ? Theme.surfaceAlt : "transparent"

                HoverHandler { id: hover }

                RowLayout {
                    anchors.fill: parent
                    spacing: Theme.spacingM

                    Repeater {
                        model: table.columns

                        ColumnLayout {
                            id: cell
                            required property var modelData
                            required property int index
                            readonly property var spec: row.cells[index] ?? {}
                            visible: table.shown(modelData)
                            spacing: 0
                            Layout.fillWidth: !modelData.width
                            Layout.preferredWidth: modelData.width || 0
                            Layout.maximumWidth: modelData.width || Number.POSITIVE_INFINITY

                            Label {
                                text: cell.spec.text ?? ""
                                color: cell.spec.struck || cell.spec.muted ? Theme.textMuted : Theme.text
                                font.strikeout: cell.spec.struck ?? false
                                font.weight: cell.spec.bold ? Font.DemiBold : Font.Normal
                                horizontalAlignment: table.align(cell.modelData)
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                visible: !!cell.spec.note
                                text: cell.spec.note ?? ""
                                font.pixelSize: Theme.fontSizeCaption
                                color: Theme.warn
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.surfaceAlt
                }
            }
        }
        Item {
            id: placeholderArea
            anchors.fill: parent
            visible: list.count === 0
        }
    }
}
