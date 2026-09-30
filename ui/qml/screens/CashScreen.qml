pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BerryWms.Ui

// Paying suppliers: who we owe on the left, the chosen supplier's balance and
// payment form in the middle, where the balance comes from on the right.
// Debts are shared by all sessions, so the desk works without an open session.
Page {
    id: screen
    title: qsTr("Каса")

    readonly property bool hasSupplier: Terminal.cashSupplierId !== 0

    function choose(supplierId) {
        Terminal.selectCashSupplier(supplierId)
        amount.clear()
        paymentError.text = ""
        amount.forceActiveFocus()
    }

    Shortcut {
        sequence: "F2"
        enabled: screen.visible
        onActivated: supplierPicker.open()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        ColumnLayout {
            Layout.preferredWidth: 260
            Layout.fillHeight: true

            Label {
                text: qsTr("Борги")
                font.bold: true
            }
            ListView {
                id: debtorList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 4
                model: Terminal.debtors

                Label {
                    anchors.centerIn: parent
                    visible: debtorList.count === 0
                    text: qsTr("Ми нікому не винні")
                    color: Theme.textMuted
                }

                delegate: ItemDelegate {
                    id: debtor
                    required property var modelData
                    width: ListView.view.width
                    highlighted: modelData.supplierId === Terminal.cashSupplierId
                    onClicked: screen.choose(modelData.supplierId)

                    contentItem: RowLayout {
                        Label {
                            text: debtor.modelData.name
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: debtor.modelData.balance
                            // Overpaid: the supplier owes us.
                            color: debtor.modelData.overpaid ? Theme.danger : Theme.text
                            font.bold: true
                        }
                    }
                }
            }
            Button {
                text: qsTr("Інший постачальник (F2)")
                Layout.fillWidth: true
                onClicked: supplierPicker.open()
            }
        }

        ColumnLayout {
            Layout.preferredWidth: 340
            Layout.fillHeight: true
            spacing: 8

            Label {
                visible: !screen.hasSupplier
                text: qsTr("Оберіть постачальника зліва або F2")
                color: Theme.warn
                font.bold: true
            }
            Label {
                visible: screen.hasSupplier
                text: Terminal.cashSupplierName
                font.pixelSize: Theme.fontSizeTitle
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Label {
                visible: screen.hasSupplier
                text: qsTr("Борг: %1").arg(Terminal.cashBalance)
                font.pixelSize: Theme.fontSizeHeading
            }

            GridLayout {
                visible: screen.hasSupplier
                columns: 2
                Layout.fillWidth: true

                Label { text: qsTr("Сума, грн") }
                RowLayout {
                    TextField {
                        id: amount
                        placeholderText: qsTr("напр. 1500")
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        Layout.fillWidth: true
                        onAccepted: payButton.clicked()
                    }
                    Button {
                        text: qsTr("Весь борг")
                        enabled: Terminal.cashDebtInput !== ""
                        onClicked: amount.text = Terminal.cashDebtInput
                    }
                }

                Label { text: qsTr("Спосіб") }
                RowLayout {
                    RadioButton {
                        id: cash
                        text: qsTr("Готівка")
                        checked: true
                    }
                    RadioButton {
                        id: transfer
                        text: qsTr("Переказ")
                    }
                }
            }

            Button {
                id: payButton
                visible: screen.hasSupplier
                text: qsTr("Виплатити…")
                enabled: amount.text !== ""
                Layout.fillWidth: true
                onClicked: {
                    // Bad input is caught here, not after the operator confirms.
                    if (Terminal.balanceAfter(amount.text) === "")
                        paymentError.text = qsTr("Сума — число гривень, напр. 1500 або 1500,50")
                    else
                        confirmDialog.open()
                }
            }
            Label {
                id: paymentError
                visible: text !== ""
                color: Theme.danger
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Item { Layout.fillHeight: true }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: screen.hasSupplier

            Label {
                text: qsTr("Історія розрахунків")
                font.bold: true
            }
            ListView {
                id: history
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 6
                model: Terminal.cashHistory
                ScrollBar.vertical: ScrollBar {}
                // Newest record is what the operator just did; keep it in view.
                onCountChanged: positionViewAtEnd()

                Label {
                    anchors.centerIn: parent
                    visible: history.count === 0
                    text: qsTr("Записів немає")
                    color: Theme.textMuted
                }

                delegate: Frame {
                    id: record
                    required property var modelData
                    width: ListView.view.width

                    ColumnLayout {
                        anchors.fill: parent

                        RowLayout {
                            Label { text: record.modelData.at }
                            Label {
                                text: record.modelData.title
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                text: record.modelData.effect
                                color: record.modelData.payment ? Theme.ok : Theme.text
                                font.bold: true
                            }
                        }
                        Label {
                            visible: text !== ""
                            text: record.modelData.detail
                            color: Theme.textMuted
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }
    }

    SupplierPicker {
        id: supplierPicker
        anchors.centerIn: parent
        onPicked: supplierId => screen.choose(supplierId)
    }

    // Money leaves the till: the operator sees what the balance becomes before it happens.
    Dialog {
        id: confirmDialog
        anchors.centerIn: parent
        modal: true
        title: qsTr("Виплата: %1").arg(Terminal.cashSupplierName)
        standardButtons: Dialog.Ok | Dialog.Cancel

        ColumnLayout {
            anchors.fill: parent

            Label {
                text: qsTr("Сума: %1 грн, %2").arg(amount.text).arg(cash.checked ? qsTr("готівкою") : qsTr("переказом"))
                font.bold: true
            }
            Label {
                text: qsTr("Борг після виплати: %1").arg(Terminal.balanceAfter(amount.text))
            }
            Label {
                visible: Terminal.balanceAfter(amount.text).startsWith("-")
                text: qsTr("Більше, ніж борг: постачальник буде винен нам")
                color: Theme.danger
            }
        }

        onAccepted: {
            paymentError.text = Terminal.pay(amount.text, transfer.checked)
            if (paymentError.text === "") amount.clear()
        }
    }
}
