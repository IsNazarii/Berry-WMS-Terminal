import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Shown instead of a screen that needs an open session: says why and leads
// to the session screen, where the day is opened deliberately.
Frame {
    id: notice
    required property string text
    signal sessionRequested()

    anchors.centerIn: parent
    padding: Theme.spacingXl

    ColumnLayout {
        spacing: Theme.spacingM

        Label {
            text: qsTr("Сесію не відкрито")
            font.pixelSize: Theme.fontSizeHeading
            font.weight: Font.Bold
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            text: notice.text
            color: Theme.textMuted
            Layout.alignment: Qt.AlignHCenter
        }
        Button {
            text: qsTr("До сесії")
            highlighted: true
            font.pixelSize: Theme.fontSizeField
            Layout.preferredHeight: Theme.controlHeightLarge
            Layout.alignment: Qt.AlignHCenter
            onClicked: notice.sessionRequested()
        }
    }
}
