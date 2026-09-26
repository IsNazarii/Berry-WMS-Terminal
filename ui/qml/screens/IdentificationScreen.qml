pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BerryWms.Ui

Page {
    id: screen
    title: qsTr("Ідентифікація постачальника")

    // The supplier is chosen, go on to weighing.
    signal proceed()

    readonly property SupplierListModel suppliers: Terminal.suppliers

    // StackLayout is not a FocusScope, so `focus: true` on the field is not enough:
    // put the cursor into the search each time the operator comes back to this tab.
    onVisibleChanged: if (visible) search.forceActiveFocus()
    Component.onCompleted: search.forceActiveFocus()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: search
                Layout.fillWidth: true
                placeholderText: qsTr("Пошук: номер або частина прізвища")
                onTextChanged: screen.suppliers.query = text
                // Enter picks the only match: a known number is typed and confirmed without the mouse.
                onAccepted: {
                    if (screen.suppliers.count === 1 && Terminal.selectSupplier(screen.suppliers.idAt(0)))
                        screen.proceed()
                }
            }
            Button {
                text: qsTr("Очистити")
                enabled: search.text.length > 0
                onClicked: search.clear()
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: screen.suppliers
            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                id: row
                required property var supplierId
                required property string name
                required property string phone
                required property string balance

                width: ListView.view.width
                highlighted: Terminal.hasSupplier && Terminal.supplierId === supplierId
                onClicked: Terminal.selectSupplier(supplierId)
                onDoubleClicked: {
                    Terminal.selectSupplier(supplierId)
                    screen.proceed()
                }

                contentItem: RowLayout {
                    spacing: 16
                    Label { text: "#" + row.supplierId; Layout.preferredWidth: 48 }
                    Label { text: row.name; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                    Label { text: row.phone; Layout.preferredWidth: 140 }
                    Label { text: row.balance; Layout.preferredWidth: 120; horizontalAlignment: Text.AlignRight }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                text: qsTr("Нікого не знайдено")
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Label {
                Layout.fillWidth: true
                text: Terminal.hasSupplier ? qsTr("Обрано: %1").arg(Terminal.supplierName)
                                           : qsTr("Оберіть постачальника зі списку")
            }
            Button {
                text: qsTr("Скасувати вибір")
                enabled: Terminal.hasSupplier
                onClicked: Terminal.clearSupplier()
            }
            Button {
                text: qsTr("До зважування →")
                highlighted: true
                enabled: Terminal.hasSupplier
                onClicked: screen.proceed()
            }
        }
    }
}
