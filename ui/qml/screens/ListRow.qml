import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A row of a side list: bold title, muted detail, a small text on the right
// (a count, a sum). Selection and hover come from the style's ItemDelegate.
ItemDelegate {
    id: row

    property string title
    property string detail
    property string trailing
    // A count is small; a sum of money is read at body size.
    property real trailingSize: Theme.fontSizeCaption
    property color trailingColor: Theme.text

    width: ListView.view ? ListView.view.width : implicitWidth

    contentItem: RowLayout {
        spacing: Theme.spacingS

        ColumnLayout {
            spacing: Theme.spacingXs
            Layout.fillWidth: true

            Label {
                text: row.title
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            CaptionLabel {
                visible: text !== ""
                text: row.detail
            }
        }
        Label {
            visible: text !== ""
            text: row.trailing
            font.pixelSize: row.trailingSize
            color: row.trailingColor
        }
    }
}
