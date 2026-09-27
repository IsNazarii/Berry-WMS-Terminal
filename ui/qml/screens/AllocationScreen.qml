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
    title: qsTr("Розподіл по фурах")

    // A truck id or "warehouse"; kept in QML, the core checks it on every allocation.
    property string destinationId: ""
    property string destinationTitle: ""
    readonly property bool hasReceipt: Terminal.allocationReceiptId !== ""

    Label {
        anchors.centerIn: parent
        visible: !Terminal.sessionOpen
        text: qsTr("Розподіл можливий лише у відкритій сесії")
        font.pixelSize: 20
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        visible: Terminal.sessionOpen
        spacing: 12

        // Receipts that still have boxes to allocate: open ones first, pallets go out while unloading.
        ColumnLayout {
            Layout.preferredWidth: 240
            Layout.fillHeight: true

            Label {
                text: qsTr("Чеки до розподілу")
                font.bold: true
            }
            ListView {
                id: sourceList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: Terminal.allocationSources

                Label {
                    anchors.centerIn: parent
                    visible: sourceList.count === 0
                    text: qsTr("Усе розподілено")
                    color: palette.placeholderText
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
                        Label {
                            text: sourceRow.modelData.supplierName
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: sourceRow.modelData.openedAt + " · "
                                  + (sourceRow.modelData.open ? qsTr("зважується") : qsTr("закритий"))
                            color: palette.placeholderText
                        }
                        Label {
                            text: qsTr("лишилось: %1").arg(sourceRow.modelData.left)
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }

        // The chosen receipt: whole pallets or a number of boxes of a crop.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            Label {
                text: screen.hasReceipt ? Terminal.allocationSupplierName : qsTr("Оберіть чек зліва")
                font.pixelSize: 18
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Label {
                text: screen.destinationId !== "" ? qsTr("Куди: %1").arg(screen.destinationTitle)
                                                  : qsTr("Оберіть фуру або склад справа")
                color: screen.destinationId !== "" ? palette.windowText : "#ef6c00"
                font.bold: true
            }

            Label {
                visible: screen.hasReceipt
                text: qsTr("Палетами")
                color: palette.placeholderText
            }
            ListView {
                id: lineList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 4
                model: Terminal.allocationLines
                ScrollBar.vertical: ScrollBar {}

                delegate: Frame {
                    id: line
                    required property var modelData
                    width: ListView.view.width
                    opacity: modelData.cancelled ? 0.45 : 1

                    RowLayout {
                        anchors.fill: parent

                        Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: qsTr("%1. %2 · %3 ящ. · %4").arg(line.modelData.index + 1).arg(line.modelData.crop)
                                  .arg(line.modelData.boxes).arg(line.modelData.net)
                            font.strikeout: line.modelData.cancelled
                        }
                        Label {
                            visible: line.modelData.placedIn !== "" || line.modelData.cancelled
                            text: line.modelData.cancelled ? qsTr("анульовано") : line.modelData.placedIn
                            color: palette.placeholderText
                        }
                        Button {
                            visible: line.modelData.placedIn === "" && !line.modelData.cancelled
                            enabled: screen.destinationId !== ""
                            text: qsTr("Палету →")
                            onClicked: allocationError.text =
                                       Terminal.allocateWeighing(line.modelData.index, screen.destinationId)
                        }
                    }
                }
            }

            Label {
                visible: screen.hasReceipt
                text: qsTr("Ящиками (вага — пропорційно)")
                color: palette.placeholderText
            }
            // Rebuilt after every allocation, which also clears the typed count: that is wanted here.
            Repeater {
                model: Terminal.allocationCrops

                RowLayout {
                    id: cropRow
                    required property var modelData
                    Layout.fillWidth: true

                    function send() {
                        allocationError.text = Terminal.allocateBoxes(modelData.cropCode, boxes.text,
                                                                      screen.destinationId)
                    }

                    Label {
                        text: qsTr("%1: лишилось %2").arg(cropRow.modelData.name).arg(cropRow.modelData.left)
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    TextField {
                        id: boxes
                        placeholderText: qsTr("ящ.")
                        Layout.preferredWidth: 80
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 1; top: cropRow.modelData.leftBoxes }
                        onAccepted: cropRow.send()
                    }
                    Button {
                        text: qsTr("Відвантажити")
                        enabled: screen.destinationId !== "" && boxes.text !== ""
                        onClicked: cropRow.send()
                    }
                }
            }

            Label {
                id: allocationError
                visible: text !== ""
                color: "#c62828"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        // Destinations: trucks of the session still loading, then the warehouse.
        ColumnLayout {
            Layout.preferredWidth: 300
            Layout.fillHeight: true

            Label {
                text: qsTr("Куди")
                font.bold: true
            }
            ListView {
                id: destinationList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 4
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
                        Label {
                            text: destination.modelData.title
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: destination.modelData.detail
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        // Overload is only shown: facts from another terminal are never rejected.
                        ProgressBar {
                            visible: !destination.modelData.warehouse
                            from: 0
                            to: 100
                            value: Math.min(destination.modelData.fillPercent, 100)
                            Layout.fillWidth: true
                        }
                        Label {
                            visible: destination.modelData.overloaded
                            text: qsTr("Перевантажено: %1%").arg(destination.modelData.fillPercent)
                            color: "#c62828"
                            font.bold: true
                        }
                    }
                }
            }
        }
    }
}
