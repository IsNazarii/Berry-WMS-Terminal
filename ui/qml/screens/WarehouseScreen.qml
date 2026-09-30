pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BerryWms.Ui

// The warehouse outlives sessions: what is left of each crop, and shipping
// boxes from it into a truck of the current session. Berries get in here
// only from receipts, on the allocation screen.
Page {
    id: screen
    title: qsTr("Склад")

    // A truck id; kept in QML, the core checks it on every shipment.
    property string truckId: ""
    property string truckTitle: ""
    // Trucks of the session still loading; the warehouse itself is not a target here.
    readonly property var trucks: Terminal.destinations.filter(d => !d.warehouse)

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // Stock is visible even without a session; shipping needs one.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            Label {
                text: qsTr("Залишок на складі")
                font.pixelSize: Theme.fontSizeTitle
                font.bold: true
            }
            Label {
                text: !Terminal.sessionOpen ? qsTr("Відвантаження можливе лише у відкритій сесії")
                      : screen.truckId !== "" ? qsTr("Куди: %1").arg(screen.truckTitle)
                                              : qsTr("Оберіть фуру справа")
                color: screen.truckId !== "" ? Theme.text : Theme.warn
                font.bold: true
            }

            // Rebuilt after every shipment, which also clears the typed count: that is wanted here.
            Repeater {
                model: Terminal.stock

                Frame {
                    id: cropRow
                    required property var modelData
                    Layout.fillWidth: true

                    function ship() {
                        shipmentError.text = Terminal.shipFromStock(modelData.cropCode, boxes.text, screen.truckId)
                    }

                    RowLayout {
                        anchors.fill: parent

                        Label {
                            text: cropRow.modelData.name
                            font.bold: true
                            Layout.preferredWidth: 110
                        }
                        Label {
                            text: cropRow.modelData.stock
                            // Two terminals shipped the same boxes offline; the logistician sorts it out.
                            color: cropRow.modelData.negative ? Theme.danger : Theme.text
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        TextField {
                            id: boxes
                            visible: cropRow.modelData.boxes > 0
                            placeholderText: qsTr("ящ.")
                            Layout.preferredWidth: 80
                            inputMethodHints: Qt.ImhDigitsOnly
                            validator: IntValidator { bottom: 1; top: Math.max(1, cropRow.modelData.boxes) }
                            onAccepted: cropRow.ship()
                        }
                        Button {
                            visible: cropRow.modelData.boxes > 0
                            text: qsTr("Відвантажити")
                            enabled: screen.truckId !== "" && boxes.text !== ""
                            onClicked: cropRow.ship()
                        }
                    }
                }
            }

            Label {
                id: shipmentError
                visible: text !== ""
                color: Theme.danger
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Item { Layout.fillHeight: true }
        }

        ColumnLayout {
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            visible: Terminal.sessionOpen

            Label {
                text: qsTr("Фури сесії")
                font.bold: true
            }
            ListView {
                id: truckList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 4
                model: screen.trucks

                Label {
                    anchors.centerIn: parent
                    visible: truckList.count === 0
                    text: qsTr("Немає фур, що завантажуються")
                    color: Theme.textMuted
                }

                delegate: ItemDelegate {
                    id: truck
                    required property var modelData
                    width: ListView.view.width
                    highlighted: modelData.destinationId === screen.truckId
                    onClicked: {
                        screen.truckId = modelData.destinationId
                        screen.truckTitle = modelData.title
                        shipmentError.text = ""
                    }

                    contentItem: ColumnLayout {
                        Label {
                            text: truck.modelData.title
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: truck.modelData.detail
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        ProgressBar {
                            from: 0
                            to: 100
                            value: Math.min(truck.modelData.fillPercent, 100)
                            Layout.fillWidth: true
                        }
                        Label {
                            visible: truck.modelData.overloaded
                            text: qsTr("Перевантажено: %1%").arg(truck.modelData.fillPercent)
                            color: Theme.danger
                            font.bold: true
                        }
                    }
                }
            }
        }
    }
}
