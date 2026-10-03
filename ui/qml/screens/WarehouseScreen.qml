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
    readonly property bool hasTruck: truckId !== ""
    // Trucks of the session still loading; the warehouse itself is not a target here.
    readonly property var trucks: Terminal.destinations.filter(d => !d.warehouse)

    // "40 / 400 ящ.": the load over its limit, or the load alone when there is none.
    function loadText(load, limit, unit) {
        return (limit !== "" ? load + " / " + limit : load) + " " + unit
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingM
        spacing: Theme.spacingM

        // Stock is visible even without a session; shipping needs one.
        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                id: stockPanel
                anchors.fill: parent
                spacing: Theme.spacingM

                // Column widths shared by the header and the rows; the crop takes the rest.
                readonly property int boxesWidth: 60
                readonly property int kgWidth: 90
                readonly property int shipWidth: 240

                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Залишок на складі")
                        font.pixelSize: Theme.fontSizeTitle
                        font.weight: Font.Bold
                    }
                    Label {
                        text: qsTr("Надходить з чеків на екрані «Розподіл»; відвантажується лише у фуру поточної сесії")
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.textMuted
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                // Where the boxes go: picked once on the right, shown here where the buttons are.
                Label {
                    text: !Terminal.sessionOpen ? qsTr("Відвантаження можливе лише у відкритій сесії")
                          : screen.hasTruck ? qsTr("Куди: %1").arg(screen.truckTitle)
                                            : qsTr("Оберіть фуру праворуч")
                    font.weight: Font.DemiBold
                    color: screen.hasTruck && Terminal.sessionOpen ? Theme.accent : Theme.warn
                    elide: Text.ElideRight
                    padding: Theme.spacingS
                    Layout.fillWidth: true
                    background: Rectangle {
                        radius: Theme.radiusS
                        color: screen.hasTruck && Terminal.sessionOpen ? Theme.accentSoft : Theme.surfaceAlt
                    }
                }

                ColumnLayout {
                    spacing: 0
                    Layout.fillWidth: true

                    RowLayout {
                        spacing: Theme.spacingM
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28

                        ColumnHeader { text: qsTr("Культура"); horizontalAlignment: Text.AlignLeft; Layout.fillWidth: true }
                        ColumnHeader { text: qsTr("Ящ."); Layout.preferredWidth: stockPanel.boxesWidth }
                        ColumnHeader { text: qsTr("Вага, кг"); Layout.preferredWidth: stockPanel.kgWidth }
                        ColumnHeader {
                            visible: Terminal.sessionOpen
                            text: qsTr("Відвантажити")
                            Layout.preferredWidth: stockPanel.shipWidth
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 1
                        color: Theme.border
                    }

                    // Rebuilt after every shipment, which also clears the typed count: that is wanted here.
                    Repeater {
                        model: Terminal.stock

                        Rectangle {
                            id: cropRow
                            required property var modelData
                            // Two terminals shipped the same boxes offline; the logistician sorts it out.
                            readonly property color numberColor: modelData.negative ? Theme.danger
                                                                 : modelData.empty ? Theme.textMuted : Theme.text
                            Layout.fillWidth: true
                            implicitHeight: 48
                            color: rowHover.hovered ? Theme.surfaceAlt : "transparent"

                            function ship() {
                                shipmentError.text = Terminal.shipFromStock(modelData.cropCode, boxes.text, screen.truckId)
                            }

                            HoverHandler { id: rowHover }

                            RowLayout {
                                anchors.fill: parent
                                spacing: Theme.spacingM

                                Label {
                                    text: cropRow.modelData.name
                                    font.weight: Font.DemiBold
                                    color: cropRow.modelData.empty ? Theme.textMuted : Theme.text
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: cropRow.modelData.boxes
                                    color: cropRow.numberColor
                                    horizontalAlignment: Text.AlignRight
                                    Layout.preferredWidth: stockPanel.boxesWidth
                                }
                                Label {
                                    text: cropRow.modelData.kg
                                    color: cropRow.numberColor
                                    horizontalAlignment: Text.AlignRight
                                    Layout.preferredWidth: stockPanel.kgWidth
                                }
                                // Fixed width, or the button stretches the column away from its header.
                                RowLayout {
                                    visible: Terminal.sessionOpen
                                    spacing: Theme.spacingS
                                    Layout.preferredWidth: stockPanel.shipWidth
                                    Layout.minimumWidth: stockPanel.shipWidth
                                    Layout.maximumWidth: stockPanel.shipWidth

                                    TextField {
                                        id: boxes
                                        visible: cropRow.modelData.boxes > 0
                                        placeholderText: qsTr("ящиків")
                                        Layout.fillWidth: true
                                        inputMethodHints: Qt.ImhDigitsOnly
                                        validator: IntValidator { bottom: 1; top: Math.max(1, cropRow.modelData.boxes) }
                                        onAccepted: cropRow.ship()
                                    }
                                    Button {
                                        visible: cropRow.modelData.boxes > 0
                                        text: qsTr("У фуру →")
                                        enabled: screen.hasTruck && boxes.text !== ""
                                        onClicked: cropRow.ship()
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
                }

                Label {
                    id: shipmentError
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
                Item { Layout.fillHeight: true }
            }
        }

        // Trucks of the session still loading, as on the allocation screen.
        Frame {
            Layout.preferredWidth: 260
            Layout.fillHeight: true
            visible: Terminal.sessionOpen

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacingM

                SectionCaption { text: qsTr("Фури сесії") }

                ListView {
                    id: truckList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: Theme.spacingXs
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
                            spacing: Theme.spacingXs
                            Label {
                                text: truck.modelData.title
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    text: screen.loadText(truck.modelData.loadBoxes, truck.modelData.limitBoxes, qsTr("ящ."))
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: Theme.textMuted
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: screen.loadText(truck.modelData.loadKg, truck.modelData.limitKg, qsTr("кг"))
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: Theme.textMuted
                                    elide: Text.ElideRight
                                }
                            }
                            // Overload is only shown: facts from another terminal are never rejected.
                            RowLayout {
                                Layout.fillWidth: true
                                ProgressBar {
                                    from: 0
                                    to: 100
                                    value: Math.min(truck.modelData.fillPercent, 100)
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: truck.modelData.overloaded ? qsTr("понад") : truck.modelData.fillPercent + "%"
                                    font.pixelSize: Theme.fontSizeCaption
                                    font.weight: truck.modelData.overloaded ? Font.DemiBold : Font.Normal
                                    color: truck.modelData.overloaded ? Theme.danger : Theme.textMuted
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
