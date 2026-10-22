pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BerryWms.Ui

// Finds a supplier by number or part of the name. Built for the keyboard:
// type, Enter picks the only match. A search without matches offers to add
// the supplier right here, so a newcomer never waits at the scale.
Dialog {
    id: picker
    modal: true
    title: qsTr("Постачальник")
    width: 560
    height: 440
    standardButtons: Dialog.Cancel

    signal picked(var supplierId)

    readonly property SupplierListModel suppliers: Terminal.suppliers

    // Adding a new supplier instead of searching.
    property bool adding: false
    // Re-read on every keystroke: the reasons show while typing.
    readonly property var addErrors: Terminal.newSupplierErrors(newName.text, newPhone.text)

    function pick(supplierId) {
        picker.picked(supplierId)
        picker.close()
    }

    function startAdding() {
        // A number in the search is an id, not a name worth keeping.
        newName.text = /^\d+$/.test(search.text.trim()) ? "" : search.text
        newPhone.clear()
        picker.adding = true
        newName.forceActiveFocus()
    }

    function add() {
        const id = Terminal.addSupplier(newName.text, newPhone.text)
        if (id !== 0) picker.pick(id)
    }

    onOpened: {
        picker.adding = false
        search.clear()
        search.forceActiveFocus()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        visible: !picker.adding

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

            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 2 * Theme.spacingXl
                visible: list.count === 0
                spacing: Theme.spacingM

                Label {
                    text: qsTr("Нікого не знайдено")
                    Layout.alignment: Qt.AlignHCenter
                }
                Button {
                    visible: search.text.trim() !== ""
                    text: /^\d+$/.test(search.text.trim()) ? qsTr("Додати нового…")
                                                            : qsTr("Додати «%1»…").arg(search.text.trim())
                    highlighted: true
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: parent.width
                    onClicked: picker.startAdding()
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacingS
        visible: picker.adding

        FieldCaption { caption: qsTr("Прізвище та ім'я") }
        TextField {
            id: newName
            Layout.fillWidth: true
            onAccepted: newPhone.forceActiveFocus()
        }

        FieldCaption { caption: qsTr("Телефон"); hint: qsTr("необов'язково") }
        TextField {
            id: newPhone
            Layout.fillWidth: true
            placeholderText: "+38 067 111 22 33"
            inputMethodHints: Qt.ImhDialableCharactersOnly
            onAccepted: picker.add()
        }

        Repeater {
            model: picker.addErrors
            delegate: Label {
                required property string modelData
                text: modelData
                color: Theme.warn
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            spacing: Theme.spacingS
            Layout.fillWidth: true
            Button {
                text: qsTr("← До пошуку")
                onClicked: {
                    picker.adding = false
                    search.forceActiveFocus()
                }
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Додати й обрати · Enter")
                highlighted: true
                enabled: picker.addErrors.length === 0
                onClicked: picker.add()
            }
        }
    }
}
