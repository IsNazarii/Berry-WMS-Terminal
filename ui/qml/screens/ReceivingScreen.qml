pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BerryWms.Ui

// The main screen: open receipts on the left, the scale in the middle, the
// active receipt on the right. Every weighing goes into the active receipt.
// Layout follows the HTML mockup (see CLAUDE.md, "Стиль").
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

    // Small uppercase caption above a group: "ВІДКРИТІ ЧЕКИ", "КУЛЬТУРА".
    component SectionCaption: Label {
        font.pixelSize: Theme.fontSizeCaption
        font.weight: Font.DemiBold
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 0.8
        color: Theme.textMuted
    }

    // A field caption with its hint on the same line, so every field of a row
    // starts at the same height whatever the hint's length.
    component FieldCaption: RowLayout {
        id: fieldCaption
        required property string caption
        property string hint: ""
        spacing: Theme.spacingS
        Layout.fillWidth: true

        Label {
            text: fieldCaption.caption
            font.pixelSize: Theme.fontSizeCaption
            font.weight: Font.Medium
            color: Theme.textMuted
        }
        Label {
            text: fieldCaption.hint
            font.pixelSize: Theme.fontSizeCaption
            color: Theme.textMuted
            opacity: 0.8
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignRight
            Layout.fillWidth: true
        }
    }

    // A crop is one press: its name and today's price on a big key.
    component CropButton: AbstractButton {
        id: cropButton
        property string caption
        property bool selected: false

        implicitHeight: Theme.controlHeightLarge
        implicitWidth: 96
        Layout.fillWidth: true
        opacity: enabled ? 1 : Theme.disabledOpacity

        contentItem: ColumnLayout {
            spacing: 0
            Label {
                text: cropButton.text
                font.weight: Font.DemiBold
                color: cropButton.selected ? Theme.textOnAccent : Theme.text
                elide: Text.ElideRight
                Layout.alignment: Qt.AlignHCenter
                Layout.maximumWidth: cropButton.availableWidth
            }
            Label {
                text: cropButton.caption
                font.pixelSize: Theme.fontSizeCaption
                color: cropButton.selected ? Theme.textOnAccent : Theme.textMuted
                opacity: cropButton.selected ? 0.85 : 1
                Layout.alignment: Qt.AlignHCenter
            }
        }

        background: Rectangle {
            radius: Theme.radiusS
            color: cropButton.selected ? Theme.accent : cropButton.hovered ? Theme.surfaceAlt : Theme.surface
            border.width: cropButton.visualFocus ? 2 : 1
            border.color: cropButton.visualFocus ? Theme.text : cropButton.selected ? Theme.accent : Theme.border
        }
    }

    // One lamp of the scale indicator: СТАБ, НУЛЬ, БРУТТО.
    component Lamp: RowLayout {
        id: lamp
        required property string caption
        property bool lit: false
        readonly property color litColor: screen.scales.stable ? Theme.scaleDigit : Theme.scaleDigitUnstable
        spacing: 6

        Rectangle {
            implicitWidth: 8
            implicitHeight: 8
            radius: 4
            color: lamp.lit ? lamp.litColor : Theme.scaleDim
        }
        Label {
            text: lamp.caption
            font.pixelSize: 11
            font.weight: Font.DemiBold
            font.letterSpacing: 1
            color: lamp.lit ? lamp.litColor : Theme.scaleDim
        }
    }

    Label {
        anchors.centerIn: parent
        visible: !Terminal.sessionOpen
        text: qsTr("Прийом можливий лише у відкритій сесії")
        font.pixelSize: Theme.fontSizeLarge
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingM
        visible: Terminal.sessionOpen
        spacing: Theme.spacingM

        // Open receipts: two big suppliers unloading in turns, a retail one in between.
        Frame {
            Layout.preferredWidth: 220
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacingM

                Button {
                    text: qsTr("Новий чек  ·  F2")
                    highlighted: true
                    Layout.fillWidth: true
                    onClicked: screen.newReceipt()
                }
                SectionCaption { text: qsTr("Відкриті чеки") }

                ListView {
                    id: receiptList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: Theme.spacingXs
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
                            spacing: 2
                            Label {
                                text: receiptRow.modelData.supplierName
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            RowLayout {
                                Label {
                                    text: receiptRow.modelData.openedAt
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: Theme.textMuted
                                }
                                Label {
                                    text: receiptRow.modelData.boxes > 0 ? qsTr("%1 грн").arg(receiptRow.modelData.amount)
                                                                         : qsTr("порожній")
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: Theme.textMuted
                                    elide: Text.ElideRight
                                    horizontalAlignment: Text.AlignRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }
            }
        }

        // The scale and the weighing form.
        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                id: weighingForm
                anchors.fill: parent
                spacing: Theme.spacingM

                property string cropCode: Terminal.crops.length > 0 ? Terminal.crops[0].cropCode : ""
                readonly property string dayPrice: Terminal.prices[cropCode] ?? ""

                // Up to five crops are keys, the rest wait behind "Ще": crops with a
                // day price come first, since those are the ones received today.
                // With six or fewer there is no point in a menu, all are keys.
                readonly property var cropKeys: {
                    const all = Terminal.crops
                    if (all.length <= 6)
                        return { keys: all, more: [] }
                    const priced = all.filter(c => Terminal.prices[c.cropCode] !== undefined)
                    const unpriced = all.filter(c => Terminal.prices[c.cropCode] === undefined)
                    const ordered = priced.concat(unpriced)
                    return { keys: ordered.slice(0, 5), more: ordered.slice(5) }
                }
                readonly property var pickedFromMore:
                    cropKeys.more.find(c => c.cropCode === cropCode) ?? null

                function priceCaption(code) {
                    const price = Terminal.prices[code]
                    return price !== undefined ? qsTr("%1 грн").arg(price) : qsTr("ціни немає")
                }

                // Crop and box tare are usually the same for the next pallet, so they
                // stay; the pallet, the price raise and a typed weight are per weighing.
                function record(acceptUnstable) {
                    weighingError.text = Terminal.recordWeighing(cropCode, boxes.text, boxTare.text,
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

                // The indicator is an instrument: dark in both themes, lamps like a real one.
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: readout.implicitHeight + 2 * Theme.spacingL
                    radius: Theme.radiusM
                    color: Theme.scaleBg

                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.spacingXl
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Theme.spacingS

                        Lamp { caption: qsTr("СТАБ"); lit: screen.scales.stable }
                        Lamp { caption: qsTr("НУЛЬ"); lit: screen.scales.zero }
                        Lamp { caption: qsTr("БРУТТО"); lit: true }
                    }

                    RowLayout {
                        id: readout
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.spacingXl
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Theme.spacingS

                        Label {
                            text: screen.scales.display
                            font.pixelSize: Theme.fontSizeScale
                            font.weight: Font.DemiBold
                            color: screen.scales.stable ? Theme.scaleDigit : Theme.scaleDigitUnstable
                        }
                        Label {
                            text: qsTr("кг")
                            font.pixelSize: Theme.fontSizeLarge
                            color: screen.scales.stable ? Theme.scaleDigit : Theme.scaleDigitUnstable
                            opacity: 0.8
                            Layout.alignment: Qt.AlignBaseline
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: screen.scales.driverName
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.textMuted
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
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

                // What stays between pallets: the crop, its price, the box tare.
                ColumnLayout {
                    Layout.fillWidth: true
                    enabled: screen.hasReceipt
                    spacing: Theme.spacingS

                    SectionCaption { text: qsTr("Культура") }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingS

                        Repeater {
                            model: weighingForm.cropKeys.keys
                            CropButton {
                                required property var modelData
                                text: modelData.name
                                caption: weighingForm.priceCaption(modelData.cropCode)
                                selected: weighingForm.cropCode === modelData.cropCode
                                onClicked: weighingForm.cropCode = modelData.cropCode
                            }
                        }
                        CropButton {
                            visible: weighingForm.cropKeys.more.length > 0
                            readonly property var picked: weighingForm.pickedFromMore
                            text: picked ? picked.name + " ▾" : qsTr("Ще ▾")
                            caption: picked ? weighingForm.priceCaption(picked.cropCode)
                                            : qsTr("ще %1").arg(weighingForm.cropKeys.more.length)
                            selected: picked !== null
                            onClicked: moreCrops.open()

                            Popup {
                                id: moreCrops
                                y: parent.height + Theme.spacingXs
                                width: Math.max(parent.width, 220)
                                padding: Theme.spacingXs

                                contentItem: ListView {
                                    implicitHeight: contentHeight
                                    model: weighingForm.cropKeys.more
                                    delegate: ItemDelegate {
                                        id: moreRow
                                        required property var modelData
                                        width: ListView.view.width
                                        highlighted: weighingForm.cropCode === modelData.cropCode
                                        onClicked: {
                                            weighingForm.cropCode = modelData.cropCode
                                            moreCrops.close()
                                        }
                                        contentItem: RowLayout {
                                            Label {
                                                text: moreRow.modelData.name
                                                Layout.fillWidth: true
                                            }
                                            Label {
                                                text: weighingForm.priceCaption(moreRow.modelData.cropCode)
                                                font.pixelSize: Theme.fontSizeCaption
                                                color: Theme.textMuted
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Captions in the first row, fields in the second: the grid keeps
                    // every field of a row on one line.
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: Theme.spacingM
                        rowSpacing: Theme.spacingXs

                        FieldCaption { caption: qsTr("Ціна, грн/кг"); hint: qsTr("порожньо — денна") }
                        FieldCaption { caption: qsTr("Тара ящика, кг") }
                        TextField {
                            id: price
                            Layout.fillWidth: true
                            // Empty means the day price; anything typed is a one-off raise for this weighing.
                            placeholderText: weighingForm.dayPrice !== "" ? weighingForm.dayPrice : qsTr("введіть ціну")
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                        }
                        TextField {
                            id: boxTare
                            Layout.fillWidth: true
                            placeholderText: qsTr("напр. 0,35")
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: Theme.border
                }

                // What is typed for every pallet; right above the record button.
                GridLayout {
                    Layout.fillWidth: true
                    enabled: screen.hasReceipt
                    columns: 3
                    columnSpacing: Theme.spacingM
                    rowSpacing: Theme.spacingXs

                    FieldCaption { caption: qsTr("Ящиків") }
                    FieldCaption { caption: qsTr("Палета, кг"); hint: qsTr("0 — без палети") }
                    FieldCaption { caption: qsTr("Вага вручну, кг"); hint: qsTr("порожньо — з ваг") }
                    TextField {
                        id: boxes
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.controlHeightLarge
                        font.pixelSize: Theme.fontSizeLarge
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 1 }
                        onAccepted: weighingForm.record(false)
                    }
                    TextField {
                        id: palletTare
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.controlHeightLarge
                        font.pixelSize: Theme.fontSizeLarge
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        onAccepted: weighingForm.record(false)
                    }
                    TextField {
                        id: manualWeight
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.controlHeightLarge
                        font.pixelSize: Theme.fontSizeLarge
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    enabled: screen.hasReceipt
                    spacing: Theme.spacingS

                    Button {
                        text: qsTr("Записати зважування  ·  Enter")
                        highlighted: true
                        font.pixelSize: Theme.fontSizeField
                        enabled: screen.scales.stable || manualWeight.text.trim() !== ""
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.controlHeightLarge
                        onClicked: weighingForm.record(false)
                    }
                    // A separate, deliberate action: the scale may keep flipping between two steps.
                    WarningButton {
                        text: qsTr("Прийняти нестабільну")
                        enabled: !screen.scales.stable && manualWeight.text.trim() === ""
                        Layout.preferredHeight: Theme.controlHeightLarge
                        onClicked: weighingForm.record(true)
                    }
                }

                Label {
                    id: weighingError
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

        // The active receipt: one table row per weighing.
        Frame {
            Layout.preferredWidth: 440
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacingM

                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true

                    Label {
                        text: screen.hasReceipt ? Terminal.supplierName : qsTr("Чек не обрано")
                        font.pixelSize: Theme.fontSizeTitle
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: screen.hasReceipt
                              ? qsTr("Відкрито %1 · борг до чека %2").arg(Terminal.activeSummary.openedAt)
                                                                     .arg(Terminal.supplierBalance)
                              : qsTr("Новий чек — F2")
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.textMuted
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                ListView {
                    id: lineList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: Terminal.activeLines
                    headerPositioning: ListView.OverlayHeader
                    ScrollBar.vertical: ScrollBar {}

                    // Column widths shared by the header and the rows; the crop takes the rest.
                    readonly property int indexWidth: 20
                    readonly property int boxesWidth: 32
                    readonly property int netWidth: 64
                    readonly property int priceWidth: 56
                    readonly property int amountWidth: 80
                    readonly property int actionWidth: 28
                    readonly property int columnSpacing: 6

                    Label {
                        anchors.centerIn: parent
                        visible: screen.hasReceipt && lineList.count === 0
                        text: qsTr("Зважувань ще немає")
                        color: Theme.textMuted
                    }

                    header: Rectangle {
                        z: 2
                        width: ListView.view.width
                        height: 28
                        visible: lineList.count > 0
                        color: Theme.surface

                        component HeaderCell: Label {
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            font.capitalization: Font.AllUppercase
                            color: Theme.textMuted
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                        }

                        RowLayout {
                            anchors.fill: parent
                            spacing: lineList.columnSpacing
                            HeaderCell { text: qsTr("№"); horizontalAlignment: Text.AlignLeft; Layout.preferredWidth: lineList.indexWidth }
                            HeaderCell { text: qsTr("Культура"); horizontalAlignment: Text.AlignLeft; Layout.fillWidth: true }
                            HeaderCell { text: qsTr("Ящ."); Layout.preferredWidth: lineList.boxesWidth }
                            HeaderCell { text: qsTr("Нетто, кг"); Layout.preferredWidth: lineList.netWidth }
                            HeaderCell { text: qsTr("Ціна"); Layout.preferredWidth: lineList.priceWidth }
                            HeaderCell { text: qsTr("Сума, грн"); Layout.preferredWidth: lineList.amountWidth }
                            Item { Layout.preferredWidth: lineList.actionWidth }
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
                        readonly property bool cancelled: modelData.cancelled
                        width: ListView.view.width
                        height: 36
                        color: rowHover.hovered ? Theme.surfaceAlt : "transparent"

                        HoverHandler { id: rowHover }

                        // A cancelled line stays visible, struck through: the trace of the mistake is kept.
                        // A value typed by hand is marked in the warning color with an asterisk.
                        component Cell: Label {
                            property bool manual: false
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                            font.strikeout: line.cancelled
                            color: line.cancelled ? Theme.textMuted : manual ? Theme.warn : Theme.text
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
                            // Gross and tare are there on hover: the table stays one line per pallet.
                            Cell {
                                text: line.modelData.net + (manual ? "*" : "")
                                manual: line.modelData.manualWeight
                                Layout.preferredWidth: lineList.netWidth
                                HoverHandler { id: netHover }
                                ToolTip.visible: netHover.hovered
                                ToolTip.text: qsTr("брутто %1 − тара %2").arg(line.modelData.gross).arg(line.modelData.tare)
                            }
                            Cell {
                                text: line.modelData.price + (manual ? "*" : "")
                                manual: line.modelData.manualPrice
                                Layout.preferredWidth: lineList.priceWidth
                            }
                            Cell {
                                text: line.modelData.amount
                                font.weight: Font.DemiBold
                                Layout.preferredWidth: lineList.amountWidth
                            }
                            Item {
                                Layout.preferredWidth: lineList.actionWidth
                                Layout.fillHeight: true

                                ToolButton {
                                    anchors.centerIn: parent
                                    visible: !line.cancelled
                                    text: "✕"
                                    opacity: rowHover.hovered || activeFocus ? 1 : 0.5
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Анулювати зважування")
                                    onClicked: receiptError.text = Terminal.cancelWeighing(line.modelData.index)
                                }
                                Label {
                                    anchors.centerIn: parent
                                    visible: line.cancelled
                                    text: qsTr("анул.")
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: Theme.textMuted
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

                Label {
                    visible: lineList.count > 0
                    text: qsTr("<font color=\"%1\">123*</font> — введено вручну; наведіть на нетто, щоб побачити брутто і тару")
                          .arg(Theme.warn)
                    textFormat: Text.StyledText
                    font.pixelSize: Theme.fontSizeCaption
                    color: Theme.textMuted
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Label {
                    id: receiptError
                    visible: text !== ""
                    color: Theme.danger
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: Theme.border
                    visible: screen.hasReceipt
                }
                GridLayout {
                    Layout.fillWidth: true
                    visible: screen.hasReceipt
                    columns: 2
                    rowSpacing: Theme.spacingXs

                    Label { text: qsTr("Ящиків"); color: Theme.textMuted; Layout.fillWidth: true }
                    Label { text: Terminal.activeSummary.boxes ?? ""; Layout.alignment: Qt.AlignRight }
                    Label { text: qsTr("Нетто, кг"); color: Theme.textMuted }
                    Label { text: Terminal.activeSummary.net ?? ""; Layout.alignment: Qt.AlignRight }
                    Label { text: qsTr("Разом, грн"); font.weight: Font.DemiBold }
                    Label {
                        text: Terminal.activeSummary.amount ?? ""
                        font.pixelSize: Theme.fontSizeHeading
                        Layout.alignment: Qt.AlignRight
                        font.weight: Font.DemiBold
                    }
                }
                Button {
                    text: qsTr("Закрити чек…")
                    enabled: screen.hasReceipt
                    Layout.fillWidth: true
                    onClicked: closeDialog.open()
                }
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
                highlighted: true
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
