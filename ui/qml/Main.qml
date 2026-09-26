import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BerryWms.Ui
import "screens"

ApplicationWindow {
    id: window
    width: 1024
    height: 700
    visible: true
    title: qsTr("Berry WMS — термінал приймальника")

    // Operator workflow goes left to right: who brought it, weigh, load, pay.
    header: TabBar {
        id: tabs
        TabButton { text: qsTr("Постачальник") }
        TabButton { text: qsTr("Зважування") }
        TabButton { text: qsTr("Розподіл") }
        TabButton { text: qsTr("Каса") }
    }

    // Always visible, so the operator never loses track of whose berries are on the scale.
    footer: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12

            Label {
                text: Terminal.hasSupplier
                      ? qsTr("Постачальник: %1 · борг: %2").arg(Terminal.supplierName).arg(Terminal.supplierBalance)
                      : qsTr("Постачальника не обрано")
                Layout.fillWidth: true
            }
            Label { text: qsTr("Записів у журналі: %1").arg(Terminal.journalSize) }
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: tabs.currentIndex

        IdentificationScreen {}
        WeighingScreen {}
        AllocationScreen {}
        CashScreen {}
    }
}
