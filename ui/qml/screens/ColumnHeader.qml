import QtQuick
import QtQuick.Controls

// A table column header: small capitals, numbers right-aligned under it.
Label {
    font.pixelSize: 11
    font.weight: Font.DemiBold
    font.capitalization: Font.AllUppercase
    color: Theme.textMuted
    horizontalAlignment: Text.AlignRight
    elide: Text.ElideRight
}
