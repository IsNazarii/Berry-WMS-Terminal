pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BerryWms.Ui

// The working session: opened around noon, prices of the day, trucks to load,
// and closing once every truck has been sent off or carried over.
Page {
    id: screen
    title: qsTr("Сесія")

    // Closed session: the only thing to do is to open a new one.
    ColumnLayout {
        anchors.centerIn: parent
        visible: !Terminal.sessionOpen
        spacing: 12

        Label {
            text: qsTr("Сесію не відкрито")
            font.pixelSize: Theme.fontSizeHeading
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            text: qsTr("Перенесені фури з попередньої сесії додадуться автоматично.")
            color: Theme.textMuted
            Layout.alignment: Qt.AlignHCenter
        }
        Button {
            text: qsTr("Відкрити сесію")
            highlighted: true
            Layout.alignment: Qt.AlignHCenter
            onClicked: Terminal.openSession()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        visible: Terminal.sessionOpen
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: Terminal.sessionTitle
                font.pixelSize: Theme.fontSizeHeading
                font.bold: true
            }
            Label {
                text: Terminal.sessionOpenedBy
                color: Theme.textMuted
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Закрити сесію")
                enabled: Terminal.closeBlockers.length === 0
                onClicked: confirmClose.open()
            }
        }

        // Shown right away rather than on a failed click: the operator sees what is left to do.
        Repeater {
            model: Terminal.closeBlockers
            Label {
                required property string modelData
                text: "• " + modelData
                color: Theme.danger
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 24

            // Prices of the day. A new price applies to weighings from now on.
            ColumnLayout {
                id: prices
                Layout.preferredWidth: 340
                Layout.alignment: Qt.AlignTop
                spacing: 8

                // Every filled field at once: the operator usually sets all prices together.
                signal applyAll()

                Label {
                    text: qsTr("Ціни за кг")
                    font.pixelSize: Theme.fontSizeTitle
                    font.bold: true
                }
                Repeater {
                    model: Terminal.crops

                    ColumnLayout {
                        id: priceRow
                        required property var modelData
                        readonly property string price: Terminal.prices[modelData.cropCode] ?? ""

                        // An empty field keeps the price; a rejected one keeps its text to be fixed.
                        function apply() {
                            if (newPrice.text.trim() === "")
                                return
                            priceError.text = Terminal.setPrice(modelData.cropCode, newPrice.text)
                            if (priceError.text === "")
                                newPrice.clear()
                        }

                        Connections {
                            target: prices
                            function onApplyAll() { priceRow.apply() }
                        }

                        RowLayout {
                            Label {
                                text: priceRow.modelData.name
                                Layout.preferredWidth: 90
                            }
                            Label {
                                text: priceRow.price !== "" ? priceRow.price : qsTr("не задано")
                                color: priceRow.price !== "" ? Theme.text : Theme.danger
                                Layout.preferredWidth: 100
                            }
                            TextField {
                                id: newPrice
                                placeholderText: qsTr("нова, грн")
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                                Layout.fillWidth: true
                                onTextEdited: priceError.text = ""
                                onAccepted: prices.applyAll()
                            }
                        }
                        Label {
                            id: priceError
                            visible: text !== ""
                            color: Theme.danger
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }
                Button {
                    text: qsTr("Зберегти ціни")
                    Layout.alignment: Qt.AlignRight
                    onClicked: prices.applyAll()
                }
            }

            // Trucks of this session with their load, and the operator's decision for each.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8

                Label {
                    text: qsTr("Фури")
                    font.pixelSize: Theme.fontSizeTitle
                    font.bold: true
                }
                ListView {
                    id: truckList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: Terminal.trucks

                    Label {
                        anchors.centerIn: parent
                        visible: truckList.count === 0
                        text: qsTr("Фур ще немає")
                        color: Theme.textMuted
                    }

                    delegate: Frame {
                        id: truckCard
                        required property var modelData
                        width: ListView.view.width

                        RowLayout {
                            anchors.fill: parent
                            spacing: 12

                            ColumnLayout {
                                Layout.fillWidth: true
                                Label {
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    text: [truckCard.modelData.plate, truckCard.modelData.make]
                                          .filter(part => part !== "").join(" · ")
                                          + " — " + truckCard.modelData.status
                                    font.bold: true
                                }
                                Label {
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    visible: text !== ""
                                    text: truckCard.modelData.note
                                    color: Theme.textMuted
                                }
                                Label {
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    text: qsTr("Завантажено: %1 з %2")
                                          .arg(truckCard.modelData.load).arg(truckCard.modelData.capacity)
                                    color: truckCard.modelData.overloaded ? Theme.danger : Theme.text
                                }
                                ProgressBar {
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 100
                                    value: Math.min(truckCard.modelData.fillPercent, 100)
                                }
                            }
                            // Stacked, so the card fits next to the prices in a laptop-sized window.
                            ColumnLayout {
                                visible: truckCard.modelData.loading
                                Button {
                                    text: qsTr("Відправити")
                                    Layout.fillWidth: true
                                    onClicked: Terminal.departTruck(truckCard.modelData.truckId)
                                }
                                Button {
                                    text: qsTr("Перенести")
                                    Layout.fillWidth: true
                                    onClicked: Terminal.carryOverTruck(truckCard.modelData.truckId)
                                }
                            }
                        }
                    }
                }

                // New truck: at least one limit, an empty field means "no limit".
                GridLayout {
                    id: newTruck
                    Layout.fillWidth: true
                    columns: 4

                    function add() {
                        truckError.text = Terminal.addTruck(plate.text, make.text, note.text,
                                                            boxLimit.text, weightLimit.text)
                        if (truckError.text === "") {
                            plate.clear()
                            make.clear()
                            note.clear()
                            boxLimit.clear()
                            weightLimit.clear()
                        }
                    }

                    TextField {
                        id: plate
                        placeholderText: qsTr("Номер, напр. BI1234AA")
                        Layout.fillWidth: true
                    }
                    TextField {
                        id: make
                        placeholderText: qsTr("Марка")
                        Layout.fillWidth: true
                    }
                    TextField {
                        id: boxLimit
                        placeholderText: qsTr("ящиків")
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 0 }
                        Layout.preferredWidth: 80
                    }
                    TextField {
                        id: weightLimit
                        placeholderText: qsTr("кг")
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 0 }
                        Layout.preferredWidth: 80
                    }
                    TextField {
                        id: note
                        placeholderText: qsTr("Водій / куди їде")
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                    }
                    Button {
                        text: qsTr("Додати фуру")
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        onClicked: newTruck.add()
                    }
                }

                // Same plate and limits as a truck from the previous session, but a new trip.
                RowLayout {
                    Layout.fillWidth: true
                    visible: Terminal.previousTrucks.length > 0

                    ComboBox {
                        id: previousTruck
                        Layout.fillWidth: true
                        model: Terminal.previousTrucks
                        textRole: "plate"
                        valueRole: "index"
                        displayText: currentIndex < 0 ? "" : qsTr("%1 %2 (%3)").arg(currentText)
                                     .arg(Terminal.previousTrucks[currentIndex].make)
                                     .arg(Terminal.previousTrucks[currentIndex].capacity)
                    }
                    Button {
                        text: qsTr("Повторити фуру з минулої сесії")
                        enabled: previousTruck.currentIndex >= 0
                        onClicked: truckError.text = Terminal.repeatTruck(previousTruck.currentValue)
                    }
                }

                Label {
                    id: truckError
                    visible: text !== ""
                    color: Theme.danger
                }
            }
        }
    }

    Dialog {
        id: confirmClose
        anchors.centerIn: parent
        modal: true
        title: qsTr("Закрити сесію?")
        standardButtons: Dialog.Yes | Dialog.No

        Label {
            text: qsTr("Після закриття змінити сесію можна лише поправкою з поясненням.")
        }
        onAccepted: Terminal.closeSession()
    }
}
