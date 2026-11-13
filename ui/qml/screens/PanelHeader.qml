import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A panel's title with a muted line under it ("Усі фури" / "2 палети · 65 ящ.").
ColumnLayout {
    property alias title: titleLabel.text
    property alias subtitle: subtitleLabel.text

    spacing: 2
    Layout.fillWidth: true

    Label {
        id: titleLabel
        font.pixelSize: Theme.fontSizeTitle
        font.weight: Font.Bold
        elide: Text.ElideRight
        Layout.fillWidth: true
    }
    CaptionLabel {
        id: subtitleLabel
        visible: text !== ""
    }
}
