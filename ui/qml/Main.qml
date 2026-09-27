import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BerryWms.Ui
import "screens"

ApplicationWindow {
    id: window
    width: 1024
    height: 700
    minimumWidth: 1024
    minimumHeight: 600
    // A terminal on a laptop: the whole screen; the size above is what "restore" gives.
    visibility: Window.Maximized
    title: qsTr("Berry WMS — термінал приймальника")

    // Operator workflow goes left to right: open the day, receive, load, ship from stock, pay.
    header: TabBar {
        id: tabs
        visible: Terminal.loggedIn
        TabButton { text: qsTr("Сесія") }
        TabButton { text: qsTr("Прийом") }
        TabButton { text: qsTr("Розподіл") }
        TabButton { text: qsTr("Склад") }
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
                      : qsTr("Чек не обрано")
                Layout.fillWidth: true
            }
            Label {
                text: Terminal.sessionOpen ? Terminal.sessionTitle : qsTr("Сесію не відкрито")
                color: Terminal.sessionOpen ? palette.windowText : "#c62828"
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

        SessionScreen {}
        ReceivingScreen {}
        AllocationScreen {}
        WarehouseScreen {}
        CashScreen {}
    }
}
