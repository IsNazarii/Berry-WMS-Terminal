pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BerryWms.Ui

// The main screen: open receipts on the left, the scale in the middle, the
// active receipt on the right. Every weighing goes into the active receipt.
Page {
    id: screen
    title: qsTr("Прийом")

    readonly property ScaleMonitor scales: Terminal.scales
    readonly property bool hasReceipt: Terminal.activeReceiptId !== ""

    function newReceipt() { supplierPicker.open() }

    Shortcut {
        sequence: "F2"
        enabled: screen.visible && Terminal.sessionOpen
        onActivated: screen.newReceipt()
    }

    Label {
        anchors.centerIn: parent
        visible: !Terminal.sessionOpen
        text: qsTr("Прийом можливий лише у відкритій сесії")
        font.pixelSize: Theme.fontSizeLarge
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        visible: Terminal.sessionOpen
        spacing: 12

        // Open receipts: two big suppliers unloading in turns, a retail one in between.
        ColumnLayout {
            Layout.preferredWidth: 220
            Layout.fillHeight: true

            Button {
                text: qsTr("Новий чек (F2)")
                highlighted: true
                Layout.fillWidth: true
                onClicked: screen.newReceipt()
            }
            ListView {
                id: receiptList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: Terminal.openReceipts

                Label {
                    anchors.centerIn: parent
                    visible: receiptList.count === 0
                    text: qsTr("Відкритих чеків немає")
                    color: Theme.textMuted
                }

                delegate: ItemDelegate {
                    id: receiptRow
                    required property var modelData
                    width: ListView.view.width
                    highlighted: modelData.receiptId === Terminal.activeReceiptId
                    onClicked: Terminal.activateReceipt(modelData.receiptId)

                    contentItem: ColumnLayout {
                        Label {
                            text: receiptRow.modelData.supplierName
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: receiptRow.modelData.openedAt + " · " + receiptRow.modelData.total
                            color: Theme.textMuted
                        }
                    }
                }
            }
        }

        // The scale and the weighing form.
        ColumnLayout {
            id: weighingForm
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            // Crop and box tare are usually the same for the next pallet, so they
            // stay; the pallet, the price raise and a typed weight are per weighing.
            function record(acceptUnstable) {
                weighingError.text = Terminal.recordWeighing(crop.currentValue, boxes.text, boxTare.text,
                                                             palletTare.text, price.text, manualWeight.text,
                                                             acceptUnstable)
                if (weighingError.text === "") {
                    boxes.clear()
                    palletTare.clear()
                    price.clear()
                    manualWeight.clear()
                    boxes.forceActiveFocus()
                }
            }

            Frame {
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: screen.scales.display
                        font.pixelSize: Theme.fontSizeScale
                    }
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: screen.scales.stable ? qsTr("● СТАБІЛЬНО") : qsTr("○ вага змінюється…")
                        color: screen.scales.stable ? Theme.ok : Theme.warn
                        font.bold: true
                    }
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        Label { text: screen.scales.driverName; color: Theme.textMuted }
                        Button {
                            text: qsTr("Поставити палету")
                            flat: true
                            onClicked: Terminal.simulatePallet()
                        }
                        Button {
                            text: qsTr("Зняти")
                            flat: true
                            onClicked: Terminal.simulateEmptyScale()
                        }
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                enabled: screen.hasReceipt
                columns: 2

                Label { text: qsTr("Культура") }
                ComboBox {
                    id: crop
                    Layout.fillWidth: true
                    model: Terminal.crops
                    textRole: "name"
                    valueRole: "cropCode"
                }
                Label { text: qsTr("Ящиків") }
                TextField {
                    id: boxes
                    Layout.fillWidth: true
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 1 }
                    onAccepted: weighingForm.record(false)
                }
                Label { text: qsTr("Тара ящика, кг") }
                TextField {
                    id: boxTare
                    Layout.fillWidth: true
                    placeholderText: qsTr("напр. 0,35")
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                }
                Label { text: qsTr("Палета, кг") }
                TextField {
                    id: palletTare
                    Layout.fillWidth: true
                    placeholderText: qsTr("щоразу; 0 — без палети")
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    onAccepted: weighingForm.record(false)
                }
                Label { text: qsTr("Ціна, грн/кг") }
                TextField {
                    id: price
                    Layout.fillWidth: true
                    readonly property string dayPrice: Terminal.prices[crop.currentValue] ?? ""
                    // Empty means the day price; anything typed is a one-off raise for this weighing.
                    placeholderText: dayPrice !== "" ? qsTr("денна: %1").arg(dayPrice) : qsTr("денної ціни немає")
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                }
                Label { text: qsTr("Вага вручну, кг") }
                TextField {
                    id: manualWeight
                    Layout.fillWidth: true
                    placeholderText: qsTr("порожньо — з ваг")
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                }
            }

            RowLayout {
                Layout.fillWidth: true
                enabled: screen.hasReceipt

                Button {
                    text: qsTr("Записати зважування")
                    highlighted: true
                    enabled: screen.scales.stable || manualWeight.text.trim() !== ""
                    Layout.fillWidth: true
                    onClicked: weighingForm.record(false)
                }
                // A separate, deliberate action: the scale may keep flipping between two steps.
                Button {
                    text: qsTr("Прийняти нестабільну")
                    enabled: !screen.scales.stable && manualWeight.text.trim() === ""
                    onClicked: weighingForm.record(true)
                }
            }

            Label {
                id: weighingError
                visible: text !== ""
                color: Theme.danger
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Item { Layout.fillHeight: true }
        }

        // The active receipt.
        ColumnLayout {
            Layout.preferredWidth: 360
            Layout.fillHeight: true

            Label {
                text: screen.hasReceipt ? Terminal.supplierName : qsTr("Чек не обрано")
                font.pixelSize: Theme.fontSizeTitle
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Label {
                visible: screen.hasReceipt
                text: qsTr("Борг до чека: %1").arg(Terminal.supplierBalance)
                color: Theme.textMuted
            }

            ListView {
                id: lineList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 4
                model: Terminal.activeLines
                ScrollBar.vertical: ScrollBar {}

                Label {
                    anchors.centerIn: parent
                    visible: screen.hasReceipt && lineList.count === 0
                    text: qsTr("Зважувань ще немає")
                    color: Theme.textMuted
                }

                delegate: Frame {
                    id: line
                    required property var modelData
                    width: ListView.view.width
                    // A cancelled line stays visible: the trace of the mistake is kept.
                    opacity: modelData.cancelled ? Theme.cancelledOpacity : 1

                    RowLayout {
                        anchors.fill: parent

                        ColumnLayout {
                            Layout.fillWidth: true
                            // Labels elide instead of pushing the cancel button out of a narrow window.
                            Label {
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                text: qsTr("%1. %2 · %3 ящ.").arg(line.modelData.index + 1)
                                      .arg(line.modelData.crop).arg(line.modelData.boxes)
                                      + (line.modelData.cancelled ? qsTr(" — анульовано") : "")
                                font.bold: true
                                font.strikeout: line.modelData.cancelled
                            }
                            Label {
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                text: qsTr("брутто %1%2 − тара %3 = %4").arg(line.modelData.gross)
                                      .arg(line.modelData.manualWeight ? qsTr(" (вручну)") : "")
                                      .arg(line.modelData.tare).arg(line.modelData.net)
                                font.strikeout: line.modelData.cancelled
                            }
                            Label {
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                text: qsTr("%1/кг%2 → %3").arg(line.modelData.price)
                                      .arg(line.modelData.manualPrice ? qsTr(" (вручну)") : "")
                                      .arg(line.modelData.amount)
                                font.strikeout: line.modelData.cancelled
                            }
                        }
                        ToolButton {
                            visible: !line.modelData.cancelled
                            text: "✕"
                            Layout.alignment: Qt.AlignTop
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Анулювати зважування")
                            onClicked: receiptError.text = Terminal.cancelWeighing(line.modelData.index)
                        }
                    }
                }
            }

            Label {
                id: receiptError
                visible: text !== ""
                color: Theme.danger
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Label {
                visible: screen.hasReceipt
                text: qsTr("Разом: %1").arg(Terminal.activeTotal)
                font.bold: true
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Закрити чек…")
                enabled: screen.hasReceipt
                Layout.fillWidth: true
                onClicked: closeDialog.open()
            }
        }
    }

    SupplierPicker {
        id: supplierPicker
        anchors.centerIn: parent
        onPicked: supplierId => {
            weighingError.text = Terminal.startReceipt(supplierId)
            receiptError.text = ""
            boxes.forceActiveFocus()
        }
    }

    // Closing is final: the receipt becomes immutable and is printed.
    Dialog {
        id: closeDialog
        anchors.centerIn: parent
        modal: true
        title: qsTr("Закрити чек: %1").arg(Terminal.supplierName)

        // Re-read on every change of the choice; nothing else changes while the dialog is modal.
        readonly property var blockers: Terminal.receiptCloseBlockers(wholesale.checked)

        onOpened: retail.checked = true

        ColumnLayout {
            anchors.fill: parent

            RadioButton {
                id: retail
                text: qsTr("Роздріб — ціна відома, розрахунок тут (зараз або пізніше)")
            }
            RadioButton {
                id: wholesale
                text: qsTr("Опт — лише вага, чек передається відповідальній особі")
            }
            Repeater {
                model: closeDialog.blockers
                Label {
                    required property string modelData
                    text: "• " + modelData
                    color: Theme.danger
                }
            }
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Закрити чек")
                enabled: closeDialog.blockers.length === 0
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
            Button {
                text: qsTr("Скасувати")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
        }

        onAccepted: {
            Terminal.closeReceipt(wholesale.checked)
            receiptError.text = ""
        }
    }
}
