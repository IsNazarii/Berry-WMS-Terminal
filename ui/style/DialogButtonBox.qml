import QtQuick
import QtQuick.Templates as T

T.DialogButtonBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    contentWidth: (contentItem as ListView)?.contentWidth
    spacing: Theme.spacingS
    padding: Theme.spacingXl
    topPadding: Theme.spacingM
    alignment: Qt.AlignRight

    // Buttons the dialog makes itself (standardButtons): the accepting one is the primary action.
    delegate: Button {
        highlighted: DialogButtonBox.buttonRole === DialogButtonBox.AcceptRole
                     || DialogButtonBox.buttonRole === DialogButtonBox.YesRole
    }

    contentItem: ListView {
        implicitWidth: contentWidth
        model: control.contentModel
        spacing: control.spacing
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        snapMode: ListView.SnapToItem
    }

    background: Item {}
}
