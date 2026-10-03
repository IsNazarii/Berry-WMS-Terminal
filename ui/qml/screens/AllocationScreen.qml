pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BerryWms.Ui

// Where the berries go: receipts with something left on the left, the chosen
// receipt in the middle, trucks and the warehouse on the right. The operator
// picks a destination once and then sends pallets or boxes into it.
Page {
    id: screen

    // No session: the main window switches to the session screen.
    signal sessionRequested()
    title: qsTr("Розподіл по фурах")

    // A truck id or "warehouse"; kept in QML, the core checks it on every allocation.
    property string destinationId: ""
    property string destinationTitle: ""
    readonly property bool hasReceipt: Terminal.allocationReceiptId !== ""
    readonly property bool hasDestination: destinationId !== ""
    readonly property var source: Terminal.allocationSources.find(r => r.receiptId === Terminal.allocationReceiptId)

    // "40 / 400 ящ.": the load over its limit, or the load alone when there is none.
    function loadText(load, limit, unit) {
        return (limit !== "" ? load + " / " + limit : load) + " " + unit
    }

    // A trace of the table rows, as on the receiving screen.
    component RowDivider: Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.surfaceAlt
    }

    NoSessionNotice {
        visible: !Terminal.sessionOpen
        text: qsTr("Розподіл можливий лише у відкритій сесії.")
        onSessionRequested: screen.sessionRequested()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingM
        visible: Terminal.sessionOpen
        spacing: Theme.spacingM

        // Receipts that still have boxes to allocate: open ones first, pallets go out while unloading.
        Frame {
            Layout.preferredWidth: 220
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacingM

                SectionCaption { text: qsTr("Чеки до розподілу") }

                ListView {
                    id: sourceList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: Theme.spacingXs
                    model: Terminal.allocationSources

                    Label {
                        anchors.centerIn: parent
                        visible: sourceList.count === 0
                        text: qsTr("Усе розподілено")
                        color: Theme.textMuted
                    }

                    delegate: ItemDelegate {
                        id: sourceRow
                        required property var modelData
                        width: ListView.view.width
                        highlighted: modelData.receiptId === Terminal.allocationReceiptId
                        onClicked: {
                            Terminal.selectAllocationReceipt(modelData.receiptId)
                            allocationError.text = ""
                        }

                        contentItem: ColumnLayout {
                            spacing: 2
                            Label {
                                text: sourceRow.modelData.supplierName
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            RowLayout {
                                Label {
                                    text: sourceRow.modelData.openedAt + " · "
                                          + (sourceRow.modelData.open ? qsTr("зважується") : qsTr("закритий"))
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: Theme.textMuted
                                }
                                Label {
                                    text: qsTr("%1 ящ.").arg(sourceRow.modelData.leftBoxes)
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: Theme.textMuted
                                    horizontalAlignment: Text.AlignRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }
            }
        }

        // The chosen receipt: whole pallets or a number of boxes of a crop.
        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacingM

                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true

                    Label {
                        text: screen.hasReceipt ? Terminal.allocationSupplierName : qsTr("Чек не обрано")
                        font.pixelSize: Theme.fontSizeTitle
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: !screen.hasReceipt ? qsTr("Оберіть чек зліва")
                              : screen.source ? qsTr("Відкрито %1 · %2 · лишилось %3 ящ., %4 кг")
                                                .arg(screen.source.openedAt)
                                                .arg(screen.source.open ? qsTr("зважується") : qsTr("закритий"))
                                                .arg(screen.source.leftBoxes).arg(screen.source.leftKg)
                              : qsTr("Усе розподілено")
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.textMuted
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                // Where the next pallet goes: picked once on the right, shown here where the buttons are.
                Label {
                    text: screen.hasDestination ? qsTr("Куди: %1").arg(screen.destinationTitle)
                                                : qsTr("Оберіть фуру або склад праворуч")
                    font.weight: Font.DemiBold
                    color: screen.hasDestination ? Theme.accent : Theme.warn
                    elide: Text.ElideRight
                    padding: Theme.spacingS
                    Layout.fillWidth: true
                    background: Rectangle {
                        radius: Theme.radiusS
                        color: screen.hasDestination ? Theme.accentSoft : Theme.surfaceAlt
                    }
                }

                SectionCaption {
                    visible: screen.hasReceipt
                    text: qsTr("Палетами")
                }

                ListView {
                    id: lineList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: Terminal.allocationLines
                    headerPositioning: ListView.OverlayHeader
                    ScrollBar.vertical: ScrollBar {}

                    // Column widths shared by the header and the rows; the crop takes the rest.
                    readonly property int indexWidth: 20
                    readonly property int boxesWidth: 40
                    readonly property int netWidth: 72
                    readonly property int placeWidth: 150
                    readonly property int columnSpacing: Theme.spacingM

                    header: Rectangle {
                        z: 2
                        width: ListView.view.width
                        height: 28
                        visible: lineList.count > 0
                        color: Theme.surface

                        RowLayout {
                            anchors.fill: parent
                            spacing: lineList.columnSpacing
                            ColumnHeader { text: qsTr("№"); horizontalAlignment: Text.AlignLeft; Layout.preferredWidth: lineList.indexWidth }
                            ColumnHeader { text: qsTr("Культура"); horizontalAlignment: Text.AlignLeft; Layout.fillWidth: true }
                            ColumnHeader { text: qsTr("Ящ."); Layout.preferredWidth: lineList.boxesWidth }
                            ColumnHeader { text: qsTr("Нетто, кг"); Layout.preferredWidth: lineList.netWidth }
                            ColumnHeader { text: qsTr("Де"); Layout.preferredWidth: lineList.placeWidth }
                        }
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: Theme.border
                        }
                    }

                    delegate: Rectangle {
                        id: line
                        required property var modelData
                        readonly property bool free: modelData.placedIn === "" && !modelData.cancelled
                        width: ListView.view.width
                        height: 44
                        color: rowHover.hovered ? Theme.surfaceAlt : "transparent"

                        HoverHandler { id: rowHover }

                        // A cancelled line stays visible, struck through, as on the receiving screen.
                        component Cell: Label {
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                            font.strikeout: line.modelData.cancelled
                            color: line.modelData.cancelled ? Theme.textMuted : Theme.text
                        }

                        RowLayout {
                            anchors.fill: parent
                            spacing: lineList.columnSpacing

                            Cell {
                                text: line.modelData.index + 1
                                color: Theme.textMuted
                                horizontalAlignment: Text.AlignLeft
                                Layout.preferredWidth: lineList.indexWidth
                            }
                            Cell {
                                text: line.modelData.crop
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignLeft
                                Layout.fillWidth: true
                            }
                            Cell {
                                text: line.modelData.boxes
                                Layout.preferredWidth: lineList.boxesWidth
                            }
                            Cell {
                                text: line.modelData.net
                                Layout.preferredWidth: lineList.netWidth
                            }
                            // A free pallet is one press away; a placed one says where it went.
                            Item {
                                Layout.preferredWidth: lineList.placeWidth
                                Layout.minimumWidth: lineList.placeWidth
                                Layout.maximumWidth: lineList.placeWidth
                                Layout.fillHeight: true

                                Button {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width
                                    visible: line.free
                                    enabled: screen.hasDestination
                                    text: qsTr("Палету →")
                                    onClicked: allocationError.text =
                                               Terminal.allocateWeighing(line.modelData.index, screen.destinationId)
                                }
                                Label {
                                    anchors.fill: parent
                                    visible: !line.free
                                    text: line.modelData.cancelled ? qsTr("анульовано") : line.modelData.placedIn
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: Theme.textMuted
                                    elide: Text.ElideRight
                                    horizontalAlignment: Text.AlignRight
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                        RowDivider {}
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: Theme.border
                    visible: cropRepeater.count > 0
                }
                SectionCaption {
                    visible: cropRepeater.count > 0
                    text: qsTr("Ящиками — вага пропорційно")
                }
                // Rebuilt after every allocation, which also clears the typed count: that is wanted here.
                Repeater {
                    id: cropRepeater
                    model: Terminal.allocationCrops

                    RowLayout {
                        id: cropRow
                        required property var modelData
                        spacing: Theme.spacingM
                        Layout.fillWidth: true

                        function send() {
                            allocationError.text = Terminal.allocateBoxes(modelData.cropCode, boxes.text,
                                                                          screen.destinationId)
                        }

                        Label {
                            text: cropRow.modelData.name
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: qsTr("лишилось %1 ящ. · %2 кг").arg(cropRow.modelData.leftBoxes).arg(cropRow.modelData.leftKg)
                            font.pixelSize: Theme.fontSizeCaption
                            color: Theme.textMuted
                        }
                        TextField {
                            id: boxes
                            placeholderText: qsTr("ящиків")
                            Layout.preferredWidth: 90
                            inputMethodHints: Qt.ImhDigitsOnly
                            validator: IntValidator { bottom: 1; top: cropRow.modelData.leftBoxes }
                            onAccepted: cropRow.send()
                        }
                        Button {
                            text: qsTr("Відвантажити")
                            enabled: screen.hasDestination && boxes.text !== ""
                            onClicked: cropRow.send()
                        }
                    }
                }

                Label {
                    id: allocationError
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
            }
        }

        // Destinations: trucks of the session still loading, then the warehouse.
        Frame {
            Layout.preferredWidth: 260
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacingM

                SectionCaption { text: qsTr("Куди") }

                ListView {
                    id: destinationList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: Theme.spacingXs
                    model: Terminal.destinations

                    delegate: ItemDelegate {
                        id: destination
                        required property var modelData
                        width: ListView.view.width
                        highlighted: modelData.destinationId === screen.destinationId
                        onClicked: {
                            screen.destinationId = modelData.destinationId
                            screen.destinationTitle = modelData.title
                            allocationError.text = ""
                        }

                        contentItem: ColumnLayout {
                            spacing: Theme.spacingXs
                            Label {
                                text: destination.modelData.title
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            // The warehouse has no limit, so it lists what it holds instead.
                            Label {
                                visible: destination.modelData.warehouse
                                text: destination.modelData.detail
                                font.pixelSize: Theme.fontSizeCaption
                                color: Theme.textMuted
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                            RowLayout {
                                visible: !destination.modelData.warehouse
                                Layout.fillWidth: true
                                Label {
                                    text: screen.loadText(destination.modelData.loadBoxes,
                                                          destination.modelData.limitBoxes, qsTr("ящ."))
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: Theme.textMuted
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: screen.loadText(destination.modelData.loadKg,
                                                          destination.modelData.limitKg, qsTr("кг"))
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: Theme.textMuted
                                    elide: Text.ElideRight
                                }
                            }
                            // Overload is only shown: facts from another terminal are never rejected.
                            RowLayout {
                                visible: !destination.modelData.warehouse
                                Layout.fillWidth: true
                                ProgressBar {
                                    from: 0
                                    to: 100
                                    value: Math.min(destination.modelData.fillPercent, 100)
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: destination.modelData.overloaded ? qsTr("понад") : destination.modelData.fillPercent + "%"
                                    font.pixelSize: Theme.fontSizeCaption
                                    font.weight: destination.modelData.overloaded ? Font.DemiBold : Font.Normal
                                    color: destination.modelData.overloaded ? Theme.danger : Theme.textMuted
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
