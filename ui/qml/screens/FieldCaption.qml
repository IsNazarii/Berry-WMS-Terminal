import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A field caption with its hint on the same line, so every field of a row
// starts at the same height whatever the hint's length.
RowLayout {
    id: fieldCaption
    required property string caption
    property string hint: ""
    spacing: Theme.spacingS
    Layout.fillWidth: true

    Label {
        text: fieldCaption.caption
        font.pixelSize: Theme.fontSizeCaption
        font.weight: Font.Medium
        color: Theme.textMuted
    }
    Label {
        text: fieldCaption.hint
        font.pixelSize: Theme.fontSizeCaption
        color: Theme.textMuted
        opacity: 0.8
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignRight
        Layout.fillWidth: true
    }
}
