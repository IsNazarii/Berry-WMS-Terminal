import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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

    StackLayout {
        anchors.fill: parent
        currentIndex: tabs.currentIndex

        IdentificationScreen {}
        WeighingScreen {}
        AllocationScreen {}
        CashScreen {}
    }
}
