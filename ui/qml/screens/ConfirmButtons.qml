import QtQuick
import QtQuick.Controls

// The footer of a dialog that asks before acting: the action, highlighted, and "Скасувати".
// The dialog reacts in onAccepted.
DialogButtonBox {
    id: buttons

    property string acceptText
    property bool acceptEnabled: true

    Button {
        text: buttons.acceptText
        enabled: buttons.acceptEnabled
        highlighted: true
        DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
    }
    Button {
        text: qsTr("Скасувати")
        DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
    }
}
