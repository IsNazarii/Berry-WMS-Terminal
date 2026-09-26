pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BerryWms.Ui

// Finds a supplier by number or part of the name. Built for the keyboard:
// type, Enter picks the only match.
Dialog {
    id: picker
    modal: true
    title: qsTr("Постачальник")
    width: 560
    height: 440
    standardButtons: Dialog.Cancel

    signal picked(var supplierId)

    readonly property SupplierListModel suppliers: Terminal.suppliers

    function pick(supplierId) {
        picker.picked(supplierId)
        picker.close()
    }

    onOpened: {
        search.clear()
        search.forceActiveFocus()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        TextField {
            id: search
            Layout.fillWidth: true
            placeholderText: qsTr("Пошук: номер або частина прізвища")
            onTextChanged: picker.suppliers.query = text
            onAccepted: if (picker.suppliers.count === 1) picker.pick(picker.suppliers.idAt(0))
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: picker.suppliers
            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                id: row
                required property var supplierId
                required property string name
                required property string phone
                required property string balance

                width: ListView.view.width
                onClicked: picker.pick(supplierId)

                contentItem: RowLayout {
                    spacing: 16
                    Label { text: "#" + row.supplierId; Layout.preferredWidth: 48 }
                    Label { text: row.name; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                    Label { text: row.phone; Layout.preferredWidth: 130 }
                    Label { text: row.balance; Layout.preferredWidth: 110; horizontalAlignment: Text.AlignRight }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                text: qsTr("Нікого не знайдено")
            }
        }
    }
}
