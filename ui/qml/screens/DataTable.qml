pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A read-only table: a header and a scrolling list whose columns line up with it.
// The screen describes the columns and what each row shows; widths and alignment
// live in one place, so a header can never drift from its column.
ColumnLayout {
    id: table

    // As in TableHeader: [{title, width, align, visible}].
    property var columns: []
    property alias model: list.model
    // One cell per column for a row: {text, muted, struck, bold, note}. `note` is a
    // small warning line under the text; `struck` crosses the text out and fades it.
    property var cellsOf: row => []
    readonly property alias count: list.count
    // Shown instead of the rows while there are none (an EmptyState).
    default property alias placeholder: placeholderArea.data

    spacing: 0

    TableHeader {
        id: header
        visible: list.count > 0
        columns: table.columns
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
        // Rows start a step below the header line, as a panel's blocks do.
        Layout.topMargin: list.count > 0 ? Theme.spacingM : 0

        ListView {
            id: list
            anchors.fill: parent
            clip: true
            ScrollBar.vertical: ScrollBar {}

            delegate: TableRow {
                id: row
                required property var modelData
                readonly property var cells: table.cellsOf(modelData)
                width: ListView.view.width

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
                            visible: header.shown(modelData)
                            spacing: 0
                            Layout.fillWidth: !modelData.width
                            Layout.preferredWidth: modelData.width || 0
                            Layout.maximumWidth: modelData.width || Number.POSITIVE_INFINITY

                            Label {
                                text: cell.spec.text ?? ""
                                color: cell.spec.struck || cell.spec.muted ? Theme.textMuted : Theme.text
                                font.strikeout: cell.spec.struck ?? false
                                font.weight: cell.spec.bold ? Font.DemiBold : Font.Normal
                                horizontalAlignment: header.align(cell.modelData)
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
            }
        }
        Item {
            id: placeholderArea
            anchors.fill: parent
            visible: list.count === 0
        }
    }
}
