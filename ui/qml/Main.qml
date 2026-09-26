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
        visible: Terminal.loggedIn
        TabButton { text: qsTr("Постачальник") }
        TabButton { text: qsTr("Зважування") }
        TabButton { text: qsTr("Розподіл") }
        TabButton { text: qsTr("Каса") }
    }

    // Always visible, so the operator never loses track of whose berries are on the scale.
    footer: ToolBar {
        visible: Terminal.loggedIn

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
            Label { text: qsTr("Оператор: %1").arg(Terminal.operatorName) }
            ToolButton {
                text: qsTr("Вийти")
                onClicked: Terminal.logout()
            }
        }
    }

    // Nothing is recorded without knowing who did it, so the screens wait for a login.
    Connections {
        target: Terminal
        function onOperatorChanged() { tabs.currentIndex = 0 }
    }

    LoginScreen {
        anchors.fill: parent
        visible: !Terminal.loggedIn
    }

    StackLayout {
        anchors.fill: parent
        visible: Terminal.loggedIn
        currentIndex: tabs.currentIndex

        IdentificationScreen {
            onProceed: tabs.currentIndex = 1
        }
        WeighingScreen {}
        AllocationScreen {}
        CashScreen {}
    }
}
