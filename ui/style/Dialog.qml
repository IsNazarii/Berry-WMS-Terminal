import QtQuick
import QtQuick.Templates as T

T.Dialog {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding,
                            implicitHeaderWidth,
                            implicitFooterWidth)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding
                             + (implicitHeaderHeight > 0 ? implicitHeaderHeight + spacing : 0)
                             + (implicitFooterHeight > 0 ? implicitFooterHeight + spacing : 0))
    padding: Theme.spacingXl
    topPadding: Theme.spacingM

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusM
        border.color: Theme.border
    }

    header: Label {
        text: control.title
        visible: control.title !== ""
        elide: Label.ElideRight
        font.pixelSize: Theme.fontSizeTitle
        font.weight: Font.Bold
        padding: Theme.spacingXl
        bottomPadding: 0
    }

    footer: DialogButtonBox {
        visible: count > 0
    }

    T.Overlay.modal: Rectangle { color: Theme.overlay }
    T.Overlay.modeless: Rectangle { color: "transparent" }
}
