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
    // What the balance becomes; "" while the amount is not a number.
    readonly property string balanceAfter: Terminal.balanceAfter(amount.text)
    // Sums come with a typographic minus (U+2212), not a hyphen.
    readonly property bool overpays: balanceAfter.startsWith("−")

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
        anchors.margins: Theme.spacingM
        spacing: Theme.spacingM

        // Who we owe, biggest debt first; anyone else is one F2 away.
        Frame {
            Layout.preferredWidth: 240
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacingM

                Button {
                    text: qsTr("Постачальник  ·  F2")
                    Layout.fillWidth: true
                    onClicked: supplierPicker.open()
                }
                RowLayout {
                    Layout.fillWidth: true
                    SectionCaption { text: qsTr("Борги"); Layout.fillWidth: true }
                    SectionCaption { text: qsTr("грн") }
                }

                ListView {
                    id: debtorList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: Theme.spacingXs
                    model: Terminal.debtors

                    ListPlaceholder {
                        visible: debtorList.count === 0
                        text: qsTr("Ми нікому не винні")
                    }

                    delegate: ListRow {
                        required property var modelData
                        title: modelData.name
                        trailing: modelData.balance
                        trailingSize: Theme.fontSizeBody
                        // Overpaid: the supplier owes us.
                        trailingColor: modelData.overpaid ? Theme.danger : Theme.text
                        highlighted: modelData.supplierId === Terminal.cashSupplierId
                        onClicked: screen.choose(modelData.supplierId)
                    }
                }
            }
        }

        // Before a supplier is chosen the form and the history are one empty panel.
        Frame {
            visible: !screen.hasSupplier
            Layout.fillWidth: true
            Layout.fillHeight: true

            EmptyState {
                anchors.centerIn: parent
                icon: "wallet"
                title: qsTr("Кому платимо?")
                text: qsTr("Оберіть боржника зліва або знайдіть будь-якого постачальника — "
                           + "побачите його баланс, історію чеків і виплат.")
                actionText: qsTr("Знайти постачальника  ·  F2")
                onActionTriggered: supplierPicker.open()
            }
        }

        // The chosen supplier: the debt in big figures and the payment form.
        Frame {
            visible: screen.hasSupplier
            Layout.preferredWidth: 320
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacingM

                PanelHeader {
                    title: Terminal.cashSupplierName
                    subtitle: qsTr("Борги спільні для всіх сесій")
                }

                // The debt, read from across the counter.
                RowLayout {
                    visible: screen.hasSupplier
                    Layout.fillWidth: true

                    Label {
                        text: Terminal.cashBalance.startsWith("−") ? qsTr("Винен нам, грн") : qsTr("Борг, грн")
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                    }
                    Label {
                        text: Terminal.cashBalance
                        font.pixelSize: Theme.fontSizeHeading
                        font.weight: Font.DemiBold
                        color: Terminal.cashBalance.startsWith("−") ? Theme.danger : Theme.text
                    }
                }
                Rectangle {
                    visible: screen.hasSupplier
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: Theme.border
                }

                ColumnLayout {
                    visible: screen.hasSupplier
                    spacing: Theme.spacingXs
                    Layout.fillWidth: true

                    FieldCaption {
                        caption: qsTr("Сума, грн")
                        hint: screen.balanceAfter !== "" ? qsTr("лишиться %1").arg(screen.balanceAfter) : ""
                    }
                    RowLayout {
                        spacing: Theme.spacingS
                        Layout.fillWidth: true

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
                }

                ColumnLayout {
                    visible: screen.hasSupplier
                    spacing: Theme.spacingXs
                    Layout.fillWidth: true

                    FieldCaption { caption: qsTr("Спосіб") }
                    RowLayout {
                        spacing: Theme.spacingL
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
                    highlighted: true
                    font.pixelSize: Theme.fontSizeField
                    enabled: amount.text !== ""
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.controlHeightLarge
                    onClicked: {
                        // Bad input is caught here, not after the operator confirms.
                        if (screen.balanceAfter === "")
                            paymentError.text = qsTr("Сума — число гривень, напр. 1500 або 1500,50")
                        else
                            confirmDialog.open()
                    }
                }
                ErrorLabel { id: paymentError }
                Item { Layout.fillHeight: true }
            }
        }

        // Where the balance comes from: receipts add, payments take away.
        Frame {
            visible: screen.hasSupplier
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacingM

                SectionCaption { text: qsTr("Історія розрахунків") }

                ListView {
                    id: history
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: Terminal.cashHistory
                    headerPositioning: ListView.OverlayHeader
                    ScrollBar.vertical: ScrollBar {}
                    // Newest record is what the operator just did; keep it in view.
                    onCountChanged: positionViewAtEnd()

                    // Column widths shared by the header and the rows; the record takes the rest.
                    readonly property int atWidth: 80
                    readonly property int effectWidth: 100

                    ListPlaceholder {
                        visible: history.count === 0
                        text: qsTr("Записів немає")
                    }

                    // Stays on top while the records scroll under it, so it needs a background.
                    header: Rectangle {
                        z: 2
                        width: ListView.view.width
                        implicitHeight: historyHeader.implicitHeight
                        visible: history.count > 0
                        color: Theme.surface

                        TableHeader {
                            id: historyHeader
                            width: parent.width
                            columns: [
                                { title: qsTr("Коли"), width: history.atWidth },
                                { title: qsTr("Запис") },
                                { title: qsTr("Сума, грн"), width: history.effectWidth, align: "right" }
                            ]
                        }
                    }

                    delegate: Rectangle {
                        id: record
                        required property var modelData
                        width: ListView.view.width
                        implicitHeight: recordRow.implicitHeight + 2 * Theme.spacingS
                        color: "transparent"

                        RowLayout {
                            id: recordRow
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Theme.spacingM

                            Label {
                                text: record.modelData.at
                                color: Theme.textMuted
                                Layout.preferredWidth: history.atWidth
                                Layout.alignment: Qt.AlignTop
                            }
                            // A receipt lists its lines under the headline.
                            ColumnLayout {
                                spacing: 2
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignTop

                                Label {
                                    text: record.modelData.title
                                    font.weight: Font.DemiBold
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                                CaptionLabel {
                                    visible: text !== ""
                                    text: record.modelData.detail
                                    wrapMode: Text.WordWrap
                                }
                                Button {
                                    visible: record.modelData.receiptId !== ""
                                    text: qsTr("Копія чека")
                                    onClicked: Terminal.printReceiptCopy(record.modelData.receiptId)
                                }
                            }
                            Label {
                                text: record.modelData.effect
                                color: record.modelData.payment ? Theme.ok : Theme.text
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: history.effectWidth
                                Layout.alignment: Qt.AlignTop
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

        GridLayout {
            anchors.fill: parent
            columns: 2
            columnSpacing: Theme.spacingXl

            Label { text: qsTr("Сума, грн"); color: Theme.textMuted }
            Label {
                text: amount.text
                font.pixelSize: Theme.fontSizeHeading
                font.weight: Font.DemiBold
                Layout.alignment: Qt.AlignRight
            }
            Label { text: qsTr("Спосіб"); color: Theme.textMuted }
            Label {
                text: cash.checked ? qsTr("готівкою") : qsTr("переказом")
                Layout.alignment: Qt.AlignRight
            }
            Label { text: qsTr("Борг після, грн"); color: Theme.textMuted }
            Label {
                text: screen.balanceAfter
                color: screen.overpays ? Theme.danger : Theme.text
                Layout.alignment: Qt.AlignRight
            }
            Label {
                visible: screen.overpays
                text: qsTr("Більше, ніж борг: постачальник буде винен нам")
                color: Theme.danger
                Layout.columnSpan: 2
            }
        }

        footer: ConfirmButtons { acceptText: qsTr("Виплатити") }

        onAccepted: {
            paymentError.text = Terminal.pay(amount.text, transfer.checked)
            if (paymentError.text === "") amount.clear()
        }
    }
}
