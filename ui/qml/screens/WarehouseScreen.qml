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
    // Nothing of any crop: the table of zeros gives way to what fills the warehouse.
    readonly property bool empty: Terminal.stock.every(s => s.empty)

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

                PanelHeader {
                    title: qsTr("Залишок на складі")
                    subtitle: qsTr("Надходить з «Прийому» і «Розподілу»; відвантажується лише у фуру поточної сесії")
                }

                TargetBanner {
                    visible: !screen.empty
                    text: !Terminal.sessionOpen ? qsTr("Відвантаження можливе лише у відкритій сесії")
                          : screen.hasTruck ? qsTr("Куди: %1").arg(screen.truckTitle)
                                            : qsTr("Оберіть фуру праворуч")
                    active: screen.hasTruck && Terminal.sessionOpen
                    warning: !Terminal.sessionOpen
                }

                ColumnLayout {
                    visible: !screen.empty
                    spacing: 0
                    Layout.fillWidth: true

                    TableHeader {
                        columns: [
                            { title: qsTr("Культура") },
                            { title: qsTr("Ящ."), width: stockPanel.boxesWidth, align: "right" },
                            { title: qsTr("Вага, кг"), width: stockPanel.kgWidth, align: "right" },
                            { title: qsTr("Відвантажити"), width: stockPanel.shipWidth, align: "right",
                              visible: Terminal.sessionOpen }
                        ]
                    }

                    // Rebuilt after every shipment, which also clears the typed count: that is wanted here.
                    Repeater {
                        model: Terminal.stock

                        TableRow {
                            id: cropRow
                            required property var modelData
                            // Two terminals shipped the same boxes offline; the logistician sorts it out.
                            readonly property color numberColor: modelData.negative ? Theme.danger
                                                                 : modelData.empty ? Theme.textMuted : Theme.text
                            Layout.fillWidth: true

                            function ship() {
                                shipmentError.text = Terminal.shipFromStock(modelData.cropCode, boxes.text, screen.truckId)
                            }

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
                        }
                    }
                }

                ErrorLabel { id: shipmentError }
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    EmptyState {
                        anchors.centerIn: parent
                        visible: screen.empty
                        icon: "warehouse"
                        title: qsTr("Склад порожній")
                        text: qsTr("Палети потрапляють сюди з «Прийому» (ціль «Склад») і з «Розподілу». "
                                   + "Звідси ящики йдуть лише у фуру поточної сесії.")
                    }
                }
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

                    ListPlaceholder {
                        visible: truckList.count === 0
                        text: qsTr("Немає фур, що завантажуються")
                    }

                    delegate: DestinationCard {
                        required property var modelData
                        place: modelData
                        highlighted: modelData.destinationId === screen.truckId
                        onClicked: {
                            screen.truckId = modelData.destinationId
                            screen.truckTitle = modelData.title
                            shipmentError.text = ""
                        }
                    }
                }
            }
        }
    }
}
