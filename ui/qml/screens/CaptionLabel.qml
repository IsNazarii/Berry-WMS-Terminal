import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Small muted text under a title or beside a value; elided to fit its row.
Label {
    font.pixelSize: Theme.fontSizeCaption
    color: Theme.textMuted
    elide: Text.ElideRight
    Layout.fillWidth: true
}
