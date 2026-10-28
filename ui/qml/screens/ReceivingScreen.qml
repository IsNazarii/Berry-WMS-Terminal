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

    // No session: the main window switches to the session screen.
    signal sessionRequested()
    title: qsTr("Прийом")

    readonly property ScaleMonitor scales: Terminal.scales
    readonly property bool hasReceipt: Terminal.activeReceiptId !== ""

    function newReceipt() { supplierPicker.open() }

    Shortcut {
        sequence: "F2"
        enabled: screen.visible && Terminal.sessionOpen
        onActivated: screen.newReceipt()
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

    NoSessionNotice {
        visible: !Terminal.sessionOpen
        text: qsTr("Прийом можливий лише у відкритій сесії.")
        onSessionRequested: screen.sessionRequested()
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
                readonly property var placement: Terminal.nextPlacements[cropCode] ?? {}
                // A truck with fewer free boxes than typed: the pallet will wait on the allocation screen.
                readonly property bool wontFit: placement.boxesLeft !== undefined && placement.boxesLeft >= 0
                                                && parseInt(boxes.text) > placement.boxesLeft

                // Five crops are keys, the rest wait behind "Ще"; which five is
                // Terminal.cropKeys (a crop used from the menu replaces the key used
                // longest ago). With six or fewer there is no point in a menu, all are keys.
                readonly property var cropKeys: {
                    const all = Terminal.crops
                    if (all.length <= 6)
                        return { keys: all, more: [] }
                    const keyCodes = Terminal.cropKeys
                    return {
                        keys: keyCodes.map(code => all.find(c => c.cropCode === code)).filter(c => c !== undefined),
                        more: all.filter(c => !keyCodes.includes(c.cropCode))
                    }
                }
                readonly property var pickedFromMore:
                    cropKeys.more.find(c => c.cropCode === cropCode) ?? null

                function priceCaption(code) {
                    const price = Terminal.prices[code]
                    return price !== undefined ? qsTr("%1 грн").arg(price) : qsTr("ціни немає")
                }

                // Crop and box tare are usually the same for the next pallet, so they
                // stay; the pallet and a typed weight are per weighing.
                function record(acceptUnstable) {
                    weighingError.text = Terminal.recordWeighing(cropCode, boxes.text, boxTare.text,
                                                                 palletTare.text, manualWeight.text, acceptUnstable)
                    if (weighingError.text === "") {
                        boxes.clear()
                        palletTare.clear()
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
                                            Terminal.useCrop(modelData.cropCode)
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

                        FieldCaption { caption: qsTr("Тара ящика, кг") }
                        FieldCaption { caption: qsTr("Ціль"); hint: qsTr("фура або склад") }
                        TextField {
                            id: boxTare
                            Layout.fillWidth: true
                            placeholderText: qsTr("напр. 0,35")
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                        }
                        // Where a crop's first pallet in the receipt goes; the next ones follow it.
                        ComboBox {
                            id: target
                            Layout.fillWidth: true
                            model: [{ destinationId: "", title: qsTr("Без цілі") }].concat(Terminal.destinations)
                            textRole: "title"
                            valueRole: "destinationId"
                            // Naming the model makes the binding re-run when it is rebuilt.
                            currentIndex: model.findIndex(d => d.destinationId === Terminal.receivingTarget)
                            onActivated: Terminal.setReceivingTarget(currentValue)
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

                // Said before the pallet is weighed: the operator must see where it goes.
                Label {
                    visible: screen.hasReceipt && weighingForm.cropCode !== ""
                    readonly property bool warn: weighingForm.placement.hangs || weighingForm.wontFit
                    text: (weighingForm.wontFit ? qsTr("Не влізе: %1 — палета чекатиме на «Розподілі»")
                                                : qsTr("Палета піде %1"))
                              .arg(weighingForm.placement.text ?? "")
                          + (weighingForm.placement.follows ? qsTr(" (як попередня цієї культури в чеку)") : "")
                    color: warn ? Theme.warn : Theme.textMuted
                    font.weight: warn ? Font.DemiBold : Font.Normal
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                RowLayout {
                    id: recordActions
                    Layout.fillWidth: true
                    enabled: screen.hasReceipt
                    spacing: Theme.spacingS

                    // Neither button gets elided: on a narrow screen the main action says less.
                    Button {
                        text: recordActions.width < 520 ? qsTr("Записати  ·  Enter") : qsTr("Записати зважування  ·  Enter")
                        Layout.minimumWidth: implicitWidth
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
                        Layout.minimumWidth: implicitWidth
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


                        RowLayout {
                            anchors.fill: parent
                            spacing: lineList.columnSpacing
                            ColumnHeader { text: qsTr("№"); horizontalAlignment: Text.AlignLeft; Layout.preferredWidth: lineList.indexWidth }
                            ColumnHeader { text: qsTr("Культура"); horizontalAlignment: Text.AlignLeft; Layout.fillWidth: true }
                            ColumnHeader { text: qsTr("Ящ."); Layout.preferredWidth: lineList.boxesWidth }
                            ColumnHeader { text: qsTr("Нетто, кг"); Layout.preferredWidth: lineList.netWidth }
                            ColumnHeader { text: qsTr("Ціна"); Layout.preferredWidth: lineList.priceWidth }
                            ColumnHeader { text: qsTr("Сума, грн"); Layout.preferredWidth: lineList.amountWidth }
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
                        height: 44
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
                            // Where the pallet went, under its crop: no room for one more column.
                            ColumnLayout {
                                spacing: 0
                                Layout.fillWidth: true
                                Cell {
                                    text: line.modelData.crop
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignLeft
                                    Layout.fillWidth: true
                                }
                                Cell {
                                    visible: !line.cancelled
                                    text: line.modelData.hanging ? qsTr("чекає розподілу") : "→ " + line.modelData.placedIn
                                    font.pixelSize: Theme.fontSizeCaption
                                    font.weight: line.modelData.hanging ? Font.DemiBold : Font.Normal
                                    color: line.modelData.hanging ? Theme.warn : Theme.textMuted
                                    horizontalAlignment: Text.AlignLeft
                                    Layout.fillWidth: true
                                }
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
                // Retail is paid here and now; wholesale only gets the receipt.
                RowLayout {
                    Layout.fillWidth: true
                    enabled: screen.hasReceipt

                    Button {
                        text: qsTr("Оплатити…")
                        highlighted: true
                        Layout.fillWidth: true
                        onClicked: payDialog.open()
                    }
                    Button {
                        text: qsTr("Видати чек…")
                        Layout.fillWidth: true
                        onClicked: handOverDialog.open()
                    }
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
    // Retail: one price per crop for all its pallets, then the whole amount is paid out.
    Dialog {
        id: payDialog
        anchors.centerIn: parent
        width: 520
        modal: true
        title: qsTr("Оплатити: %1").arg(Terminal.supplierName)

        // Read once on opening: rebuilding the rows while typing would wipe the fields.
        property var rows: []
        property var typed: ({})
        readonly property var check: Terminal.payCheck(typed)

        function typePrice(cropCode, text) {
            const next = Object.assign({}, typed)
            next[cropCode] = text
            typed = next
        }

        onAboutToShow: {
            rows = Terminal.payRows()
            const prices = {}
            for (const row of rows)
                prices[row.cropCode] = row.price
            typed = prices
            cash.checked = true
            payError.text = ""
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.spacingS

            GridLayout {
                Layout.fillWidth: true
                columns: 4
                columnSpacing: Theme.spacingM
                rowSpacing: Theme.spacingXs

                ColumnHeader { text: qsTr("Культура"); horizontalAlignment: Text.AlignLeft; Layout.fillWidth: true }
                ColumnHeader { text: qsTr("Нетто, кг"); horizontalAlignment: Text.AlignRight }
                ColumnHeader { text: qsTr("Ціна, грн/кг"); horizontalAlignment: Text.AlignLeft }
                ColumnHeader { text: qsTr("Сума, грн"); horizontalAlignment: Text.AlignRight }

                Repeater {
                    model: payDialog.rows
                    delegate: Label {
                        required property var modelData
                        required property int index
                        // Four cells per row of the grid, one Repeater per column keeps them in order.
                        text: modelData.crop
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        Layout.row: index + 1
                        Layout.column: 0
                    }
                }
                Repeater {
                    model: payDialog.rows
                    delegate: Label {
                        required property var modelData
                        required property int index
                        text: modelData.net
                        Layout.alignment: Qt.AlignRight
                        Layout.row: index + 1
                        Layout.column: 1
                    }
                }
                Repeater {
                    model: payDialog.rows
                    delegate: TextField {
                        required property var modelData
                        required property int index
                        // The day price is filled in; typing another one makes it manual for the whole crop.
                        text: modelData.price
                        placeholderText: qsTr("введіть ціну")
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        Layout.preferredWidth: Theme.controlHeightLarge * 2
                        Layout.row: index + 1
                        Layout.column: 2
                        onTextEdited: payDialog.typePrice(modelData.cropCode, text)
                    }
                }
                Repeater {
                    model: payDialog.rows
                    delegate: Label {
                        required property var modelData
                        required property int index
                        text: payDialog.check.amounts?.[modelData.cropCode] ?? "—"
                        Layout.alignment: Qt.AlignRight
                        Layout.row: index + 1
                        Layout.column: 3
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("До виплати, грн"); font.weight: Font.DemiBold; Layout.fillWidth: true }
                Label {
                    text: payDialog.check.total || "—"
                    font.pixelSize: Theme.fontSizeHeading
                    font.weight: Font.DemiBold
                }
            }

            RowLayout {
                RadioButton { id: cash; text: qsTr("Готівка") }
                RadioButton { id: transfer; text: qsTr("Переказ") }
            }

            Repeater {
                model: payDialog.check.blockers ?? []
                Label {
                    required property string modelData
                    text: "• " + modelData
                    color: Theme.danger
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
            Label {
                id: payError
                visible: text !== ""
                color: Theme.danger
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Виплатити")
                highlighted: true
                enabled: (payDialog.check.blockers ?? []).length === 0
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
            Button {
                text: qsTr("Скасувати")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
        }

        // Kept open on an error, so the typed prices stay.
        onAccepted: {
            payError.text = Terminal.payReceipt(typed, transfer.checked)
            if (payError.text !== "")
                open()
            else
                receiptError.text = ""
        }
    }

    // Wholesale: weight only, priced and paid outside the terminal.
    Dialog {
        id: handOverDialog
        anchors.centerIn: parent
        width: 420
        modal: true
        title: qsTr("Видати чек: %1").arg(Terminal.supplierName)

        property var blockers: []
        onAboutToShow: blockers = Terminal.handOverBlockers()

        ColumnLayout {
            anchors.fill: parent

            Label {
                text: qsTr("Опт: %1 ящ., нетто %2 кг. Ціни в чеку не буде — його отримує відповідальна особа.")
                      .arg(Terminal.activeSummary.boxes ?? 0).arg(Terminal.activeSummary.net ?? "")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Repeater {
                model: handOverDialog.blockers
                Label {
                    required property string modelData
                    text: "• " + modelData
                    color: Theme.danger
                }
            }
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Видати чек")
                highlighted: true
                enabled: handOverDialog.blockers.length === 0
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
            Button {
                text: qsTr("Скасувати")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
        }

        onAccepted: {
            Terminal.handOverReceipt()
            receiptError.text = ""
        }
    }
}
