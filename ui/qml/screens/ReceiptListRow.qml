import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A receipt in a side list: the supplier, and under it when it was opened (left)
// and what it holds (right) — "22:46 · 2 палети" / "8 823,00 ₴".
ItemDelegate {
    id: row

    property string title
    property string detail
    property string trailing

    width: ListView.view ? ListView.view.width : implicitWidth

    contentItem: ColumnLayout {
        spacing: 2

        Label {
            text: row.title
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        RowLayout {
            CaptionLabel {
                text: row.detail
                Layout.fillWidth: false
            }
            CaptionLabel {
                text: row.trailing
                horizontalAlignment: Text.AlignRight
            }
        }
    }
}
