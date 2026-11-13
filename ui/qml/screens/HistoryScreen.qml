pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BerryWms.Ui

// What a work day loaded and where from: every pallet with its supplier and
// who put it there, by truck or for all trucks, and the warehouse in and out.
// Read-only; the open session by default, any closed one from the archive.
Page {
    id: screen
    title: qsTr("Історія")

    readonly property bool hasDay: Terminal.historySessionId !== ""
    readonly property bool allTrucks: Terminal.historyPlace === "all"
    readonly property bool warehouse: Terminal.historyPlace === "warehouse"
    readonly property var place: Terminal.historyPlaces.find(p => p.placeId === Terminal.historyPlace)
    // Down to the minimum window (1024) the supplier column still needs room.
    readonly property bool narrow: width < 1200

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingM
        spacing: Theme.spacingM

        // The day, then the place within it.
        Frame {
            Layout.preferredWidth: screen.narrow ? 210 : 260
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacingM

                SectionCaption { text: qsTr("День") }
                ComboBox {
                    id: dayPicker
                    Layout.fillWidth: true
                    enabled: screen.hasDay
                    model: Terminal.historySessions
                    textRole: "title"
                    valueRole: "sessionId"
                    // Follows the terminal: the default day changes when a session opens or closes.
                    currentIndex: Terminal.historySessions.findIndex(s => s.sessionId === Terminal.historySessionId)
                    onActivated: index => Terminal.selectHistorySession(currentValue)
                }
                Label {
                    visible: dayPicker.currentIndex >= 0
                    text: dayPicker.currentIndex >= 0 ? Terminal.historySessions[dayPicker.currentIndex].detail : ""
                    font.pixelSize: Theme.fontSizeCaption
                    color: Theme.textMuted
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                SectionCaption { text: qsTr("Куди вантажили") }
                ListView {
                    id: placeList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: Theme.spacingXs
                    model: Terminal.historyPlaces

                    delegate: ItemDelegate {
                        id: placeItem
                        required property var modelData
                        width: ListView.view.width
                        highlighted: modelData.placeId === Terminal.historyPlace
                        onClicked: Terminal.selectHistoryPlace(modelData.placeId)

                        contentItem: RowLayout {
                            spacing: Theme.spacingS
                            ColumnLayout {
                                spacing: Theme.spacingXs
                                Layout.fillWidth: true
                                Label {
                                    text: placeItem.modelData.title
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: placeItem.modelData.detail
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: Theme.textMuted
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                            // How many records, not boxes: what the table on the right will list.
                            Label {
                                text: placeItem.modelData.rows
                                font.pixelSize: Theme.fontSizeCaption
                                color: placeItem.modelData.rows > 0 ? Theme.text : Theme.textMuted
                            }
                        }
                    }
                }
            }
        }

        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                id: table
                anchors.fill: parent
                spacing: Theme.spacingM

                // Column widths shared by the header and the rows; the supplier takes the rest.
                readonly property int timeWidth: 92
                readonly property int placeWidth: screen.narrow ? 90 : 110
                readonly property int cropWidth: screen.narrow ? 84 : 110
                readonly property int palletWidth: 44
                readonly property int boxesWidth: 52
                readonly property int kgWidth: screen.narrow ? 72 : 80
                readonly property int byWidth: screen.narrow ? 80 : 100
                // A plate on every truck, or where a warehouse row went; one truck needs none.
                readonly property bool showPlace: screen.allTrucks || screen.warehouse

                ColumnLayout {
                    visible: screen.hasDay
                    spacing: 2
                    Layout.fillWidth: true

                    Label {
                        text: screen.place ? screen.place.title : ""
                        font.pixelSize: Theme.fontSizeTitle
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: Terminal.historySummary
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.textMuted
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                ColumnLayout {
                    visible: historyList.count > 0
                    spacing: 0
                    Layout.fillWidth: true

                    RowLayout {
                        spacing: Theme.spacingM
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28

                        ColumnHeader {
                            text: qsTr("Час")
                            horizontalAlignment: Text.AlignLeft
                            Layout.preferredWidth: table.timeWidth
                        }
                        ColumnHeader {
                            visible: table.showPlace
                            text: screen.warehouse ? qsTr("Куди") : qsTr("Фура")
                            horizontalAlignment: Text.AlignLeft
                            Layout.preferredWidth: table.placeWidth
                        }
                        ColumnHeader {
                            text: qsTr("Від кого")
                            horizontalAlignment: Text.AlignLeft
                            Layout.fillWidth: true
                        }
                        ColumnHeader {
                            text: qsTr("Культура")
                            horizontalAlignment: Text.AlignLeft
                            Layout.preferredWidth: table.cropWidth
                        }
                        ColumnHeader { text: qsTr("Пал."); Layout.preferredWidth: table.palletWidth }
                        ColumnHeader { text: qsTr("Ящ."); Layout.preferredWidth: table.boxesWidth }
                        ColumnHeader { text: qsTr("Нетто, кг"); Layout.preferredWidth: table.kgWidth }
                        ColumnHeader {
                            text: qsTr("Хто")
                            horizontalAlignment: Text.AlignLeft
                            Layout.preferredWidth: table.byWidth
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 1
                        color: Theme.border
                    }
                }

                ListView {
                    id: historyList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: Terminal.historyRows
                    ScrollBar.vertical: ScrollBar {}

                    delegate: Rectangle {
                        id: row
                        required property var modelData
                        // A pallet taken back stays as a trace of the mistake, but crossed out.
                        readonly property color textColor: modelData.reversed ? Theme.textMuted : Theme.text
                        width: ListView.view.width
                        implicitHeight: 48
                        color: rowHover.hovered ? Theme.surfaceAlt : "transparent"

                        HoverHandler { id: rowHover }

                        RowLayout {
                            anchors.fill: parent
                            spacing: Theme.spacingM

                            Label {
                                text: row.modelData.time
                                color: row.textColor
                                font.strikeout: row.modelData.reversed
                                elide: Text.ElideRight
                                Layout.preferredWidth: table.timeWidth
                            }
                            Label {
                                visible: table.showPlace
                                // On the warehouse view a row either came in or left for a truck.
                                text: screen.warehouse ? (row.modelData.out ? "→ " + row.modelData.place : qsTr("на склад"))
                                                       : row.modelData.place
                                color: row.textColor
                                font.strikeout: row.modelData.reversed
                                elide: Text.ElideRight
                                Layout.preferredWidth: table.placeWidth
                            }
                            ColumnLayout {
                                spacing: 0
                                Layout.fillWidth: true
                                Label {
                                    text: row.modelData.from
                                    font.weight: Font.DemiBold
                                    color: row.modelData.out ? Theme.textMuted : row.textColor
                                    font.strikeout: row.modelData.reversed
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    visible: row.modelData.reversed
                                    text: row.modelData.reversedNote
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: Theme.warn
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                            Label {
                                text: row.modelData.crop
                                color: row.textColor
                                font.strikeout: row.modelData.reversed
                                elide: Text.ElideRight
                                Layout.preferredWidth: table.cropWidth
                            }
                            Label {
                                text: row.modelData.pallet
                                color: row.textColor
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: table.palletWidth
                            }
                            Label {
                                text: row.modelData.boxes
                                color: row.textColor
                                font.strikeout: row.modelData.reversed
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: table.boxesWidth
                            }
                            Label {
                                text: row.modelData.kg
                                color: row.textColor
                                font.strikeout: row.modelData.reversed
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: table.kgWidth
                            }
                            Label {
                                text: row.modelData.by
                                color: Theme.textMuted
                                elide: Text.ElideRight
                                Layout.preferredWidth: table.byWidth
                            }
                        }
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: Theme.surfaceAlt
                        }
                    }

                    EmptyState {
                        anchors.centerIn: parent
                        visible: historyList.count === 0
                        icon: "history"
                        title: !screen.hasDay ? qsTr("Ще не було жодної сесії")
                               : screen.warehouse ? qsTr("Склад цього дня не змінювався")
                                                  : qsTr("Ще нічого не завантажено")
                        text: !screen.hasDay ? qsTr("Історія з'явиться, щойно відкриють першу сесію й запишуть палету.")
                              : qsTr("Тут з'являється кожна палета, щойно вона потрапляє у фуру чи на склад — "
                                     + "з «Прийому» чи з «Розподілу».")
                    }
                }
            }
        }
    }
}
