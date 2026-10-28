import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import BerryWms.Ui

// What a panel shows while it has nothing to show: what goes here and the next step,
// instead of a lone grey line in an empty box. Centre it in the panel it stands for.
ColumnLayout {
    id: emptyState

    // An icon name from ui/style/icons.
    property string icon
    property string title
    property string text
    // The next step; no button without it.
    property string actionText
    signal actionTriggered()

    spacing: Theme.spacingM
    width: Math.min(implicitWidth, Theme.formMaxWidth)

    Rectangle {
        visible: emptyState.icon !== ""
        implicitWidth: Theme.emptyBadgeSize
        implicitHeight: Theme.emptyBadgeSize
        radius: width / 2
        color: Theme.accentSoft
        Layout.alignment: Qt.AlignHCenter

        IconImage {
            anchors.centerIn: parent
            source: Theme.icon(emptyState.icon)
            sourceSize: Qt.size(Theme.iconSizeLarge, Theme.iconSizeLarge)
            color: Theme.accent
        }
    }
    Label {
        text: emptyState.title
        font.pixelSize: Theme.fontSizeLarge
        font.weight: Font.Bold
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    Label {
        visible: text !== ""
        text: emptyState.text
        color: Theme.textMuted
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        lineHeight: 1.3
        Layout.fillWidth: true
        Layout.maximumWidth: Theme.formMaxWidth - Theme.spacingXl * 4
        Layout.alignment: Qt.AlignHCenter
    }
    Button {
        visible: emptyState.actionText !== ""
        text: emptyState.actionText
        highlighted: true
        font.pixelSize: Theme.fontSizeField
        Layout.preferredHeight: Theme.controlHeightLarge
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Theme.spacingS
        onClicked: emptyState.actionTriggered()
    }
}
