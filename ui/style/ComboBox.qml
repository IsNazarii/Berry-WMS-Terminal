pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

T.ComboBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)
    leftPadding: Theme.spacingM
    rightPadding: Theme.spacingM + (indicator ? indicator.width + spacing : 0)
    spacing: Theme.spacingS
    opacity: control.enabled ? 1 : Theme.disabledOpacity

    delegate: ItemDelegate {
        required property var model
        required property int index
        width: ListView.view.width
        text: model[control.textRole]
        font.weight: control.currentIndex === index ? Font.DemiBold : Font.Normal
        highlighted: control.highlightedIndex === index
        hoverEnabled: control.hoverEnabled
    }

    indicator: Text {
        x: control.width - width - Theme.spacingM
        y: (control.height - height) / 2
        text: "▾"
        color: Theme.textMuted
        font.pixelSize: Theme.fontSizeField
    }

    contentItem: Text {
        text: control.displayText
        font: control.font
        color: Theme.text
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 140
        implicitHeight: Theme.controlHeight
        radius: Theme.radiusS
        color: control.down || control.hovered ? Theme.surfaceAlt : Theme.surface
        border.width: control.visualFocus || control.popup.visible ? 2 : 1
        border.color: control.visualFocus || control.popup.visible ? Theme.accent : Theme.border
    }

    popup: T.Popup {
        y: control.height + Theme.spacingXs
        width: control.width
        // Up to eight rows, then the list scrolls.
        height: Math.min(contentItem.implicitHeight, Theme.controlHeight * 8) + topPadding + bottomPadding
        topMargin: Theme.spacingS
        bottomMargin: Theme.spacingS
        padding: Theme.spacingXs

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0
            T.ScrollBar.vertical: ScrollBar {}
        }

        background: Rectangle {
            color: Theme.surface
            radius: Theme.radiusM
            border.color: Theme.border
        }
    }
}
