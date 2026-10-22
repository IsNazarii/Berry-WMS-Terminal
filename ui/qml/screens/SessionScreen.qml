pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BerryWms.Ui

// The working session: opened around noon, prices of the day, trucks to load,
// and closing once every truck has been sent off or carried over.
// Prices on the left, the trucks in the middle, a new truck on the right.
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
    Frame {
        anchors.centerIn: parent
        visible: !Terminal.sessionOpen
        padding: Theme.spacingXl

        ColumnLayout {
            spacing: Theme.spacingM

            Label {
                text: qsTr("Сесію не відкрито")
                font.pixelSize: Theme.fontSizeHeading
                font.weight: Font.Bold
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
                font.pixelSize: Theme.fontSizeField
                Layout.preferredHeight: Theme.controlHeightLarge
                Layout.alignment: Qt.AlignHCenter
                onClicked: Terminal.openSession()
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

        // Shown right away rather than on a failed click: the operator sees what is left to do.
        // A to-do list, not an error: the warning tone, not the danger one.
        Label {
            visible: Terminal.closeBlockers.length > 0
            text: qsTr("Щоб закрити сесію:") + "\n• " + Terminal.closeBlockers.join("\n• ")
            color: Theme.warn
            wrapMode: Text.WordWrap
            padding: Theme.spacingS
            Layout.fillWidth: true
            background: Rectangle {
                color: Theme.surfaceAlt
                radius: Theme.radiusS
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingM

            // Prices of the day. A new price applies to weighings from now on.
            Frame {
                Layout.preferredWidth: 280
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
                                    Layout.preferredWidth: 80
                                }
                                TextField {
                                    id: newPrice
                                    placeholderText: qsTr("нова")
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                    Layout.preferredWidth: 80
                                    onTextEdited: priceError.text = ""
                                    onAccepted: prices.applyAll()
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
                            text: qsTr("Фури сесії")
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
                        model: Terminal.trucks
                        headerPositioning: ListView.OverlayHeader
                        ScrollBar.vertical: ScrollBar {}

                        // Column widths shared by the header and the rows; the truck takes the rest.
                        readonly property int boxesWidth: 64
                        readonly property int kgWidth: 84
                        readonly property int fillWidth: 72
                        readonly property int actionWidth: 224
                        readonly property int columnSpacing: Theme.spacingM

                        Label {
                            anchors.centerIn: parent
                            visible: truckList.count === 0
                            text: qsTr("Фур ще немає")
                            color: Theme.textMuted
                        }

                        header: Rectangle {
                            z: 2
                            width: ListView.view.width
                            height: 28
                            visible: truckList.count > 0
                            color: Theme.surface


                            RowLayout {
                                anchors.fill: parent
                                spacing: truckList.columnSpacing
                                ColumnHeader { text: qsTr("Фура"); horizontalAlignment: Text.AlignLeft; Layout.fillWidth: true }
                                ColumnHeader { text: qsTr("Ящиків"); Layout.preferredWidth: truckList.boxesWidth }
                                ColumnHeader { text: qsTr("Кг"); Layout.preferredWidth: truckList.kgWidth }
                                ColumnHeader { text: qsTr("Заповн."); Layout.preferredWidth: truckList.fillWidth }
                                Item { Layout.preferredWidth: truckList.actionWidth }
                            }
                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: 1
                                color: Theme.border
                            }
                        }

                        delegate: Rectangle {
                            id: truckRow
                            required property var modelData
                            width: ListView.view.width
                            height: 60
                            color: rowHover.hovered ? Theme.surfaceAlt : "transparent"

                            HoverHandler { id: rowHover }

                            // A load over its limit; the limit itself is the muted line below.
                            component LoadCell: ColumnLayout {
                                id: loadCell
                                required property string load
                                required property string limit
                                spacing: 0
                                Label {
                                    text: loadCell.load
                                    font.weight: Font.DemiBold
                                    color: truckRow.modelData.overloaded ? Theme.danger : Theme.text
                                    Layout.alignment: Qt.AlignRight
                                }
                                Label {
                                    text: loadCell.limit !== "" ? qsTr("з %1").arg(loadCell.limit) : qsTr("без ліміту")
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: Theme.textMuted
                                    Layout.alignment: Qt.AlignRight
                                }
                            }

                            RowLayout {
                                anchors.fill: parent
                                spacing: truckList.columnSpacing

                                ColumnLayout {
                                    spacing: 0
                                    Layout.fillWidth: true
                                    Label {
                                        text: [truckRow.modelData.plate, truckRow.modelData.make]
                                              .filter(part => part !== "").join(" · ")
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Label {
                                        visible: text !== ""
                                        text: truckRow.modelData.note
                                        font.pixelSize: Theme.fontSizeCaption
                                        color: Theme.textMuted
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }
                                LoadCell {
                                    load: truckRow.modelData.loadBoxes
                                    limit: truckRow.modelData.limitBoxes
                                    Layout.preferredWidth: truckList.boxesWidth
                                    Layout.minimumWidth: truckList.boxesWidth
                                    Layout.maximumWidth: truckList.boxesWidth
                                }
                                LoadCell {
                                    load: truckRow.modelData.loadKg
                                    limit: truckRow.modelData.limitKg
                                    Layout.preferredWidth: truckList.kgWidth
                                    Layout.minimumWidth: truckList.kgWidth
                                    Layout.maximumWidth: truckList.kgWidth
                                }
                                // Full is fine, over the limit is said in red: the facts are
                                // accepted and the logistician sorts the overload out.
                                ColumnLayout {
                                    spacing: Theme.spacingXs
                                    Layout.preferredWidth: truckList.fillWidth
                                    Layout.minimumWidth: truckList.fillWidth
                                    Layout.maximumWidth: truckList.fillWidth
                                    Label {
                                        text: truckRow.modelData.overloaded ? qsTr("понад") : truckRow.modelData.fillPercent + "%"
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.weight: truckRow.modelData.overloaded ? Font.DemiBold : Font.Normal
                                        color: truckRow.modelData.overloaded ? Theme.danger : Theme.textMuted
                                        Layout.alignment: Qt.AlignRight
                                    }
                                    ProgressBar {
                                        from: 0
                                        to: 100
                                        value: Math.min(truckRow.modelData.fillPercent, 100)
                                        Layout.fillWidth: true
                                    }
                                }
                                // A loading truck waits for the decision; a decided one shows it.
                                RowLayout {
                                    spacing: Theme.spacingXs
                                    Layout.preferredWidth: truckList.actionWidth
                                    Layout.minimumWidth: truckList.actionWidth
                                    Layout.maximumWidth: truckList.actionWidth

                                    Button {
                                        visible: truckRow.modelData.loading
                                        text: qsTr("Відправити")
                                        Layout.fillWidth: true
                                        onClicked: Terminal.departTruck(truckRow.modelData.truckId)
                                    }
                                    Button {
                                        visible: truckRow.modelData.loading
                                        text: qsTr("Перенести")
                                        Layout.fillWidth: true
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("У наступну сесію з тим самим завантаженням")
                                        onClicked: Terminal.carryOverTruck(truckRow.modelData.truckId)
                                    }
                                    Label {
                                        visible: !truckRow.modelData.loading
                                        text: truckRow.modelData.status
                                        font.pixelSize: Theme.fontSizeCaption
                                        color: Theme.textMuted
                                        elide: Text.ElideRight
                                        horizontalAlignment: Text.AlignRight
                                        Layout.fillWidth: true
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
            }
        }
    }

    // A new truck (at least one limit, an empty field means "no limit"), or the
    // same plate and limits as a truck of the previous session on a new trip.
    Dialog {
        id: truckDialog
        anchors.centerIn: parent
        width: 420
        modal: true
        title: qsTr("Нова фура")
        standardButtons: Dialog.Cancel

        function add() {
            truckError.text = Terminal.addTruck(plate.text, make.text, note.text,
                                                boxLimit.text, weightLimit.text)
            if (truckError.text === "")
                close()
        }

        onOpened: {
            plate.clear()
            make.clear()
            note.clear()
            boxLimit.clear()
            weightLimit.clear()
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

                FieldCaption { caption: qsTr("Ліміт, ящиків") }
                FieldCaption { caption: qsTr("Ліміт, кг") }
                TextField {
                    id: boxLimit
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 0 }
                    Layout.fillWidth: true
                    onAccepted: truckDialog.add()
                }
                TextField {
                    id: weightLimit
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 0 }
                    Layout.fillWidth: true
                    onAccepted: truckDialog.add()
                }

                FieldCaption { caption: qsTr("Водій / куди їде"); Layout.columnSpan: 2 }
                TextField {
                    id: note
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    onAccepted: truckDialog.add()
                }
            }
            Label {
                text: qsTr("Потрібен хоча б один ліміт; порожнє поле — без ліміту.")
                font.pixelSize: Theme.fontSizeCaption
                color: Theme.textMuted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
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
