import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T

T.TextField {
    id: control

    implicitWidth: implicitBackgroundWidth + leftInset + rightInset
                   || Math.max(contentWidth, placeholder.implicitWidth) + leftPadding + rightPadding
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding,
                             placeholder.implicitHeight + topPadding + bottomPadding)
    padding: Theme.spacingS
    leftPadding: Theme.spacingM
    rightPadding: Theme.spacingM
    // Fields hold numbers the operator reads back: a notch larger than body text.
    font.pixelSize: Theme.fontSizeField
    font.weight: Font.Medium
    color: Theme.text
    selectionColor: Theme.accent
    selectedTextColor: Theme.onAccent
    placeholderTextColor: Theme.textMuted
    verticalAlignment: TextInput.AlignVCenter
    opacity: control.enabled ? 1 : Theme.disabledOpacity

    PlaceholderText {
        id: placeholder
        x: control.leftPadding
        y: control.topPadding
        width: control.width - (control.leftPadding + control.rightPadding)
        height: control.height - (control.topPadding + control.bottomPadding)
        text: control.placeholderText
        font.family: control.font.family
        font.pixelSize: Theme.fontSizeBody
        color: control.placeholderTextColor
        verticalAlignment: control.verticalAlignment
        visible: !control.length && !control.preeditText
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 160
        implicitHeight: Theme.controlHeight
        radius: Theme.radiusS
        color: Theme.surface
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }
}
