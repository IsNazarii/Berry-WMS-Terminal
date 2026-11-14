import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A place to load into, in a side list: a truck with its load and how full it is,
// or the warehouse with what it holds. `place` is an item of Terminal.destinations.
ItemDelegate {
    id: card

    property var place: ({})

    width: ListView.view ? ListView.view.width : implicitWidth

    contentItem: ColumnLayout {
        spacing: Theme.spacingXs

        Label {
            text: card.place.title ?? ""
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        // The warehouse has no limit, so it lists what it holds instead.
        CaptionLabel {
            visible: !!card.place.warehouse
            text: card.place.detail ?? ""
            wrapMode: Text.WordWrap
        }
        RowLayout {
            visible: !card.place.warehouse
            Layout.fillWidth: true

            CaptionLabel { text: qsTr("%1 / %2 ящ.").arg(card.place.loadBoxes).arg(card.place.limitBoxes) }
            CaptionLabel {
                text: card.place.loadKg + " " + qsTr("кг")
                Layout.fillWidth: false
            }
        }
        // Overload is only shown: facts from another terminal are never rejected.
        RowLayout {
            visible: !card.place.warehouse
            Layout.fillWidth: true

            ProgressBar {
                from: 0
                to: 100
                value: Math.min(card.place.fillPercent, 100)
                Layout.fillWidth: true
            }
            Label {
                text: card.place.overloaded ? qsTr("понад") : card.place.fillPercent + "%"
                font.pixelSize: Theme.fontSizeCaption
                font.weight: card.place.overloaded ? Font.DemiBold : Font.Normal
                color: card.place.overloaded ? Theme.danger : Theme.textMuted
            }
        }
    }
}
