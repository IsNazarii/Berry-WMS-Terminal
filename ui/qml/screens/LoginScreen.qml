import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BerryWms.Ui

// Who is working: picked from the list and confirmed by a PIN. Not a security
// barrier, it only makes every record say who made it.
Page {
    id: screen
    title: qsTr("Вхід оператора")

    onVisibleChanged: if (visible) screen.reset()
    Component.onCompleted: pin.forceActiveFocus()

    function reset() {
        pin.clear()
        error.visible = false
        pin.forceActiveFocus()
    }

    function tryLogin() {
        if (Terminal.login(operators.currentValue, pin.text))
            return
        error.visible = true
        pin.clear()
        pin.forceActiveFocus()
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: 320
        spacing: 12

        Label {
            text: screen.title
            font.pixelSize: Theme.fontSizeHeading
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }
        ComboBox {
            id: operators
            Layout.fillWidth: true
            model: Terminal.operators
            textRole: "name"
            valueRole: "operatorId"
            onActivated: screen.reset()
        }
        TextField {
            id: pin
            Layout.fillWidth: true
            placeholderText: qsTr("PIN (4–6 цифр)")
            echoMode: TextInput.Password
            inputMethodHints: Qt.ImhDigitsOnly
            validator: RegularExpressionValidator { regularExpression: /[0-9]{0,6}/ }
            onTextEdited: error.visible = false
            onAccepted: if (acceptableInput && text.length >= 4) screen.tryLogin()
        }
        Label {
            id: error
            visible: false
            text: qsTr("Невірний PIN")
            color: Theme.danger
        }
        Button {
            Layout.fillWidth: true
            text: qsTr("Увійти")
            highlighted: true
            enabled: pin.text.length >= 4
            onClicked: screen.tryLogin()
        }
    }
}
