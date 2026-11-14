pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BerryWms.Ui

// Where the berries go: receipts with something left on the left, the chosen
// receipt in the middle, trucks and the warehouse on the right. The operator
// picks a destination once and then sends pallets or boxes into it.
Page {
    id: screen

    // No session: the main window switches to the session screen.
    signal sessionRequested()
    title: qsTr("Розподіл по фурах")

    // A truck id or "warehouse"; kept in QML, the core checks it on every allocation.
    property string destinationId: ""
    property string destinationTitle: ""
    readonly property bool hasReceipt: Terminal.allocationReceiptId !== ""
    readonly property bool hasDestination: destinationId !== ""
    readonly property var source: Terminal.allocationSources.find(r => r.receiptId === Terminal.allocationReceiptId)

    NoSessionNotice {
        visible: !Terminal.sessionOpen
        text: qsTr("Розподіл можливий лише у відкритій сесії.")
        onSessionRequested: screen.sessionRequested()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingM
        visible: Terminal.sessionOpen
        spacing: Theme.spacingM

        // Receipts that still have boxes to allocate: open ones first, pallets go out while unloading.
        Frame {
            Layout.preferredWidth: 220
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacingM

                SectionCaption { text: qsTr("Чеки до розподілу") }

                ListView {
                    id: sourceList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: Theme.spacingXs
                    // Empty: the middle panel says so and why a receipt would turn up here.
                    model: Terminal.allocationSources

                    delegate: ItemDelegate {
                        id: sourceRow
                        required property var modelData
                        width: ListView.view.width
                        highlighted: modelData.receiptId === Terminal.allocationReceiptId
                        onClicked: {
                            Terminal.selectAllocationReceipt(modelData.receiptId)
                            allocationError.text = ""
                        }

                        contentItem: ColumnLayout {
                            spacing: 2
                            Label {
                                text: sourceRow.modelData.supplierName
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            RowLayout {
                                CaptionLabel {
                                    text: sourceRow.modelData.openedAt + " · "
                                          + (sourceRow.modelData.open ? qsTr("зважується") : qsTr("закритий"))
                                    Layout.fillWidth: false
                                }
                                CaptionLabel {
                                    text: qsTr("%1 ящ.").arg(sourceRow.modelData.leftBoxes)
                                    horizontalAlignment: Text.AlignRight
                                }
                            }
                        }
                    }
                }
            }
        }

        // The chosen receipt: whole pallets or a number of boxes of a crop.
        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Nothing to work on yet: either all is placed or a receipt is still to be picked.
            EmptyState {
                anchors.centerIn: parent
                visible: !screen.hasReceipt
                icon: sourceList.count === 0 ? "circle-check" : "truck"
                title: sourceList.count === 0 ? qsTr("Усе розподілено") : qsTr("Оберіть чек зліва")
                text: sourceList.count === 0
                      ? qsTr("Чек з'явиться зліва, коли палеті забракне місця у фурі або на «Прийомі» не обрано ціль.")
                      : qsTr("Там чеки з палетами, що ще чекають на фуру або склад.")
            }

            ColumnLayout {
                anchors.fill: parent
                visible: screen.hasReceipt
                spacing: Theme.spacingM

                // Shown only with a receipt chosen; with nothing left it drops out of the sources.
                PanelHeader {
                    title: Terminal.allocationSupplierName
                    subtitle: screen.source ? qsTr("Відкрито %1 · %2 · лишилось %3 ящ., %4 кг")
                                              .arg(screen.source.openedAt)
                                              .arg(screen.source.open ? qsTr("зважується") : qsTr("закритий"))
                                              .arg(screen.source.leftBoxes).arg(screen.source.leftKg)
                                            : qsTr("Усе розподілено")
                }

                TargetBanner {
                    text: screen.hasDestination ? qsTr("Куди: %1").arg(screen.destinationTitle)
                                                : qsTr("Оберіть фуру або склад праворуч")
                    active: screen.hasDestination
                }

                SectionCaption {
                    visible: screen.hasReceipt
                    text: qsTr("Палетами")
                }

                ListView {
                    id: lineList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: Terminal.allocationLines
                    headerPositioning: ListView.OverlayHeader
                    ScrollBar.vertical: ScrollBar {}

                    // Column widths shared by the header and the rows; the crop takes the rest.
                    readonly property int indexWidth: 20
                    readonly property int boxesWidth: 40
                    readonly property int netWidth: 72
                    readonly property int placeWidth: 150

                    // Stays on top while the lines scroll under it, so it needs a background.
                    header: Rectangle {
                        z: 2
                        width: ListView.view.width
                        implicitHeight: lineHeader.implicitHeight
                        visible: lineList.count > 0
                        color: Theme.surface

                        TableHeader {
                            id: lineHeader
                            width: parent.width
                            columns: [
                                { title: qsTr("№"), width: lineList.indexWidth },
                                { title: qsTr("Культура") },
                                { title: qsTr("Ящ."), width: lineList.boxesWidth, align: "right" },
                                { title: qsTr("Нетто, кг"), width: lineList.netWidth, align: "right" },
                                { title: qsTr("Де"), width: lineList.placeWidth, align: "right" }
                            ]
                        }
                    }

                    delegate: TableRow {
                        id: line
                        required property var modelData
                        readonly property bool free: modelData.placedIn === "" && !modelData.cancelled
                        width: ListView.view.width
                        implicitHeight: 44

                        // A cancelled line stays visible, struck through, as on the receiving screen.
                        component Cell: Label {
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                            font.strikeout: line.modelData.cancelled
                            color: line.modelData.cancelled ? Theme.textMuted : Theme.text
                        }

                        RowLayout {
                            anchors.fill: parent
                            spacing: Theme.spacingM

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
                            Cell {
                                text: line.modelData.net
                                Layout.preferredWidth: lineList.netWidth
                            }
                            // A free pallet is one press away; a placed one says where it went.
                            Item {
                                Layout.preferredWidth: lineList.placeWidth
                                Layout.minimumWidth: lineList.placeWidth
                                Layout.maximumWidth: lineList.placeWidth
                                Layout.fillHeight: true

                                Button {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width
                                    visible: line.free
                                    enabled: screen.hasDestination
                                    text: qsTr("Палету →")
                                    onClicked: allocationError.text =
                                               Terminal.allocateWeighing(line.modelData.index, screen.destinationId)
                                }
                                Label {
                                    anchors.fill: parent
                                    visible: !line.free
                                    text: line.modelData.cancelled ? qsTr("анульовано") : line.modelData.placedIn
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: Theme.textMuted
                                    elide: Text.ElideRight
                                    horizontalAlignment: Text.AlignRight
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: Theme.border
                    visible: cropRepeater.count > 0
                }
                SectionCaption {
                    visible: cropRepeater.count > 0
                    text: qsTr("Ящиками — вага пропорційно")
                }
                // Rebuilt after every allocation, which also clears the typed count: that is wanted here.
                Repeater {
                    id: cropRepeater
                    model: Terminal.allocationCrops

                    RowLayout {
                        id: cropRow
                        required property var modelData
                        spacing: Theme.spacingM
                        Layout.fillWidth: true

                        function send() {
                            allocationError.text = Terminal.allocateBoxes(modelData.cropCode, boxes.text,
                                                                          screen.destinationId)
                        }

                        Label {
                            text: cropRow.modelData.name
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: qsTr("лишилось %1 ящ. · %2 кг").arg(cropRow.modelData.leftBoxes).arg(cropRow.modelData.leftKg)
                            font.pixelSize: Theme.fontSizeCaption
                            color: Theme.textMuted
                        }
                        TextField {
                            id: boxes
                            placeholderText: qsTr("ящиків")
                            Layout.preferredWidth: 90
                            inputMethodHints: Qt.ImhDigitsOnly
                            validator: IntValidator { bottom: 1; top: cropRow.modelData.leftBoxes }
                            onAccepted: cropRow.send()
                        }
                        Button {
                            text: qsTr("Відвантажити")
                            enabled: screen.hasDestination && boxes.text !== ""
                            onClicked: cropRow.send()
                        }
                    }
                }

                ErrorLabel { id: allocationError }
            }
        }

        // Destinations: trucks of the session still loading, then the warehouse.
        Frame {
            Layout.preferredWidth: 260
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacingM

                SectionCaption { text: qsTr("Куди") }

                ListView {
                    id: destinationList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: Theme.spacingXs
                    model: Terminal.destinations

                    delegate: DestinationCard {
                        required property var modelData
                        place: modelData
                        highlighted: modelData.destinationId === screen.destinationId
                        onClicked: {
                            screen.destinationId = modelData.destinationId
                            screen.destinationTitle = modelData.title
                            allocationError.text = ""
                        }
                    }
                }
            }
        }
    }
}
