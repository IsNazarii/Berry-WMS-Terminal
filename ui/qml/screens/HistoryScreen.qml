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
                    onActivated: Terminal.selectHistorySession(currentValue)
                }
                CaptionLabel {
                    visible: dayPicker.currentIndex >= 0
                    text: visible ? Terminal.historySessions[dayPicker.currentIndex].detail : ""
                }

                SectionCaption { text: qsTr("Куди вантажили") }
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: Theme.spacingXs
                    model: Terminal.historyPlaces

                    // How many records, not boxes: what the table on the right will list.
                    delegate: ListRow {
                        required property var modelData
                        title: modelData.title
                        detail: modelData.detail
                        trailing: modelData.rows
                        highlighted: modelData.placeId === Terminal.historyPlace
                        onClicked: Terminal.selectHistoryPlace(modelData.placeId)
                    }
                }
            }
        }

        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacingM

                PanelHeader {
                    visible: screen.hasDay
                    title: screen.place ? screen.place.title : ""
                    subtitle: Terminal.historySummary
                }

                DataTable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: Terminal.historyRows
                    columns: [
                        { title: qsTr("Час"), width: 92 },
                        // A plate on every truck, or where a warehouse row went; one truck needs none.
                        { title: screen.warehouse ? qsTr("Куди") : qsTr("Фура"), width: screen.narrow ? 90 : 110,
                          visible: screen.warehouse || Terminal.historyPlace === "all" },
                        { title: qsTr("Від кого") },
                        { title: qsTr("Культура"), width: screen.narrow ? 84 : 110 },
                        { title: qsTr("Пал."), width: 44, align: "right" },
                        { title: qsTr("Ящ."), width: 52, align: "right" },
                        { title: qsTr("Нетто, кг"), width: screen.narrow ? 72 : 80, align: "right" },
                        { title: qsTr("Хто"), width: screen.narrow ? 80 : 100 }
                    ]
                    // A pallet taken back stays as a trace of the mistake, crossed out; its number
                    // and who loaded it stay readable, they identify the mistake.
                    cellsOf: row => [
                        { text: row.time, struck: row.reversed },
                        { text: screen.warehouse ? (row.out ? "→ " + row.place : qsTr("на склад")) : row.place,
                          struck: row.reversed },
                        { text: row.from, bold: true, muted: row.out, struck: row.reversed, note: row.reversedNote },
                        { text: row.crop, struck: row.reversed },
                        { text: row.pallet, muted: row.reversed },
                        { text: row.boxes, struck: row.reversed },
                        { text: row.kg, struck: row.reversed },
                        { text: row.by, muted: true }
                    ]

                    EmptyState {
                        anchors.centerIn: parent
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
