pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import BerryWms.Ui

// The working session: opened around noon, prices of the day, trucks to load,
// and closing once every truck has been sent off or carried over.
// The day so far in tiles on top, prices on the left, trucks as cards on the right.
Page {
    id: screen
    title: qsTr("Сесія")

    // An error under a form, on the same soft background as on the receiving screen.
    component ErrorLabel: Label {
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

    // Closed session: the only thing to do is to open a new one.
    EmptyState {
        anchors.centerIn: parent
        visible: !Terminal.sessionOpen
        icon: "calendar"
        title: qsTr("Сесію не відкрито")
        text: qsTr("Перенесені фури з попередньої сесії додадуться автоматично.")
        actionText: qsTr("Відкрити сесію")
        onActionTriggered: Terminal.openSession()
    }

    // One number of the day: a caption, the figure, and what it is made of.
    component StatTile: Rectangle {
        id: tile
        property string caption
        property string value
        property string unit
        property string detail
        property color detailColor: Theme.textMuted

        Layout.fillWidth: true
        Layout.preferredWidth: 1
        implicitHeight: tileColumn.implicitHeight + 2 * Theme.spacingM
        radius: Theme.radiusM
        color: Theme.surface
        border.color: Theme.border

        ColumnLayout {
            id: tileColumn
            anchors.fill: parent
            anchors.margins: Theme.spacingM
            anchors.leftMargin: Theme.spacingL
            spacing: Theme.spacingXs

            Label {
                text: tile.caption
                font.pixelSize: Theme.fontSizeCaption
                color: Theme.textMuted
            }
            RowLayout {
                spacing: Theme.spacingXs
                Label {
                    text: tile.value
                    font.pixelSize: Theme.fontSizeHeading
                    font.weight: Font.Bold
                }
                Label {
                    visible: tile.unit !== ""
                    text: tile.unit
                    font.pixelSize: Theme.fontSizeField
                    color: Theme.textMuted
                    Layout.alignment: Qt.AlignBaseline
                }
            }
            Label {
                text: tile.detail
                font.pixelSize: Theme.fontSizeCaption
                color: tile.detailColor
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingM
        visible: Terminal.sessionOpen
        spacing: Theme.spacingM

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingM

            Label {
                text: Terminal.sessionTitle
                font.pixelSize: Theme.fontSizeHeading
                font.weight: Font.Bold
            }
            Label {
                text: qsTr("ВІДКРИТА")
                font.pixelSize: Theme.fontSizeCaption
                font.weight: Font.DemiBold
                color: Theme.ok
                leftPadding: Theme.spacingS
                rightPadding: Theme.spacingS
                topPadding: Theme.spacingXs
                bottomPadding: Theme.spacingXs
                background: Rectangle {
                    radius: height / 2
                    color: Theme.okSoft
                }
            }
            Label {
                text: Terminal.sessionOpenedBy
                color: Theme.textMuted
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Закрити сесію…")
                enabled: Terminal.closeBlockers.length === 0
                onClicked: confirmClose.open()
            }
        }

        // The day so far: what came in, where it is, how much was paid out.
        RowLayout {
            readonly property var summary: Terminal.sessionSummary
            Layout.fillWidth: true
            spacing: Theme.spacingM

            StatTile {
                caption: qsTr("Прийнято")
                value: parent.summary.receivedKg ?? ""
                unit: qsTr("кг")
                detail: parent.summary.crops || qsTr("ще нічого")
            }
            StatTile {
                caption: qsTr("Ящиків")
                value: parent.summary.receivedBoxes ?? ""
                detail: qsTr("у фурах %1 · на складі %2 · чекає %3")
                        .arg(parent.summary.inTrucks).arg(parent.summary.inWarehouse).arg(parent.summary.waiting)
                detailColor: parent.summary.waiting > 0 ? Theme.warn : Theme.textMuted
            }
            StatTile {
                caption: qsTr("Чеків")
                value: (parent.summary.closedReceipts ?? 0) + (parent.summary.openReceipts ?? 0)
                detail: parent.summary.openReceipts > 0 ? qsTr("%1 відкрито").arg(parent.summary.openReceipts)
                                                        : qsTr("усі закрито")
                detailColor: parent.summary.openReceipts > 0 ? Theme.warn : Theme.textMuted
            }
            StatTile {
                caption: qsTr("Виплачено")
                value: parent.summary.paid ?? ""
                unit: qsTr("грн")
                detail: qsTr("готівка %1 · переказ %2").arg(parent.summary.paidCash).arg(parent.summary.paidTransfer)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingM

            // Prices of the day. A new price applies to weighings from now on.
            Frame {
                Layout.preferredWidth: 340
                Layout.fillHeight: true

                ColumnLayout {
                    id: prices
                    anchors.fill: parent
                    spacing: Theme.spacingS

                    // Every filled field at once: the operator usually sets all prices together.
                    signal applyAll()

                    SectionCaption { text: qsTr("Ціни дня, грн/кг") }

                    Repeater {
                        model: Terminal.crops

                        ColumnLayout {
                            id: priceRow
                            required property var modelData
                            readonly property string price: Terminal.prices[modelData.cropCode] ?? ""
                            spacing: Theme.spacingXs
                            Layout.fillWidth: true

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
                                spacing: Theme.spacingS
                                Layout.fillWidth: true

                                Label {
                                    text: priceRow.modelData.name
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                // The current price, big: this is what goes into the next weighing.
                                Label {
                                    text: priceRow.price !== "" ? priceRow.price : qsTr("не задано")
                                    font.pixelSize: priceRow.price !== "" ? Theme.fontSizeTitle : Theme.fontSizeCaption
                                    font.weight: Font.DemiBold
                                    color: priceRow.price !== "" ? Theme.text : Theme.warn
                                    horizontalAlignment: Text.AlignRight
                                    Layout.preferredWidth: 72
                                }
                                TextField {
                                    id: newPrice
                                    placeholderText: qsTr("нова")
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                    Layout.preferredWidth: 80
                                    onTextEdited: priceError.text = ""
                                    onAccepted: prices.applyAll()
                                }
                                // Rare, so last in the row: away from the price field the operator types into.
                                ToolButton {
                                    text: "✕"
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Прибрати культуру")
                                    onClicked: removeCropDialog.ask(priceRow.modelData.cropCode,
                                                                    priceRow.modelData.name)
                                }
                            }
                            ErrorLabel { id: priceError }
                        }
                    }

                    Button {
                        text: qsTr("Зберегти ціни")
                        highlighted: true
                        Layout.fillWidth: true
                        onClicked: prices.applyAll()
                    }
                    Label {
                        text: qsTr("Нова ціна діє для наступних зважувань; записані лишаються з попередньою.")
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.textMuted
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    // Crops change during the season: a new berry comes in, so it is added here.
                    Button {
                        text: qsTr("Додати культуру…")
                        flat: true
                        Layout.fillWidth: true
                        onClicked: cropDialog.open()
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            // Trucks of this session with their load, and the operator's decision for each.
            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spacingS

                    RowLayout {
                        Layout.fillWidth: true
                        SectionCaption {
                            text: truckList.count > 0 ? qsTr("Фури сесії · %1").arg(truckList.count) : qsTr("Фури сесії")
                            Layout.fillWidth: true
                        }
                        Button {
                            text: qsTr("Додати фуру…")
                            onClicked: truckDialog.open()
                        }
                    }

                    ListView {
                        id: truckList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: Theme.spacingS
                        model: Terminal.trucks
                        ScrollBar.vertical: ScrollBar {}

                        // Narrower in a small window, so the plate keeps its room.
                        readonly property real loadWidth: Math.min(280, width * 0.4)

                        EmptyState {
                            anchors.centerIn: parent
                            visible: truckList.count === 0
                            icon: "truck"
                            title: qsTr("Фур ще немає")
                            text: qsTr("Додайте фуру або повторіть учорашню — тоді палети з «Прийому» підуть одразу в неї.")
                        }

                        // A card per truck: who it is, how full, and what to do with it.
                        delegate: Rectangle {
                            id: truckCard
                            required property var modelData
                            width: ListView.view.width
                            implicitHeight: cardRow.implicitHeight + 2 * Theme.spacingM
                            radius: Theme.radiusM
                            color: Theme.surface
                            border.color: Theme.surfaceAlt

                            RowLayout {
                                id: cardRow
                                anchors.fill: parent
                                anchors.margins: Theme.spacingM
                                anchors.leftMargin: Theme.spacingL
                                spacing: Theme.spacingXl

                                ColumnLayout {
                                    spacing: 2
                                    Layout.fillWidth: true
                                    // The plate is never cut: the load bar gives way instead.
                                    Label {
                                        text: truckCard.modelData.plate
                                        font.pixelSize: Theme.fontSizeTitle
                                        font.weight: Font.Bold
                                        Layout.minimumWidth: implicitWidth
                                        Layout.fillWidth: true
                                    }
                                    Label {
                                        visible: text !== ""
                                        text: [truckCard.modelData.make, truckCard.modelData.note]
                                              .filter(part => part !== "").join(" · ")
                                        font.pixelSize: Theme.fontSizeCaption
                                        color: Theme.textMuted
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }

                                // Boxes decide, kilograms are for reference. Over the limit is said in red:
                                // the facts are accepted and the logistician sorts the overload out.
                                ColumnLayout {
                                    spacing: Theme.spacingXs
                                    Layout.preferredWidth: truckList.loadWidth
                                    Layout.maximumWidth: truckList.loadWidth

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label {
                                            text: truckCard.modelData.loadBoxes
                                            font.pixelSize: Theme.fontSizeField
                                            font.weight: Font.Bold
                                            color: truckCard.modelData.overloaded ? Theme.danger : Theme.text
                                        }
                                        Label {
                                            text: qsTr("/ %1 ящ.").arg(truckCard.modelData.limitBoxes)
                                            color: Theme.textMuted
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            text: truckCard.modelData.overloaded
                                                  ? qsTr("%1 кг · понад").arg(truckCard.modelData.loadKg)
                                                  : qsTr("%1 кг · %2%").arg(truckCard.modelData.loadKg)
                                                                     .arg(truckCard.modelData.fillPercent)
                                            font.pixelSize: Theme.fontSizeCaption
                                            color: truckCard.modelData.overloaded ? Theme.danger : Theme.textMuted
                                        }
                                    }
                                    ProgressBar {
                                        from: 0
                                        to: 100
                                        value: Math.min(truckCard.modelData.fillPercent, 100)
                                        Layout.fillWidth: true
                                    }
                                }

                                // A loading truck waits for the decision; a decided one shows it.
                                RowLayout {
                                    spacing: Theme.spacingS
                                    Button {
                                        visible: truckCard.modelData.loading
                                        text: qsTr("Відправити")
                                        onClicked: Terminal.departTruck(truckCard.modelData.truckId)
                                    }
                                    Button {
                                        visible: truckCard.modelData.loading
                                        text: qsTr("Перенести")
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("У наступну сесію з тим самим завантаженням")
                                        onClicked: Terminal.carryOverTruck(truckCard.modelData.truckId)
                                    }
                                    Label {
                                        visible: !truckCard.modelData.loading
                                        text: truckCard.modelData.status
                                        color: Theme.textMuted
                                    }
                                }
                            }
                        }
                    }

                    // Shown right away rather than on a failed click: the operator sees what is left to do.
                    // A to-do list, not an error: the warning tone, not the danger one.
                    Rectangle {
                        visible: Terminal.closeBlockers.length > 0
                        Layout.fillWidth: true
                        implicitHeight: blockerRow.implicitHeight + 2 * Theme.spacingM
                        radius: Theme.radiusM
                        color: Theme.warnSoft

                        RowLayout {
                            id: blockerRow
                            anchors.fill: parent
                            anchors.margins: Theme.spacingM
                            anchors.leftMargin: Theme.spacingL
                            spacing: Theme.spacingM

                            IconImage {
                                source: Theme.icon("circle-alert")
                                sourceSize: Qt.size(Theme.iconSize, Theme.iconSize)
                                color: Theme.warn
                                Layout.alignment: Qt.AlignTop
                            }
                            ColumnLayout {
                                spacing: Theme.spacingXs
                                Layout.fillWidth: true
                                Label {
                                    text: qsTr("Перед закриттям сесії")
                                    font.weight: Font.DemiBold
                                    color: Theme.warn
                                }
                                Repeater {
                                    model: Terminal.closeBlockers
                                    Label {
                                        required property string modelData
                                        text: modelData
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // A new truck (the limit is in boxes), or the same plate and limit as a
    // truck of the previous session on a new trip.
    Dialog {
        id: truckDialog
        anchors.centerIn: parent
        width: 420
        modal: true
        title: qsTr("Нова фура")
        standardButtons: Dialog.Cancel

        function add() {
            truckError.text = Terminal.addTruck(plate.text, make.text, note.text,
                                                boxLimit.text)
            if (truckError.text === "")
                close()
        }

        onOpened: {
            plate.clear()
            make.clear()
            note.clear()
            boxLimit.clear()
            truckError.text = ""
            plate.forceActiveFocus()
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.spacingS

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Theme.spacingM
                rowSpacing: Theme.spacingXs

                FieldCaption { caption: qsTr("Номер") }
                FieldCaption { caption: qsTr("Марка") }
                TextField {
                    id: plate
                    placeholderText: qsTr("BI1234AA")
                    Layout.fillWidth: true
                    onAccepted: truckDialog.add()
                }
                TextField {
                    id: make
                    Layout.fillWidth: true
                    onAccepted: truckDialog.add()
                }

                FieldCaption { caption: qsTr("Ліміт, ящиків"); Layout.columnSpan: 2 }
                TextField {
                    id: boxLimit
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 1 }
                    Layout.fillWidth: true
                    onAccepted: truckDialog.add()
                }
                Item { Layout.fillWidth: true }

                FieldCaption { caption: qsTr("Водій / куди їде"); Layout.columnSpan: 2 }
                TextField {
                    id: note
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    onAccepted: truckDialog.add()
                }
            }
            Button {
                text: qsTr("Додати фуру")
                highlighted: true
                Layout.fillWidth: true
                onClicked: truckDialog.add()
            }

            ColumnLayout {
                visible: Terminal.previousTrucks.length > 0
                spacing: Theme.spacingS
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingM

                SectionCaption { text: qsTr("Або повторити з минулої сесії") }
                RowLayout {
                    spacing: Theme.spacingS
                    Layout.fillWidth: true

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
                        text: qsTr("Повторити")
                        enabled: previousTruck.currentIndex >= 0
                        onClicked: {
                            truckError.text = Terminal.repeatTruck(previousTruck.currentValue)
                            if (truckError.text === "")
                                truckDialog.close()
                        }
                    }
                }
            }

            ErrorLabel { id: truckError }
        }
    }

    // A crop is just a name; the same name in other letter case is refused while typing.
    Dialog {
        id: cropDialog
        anchors.centerIn: parent
        width: 360
        modal: true
        title: qsTr("Нова культура")
        standardButtons: Dialog.Cancel

        readonly property var errors: cropName.text.trim() === "" ? [] : Terminal.newCropErrors(cropName.text)

        function add() {
            if (cropName.text.trim() === "" || errors.length > 0)
                return
            cropError.text = Terminal.addCrop(cropName.text)
            if (cropError.text === "")
                close()
        }

        onOpened: {
            cropName.clear()
            cropError.text = ""
            cropName.forceActiveFocus()
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.spacingS

            FieldCaption { caption: qsTr("Назва") }
            TextField {
                id: cropName
                placeholderText: qsTr("Агрус")
                Layout.fillWidth: true
                onTextEdited: cropError.text = ""
                onAccepted: cropDialog.add()
            }
            ErrorLabel { text: cropDialog.errors.join("\n") }
            Button {
                text: qsTr("Додати культуру")
                highlighted: true
                enabled: cropName.text.trim() !== "" && cropDialog.errors.length === 0
                Layout.fillWidth: true
                onClicked: cropDialog.add()
            }
            ErrorLabel { id: cropError }
        }
    }

    // Says beforehand what removing does: a crop nothing names is deleted
    // (a typo), one with records is only hidden and can be added back.
    Dialog {
        id: removeCropDialog
        anchors.centerIn: parent
        width: 380
        modal: true
        title: qsTr("Прибрати «%1»?").arg(removedName)

        property string removedCode
        property string removedName
        property var removal: ({ errors: [], keepsRecords: false })

        function ask(code, name) {
            removedCode = code
            removedName = name
            removal = Terminal.cropRemoval(code)
            removeCropError.text = ""
            open()
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.spacingS

            Label {
                visible: removeCropDialog.removal.errors.length === 0
                text: removeCropDialog.removal.keepsRecords
                      ? qsTr("На цій культурі вже є записи, тому її буде приховано, а не видалено: старі чеки "
                             + "й надалі показуватимуть назву. Повернути — «Додати культуру…» з тією ж назвою.")
                      : qsTr("Записів на цій культурі нема — її буде видалено.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            ErrorLabel { text: removeCropDialog.removal.errors.join("\n") }
            ErrorLabel { id: removeCropError }
        }

        footer: DialogButtonBox {
            Button {
                text: removeCropDialog.removal.keepsRecords ? qsTr("Приховати") : qsTr("Видалити")
                enabled: removeCropDialog.removal.errors.length === 0
                highlighted: true
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
            Button {
                text: qsTr("Скасувати")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
        }
        onAccepted: {
            const error = Terminal.removeCrop(removedCode)
            if (error !== "") {
                removeCropError.text = error
                open()
            }
        }
    }

    Dialog {
        id: confirmClose
        anchors.centerIn: parent
        modal: true
        title: qsTr("Закрити сесію?")

        Label {
            text: qsTr("Після закриття змінити сесію можна лише поправкою з поясненням.")
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Закрити сесію")
                highlighted: true
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
            Button {
                text: qsTr("Скасувати")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
        }
        onAccepted: Terminal.closeSession()
    }
}
