import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BerryWms.Ui

Page {
    id: screen
    title: qsTr("Зважування")

    readonly property ScaleMonitor scales: Terminal.scales

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        Label {
            text: Terminal.hasSupplier ? Terminal.supplierName : qsTr("Постачальника не обрано")
            font.pixelSize: 20
            font.bold: true
        }

        // The readout is the first thing the operator looks at, so it is big and colour-coded.
        Frame {
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: screen.scales.display
                    font.pixelSize: 64
                    font.family: "monospace"
                }
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: screen.scales.stable ? qsTr("● СТАБІЛЬНО") : qsTr("○ вага змінюється…")
                    color: screen.scales.stable ? "#2e7d32" : "#ef6c00"
                    font.pixelSize: 18
                    font.bold: true
                }
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: screen.scales.driverName
                    opacity: 0.6
                }
            }
        }

        RowLayout {
            Button {
                text: qsTr("Симулятор: поставити палету")
                onClicked: Terminal.simulatePallet()
            }
            Button {
                text: qsTr("Симулятор: зняти")
                onClicked: Terminal.simulateEmptyScale()
            }
        }

        Item { Layout.fillHeight: true }
    }
}
