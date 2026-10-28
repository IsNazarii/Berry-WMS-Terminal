pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Templates as T
import BerryWms.Ui

// Who is working: a card per operator, confirmed by a PIN. Not a security
// barrier, it only makes every record say who made it. Usually the same person
// comes back, so the last one is picked already and only the PIN is left to type.
Page {
    id: screen

    // The operatorId of the picked card.
    property var picked: undefined
    readonly property string pickedName: {
        const op = Terminal.operators.find(o => o.operatorId === screen.picked)
        return op ? op.name : ""
    }

    onVisibleChanged: if (visible) screen.reset()
    Component.onCompleted: {
        const known = Terminal.operators.some(o => o.operatorId === loginSettings.lastOperator)
        screen.picked = known ? loginSettings.lastOperator
                              : (Terminal.operators.length > 0 ? Terminal.operators[0].operatorId : undefined)
        pin.forceActiveFocus()
    }

    Settings {
        id: loginSettings
        category: "login"
        property var lastOperator
    }

    function reset() {
        pin.clear()
        error.visible = false
        pin.forceActiveFocus()
    }

    function tryLogin() {
        if (Terminal.login(screen.picked, pin.text)) {
            loginSettings.lastOperator = screen.picked
            return
        }
        error.visible = true
        pin.clear()
        pin.forceActiveFocus()
    }

    // A pickable operator: avatar, name, and who opened the session today.
    component OperatorCard: T.AbstractButton {
        id: card
        required property var modelData
        readonly property bool opener: Terminal.sessionOpen && Terminal.sessionOpenedBy === modelData.name

        checkable: true
        checked: screen.picked === modelData.operatorId
        implicitHeight: Theme.tileHeight
        padding: Theme.spacingL
        spacing: Theme.spacingM
        Accessible.name: modelData.name
        onClicked: {
            screen.picked = modelData.operatorId
            screen.reset()
        }

        contentItem: RowLayout {
            spacing: card.spacing

            Rectangle {
                implicitWidth: Theme.avatarSizeLarge
                implicitHeight: Theme.avatarSizeLarge
                radius: width / 2
                color: card.checked ? Theme.accent : Theme.surfaceAlt

                Label {
                    anchors.centerIn: parent
                    text: card.modelData.name.charAt(0)
                    font.pixelSize: Theme.fontSizeLarge
                    font.weight: Font.DemiBold
                    color: card.checked ? Theme.textOnAccent : Theme.accent
                }
            }
            ColumnLayout {
                spacing: 0
                Layout.fillWidth: true

                Label {
                    text: card.modelData.name
                    font.pixelSize: Theme.fontSizeTitle
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    visible: card.opener
                    text: qsTr("відкрив(ла) сесію")
                    font.pixelSize: Theme.fontSizeCaption
                    color: Theme.textMuted
                }
            }
        }
        background: Rectangle {
            radius: Theme.radiusM
            color: card.checked ? Theme.accentSoft : card.hovered ? Theme.surfaceAlt : Theme.surface
            border.width: card.checked || card.visualFocus ? 2 : 1
            border.color: card.checked || card.visualFocus ? Theme.accent : Theme.border
        }
    }

    background: Rectangle { color: Theme.bg }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Who we are and what state the point is in, before anyone logs in.
        Rectangle {
            color: Theme.brandPanel
            Layout.fillHeight: true
            Layout.preferredWidth: Math.max(screen.width / 3, Theme.loginPanelMinWidth)

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingXl * 2
                spacing: Theme.spacingL

                RowLayout {
                    spacing: Theme.spacingM

                    BrandMark {
                        size: Theme.brandMarkSizeLarge
                        color: Theme.brandMarkOnPanel
                    }
                    Label {
                        text: qsTr("Berry WMS")
                        font.pixelSize: Theme.fontSizeLarge
                        font.weight: Font.Bold
                        color: Theme.brandPanelText
                    }
                }
                Item { Layout.fillHeight: true }
                Label {
                    text: qsTr("Термінал\nприймання ягід")
                    font.pixelSize: Theme.fontSizeDisplay
                    font.weight: Font.Bold
                    color: Theme.brandPanelText
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Label {
                    text: qsTr("Прийом, зважування, розподіл по фурах і розрахунок з постачальниками")
                    font.pixelSize: Theme.fontSizeField
                    color: Theme.brandPanelMuted
                    wrapMode: Text.WordWrap
                    lineHeight: 1.3
                    Layout.fillWidth: true
                }
                Item { Layout.fillHeight: true }
                Flow {
                    spacing: Theme.spacingXl
                    Layout.fillWidth: true

                    Label {
                        text: Terminal.sessionOpen ? qsTr("%1 відкрита").arg(Terminal.sessionTitle)
                                                   : qsTr("Сесію не відкрито")
                        color: Theme.brandPanelMuted
                    }
                    Label {
                        text: Terminal.scales.driverName
                        color: Theme.brandPanelMuted
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - Theme.spacingXl * 2, Theme.formMaxWidth)
                spacing: Theme.spacingL

                Label {
                    text: qsTr("Хто працює?")
                    font.pixelSize: Theme.fontSizeHeading
                    font.weight: Font.Bold
                }
                GridLayout {
                    columns: 2
                    columnSpacing: Theme.spacingM
                    rowSpacing: Theme.spacingM
                    Layout.fillWidth: true

                    Repeater {
                        model: Terminal.operators

                        OperatorCard {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                        }
                    }
                }
                Label {
                    text: qsTr("PIN — %1 (4–6 цифр)").arg(screen.pickedName)
                    color: Theme.textMuted
                    Layout.topMargin: Theme.spacingS
                }
                TextField {
                    id: pin
                    echoMode: TextInput.Password
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: RegularExpressionValidator { regularExpression: /[0-9]{0,6}/ }
                    font.pixelSize: Theme.fontSizeHeading
                    font.letterSpacing: Theme.spacingXs
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.controlHeightXl
                    onTextEdited: error.visible = false
                    onAccepted: if (acceptableInput && text.length >= 4) screen.tryLogin()
                }
                Label {
                    id: error
                    visible: false
                    text: qsTr("Невірний PIN")
                    color: Theme.danger
                }
                Button {
                    text: qsTr("Увійти  ·  Enter")
                    highlighted: true
                    font.pixelSize: Theme.fontSizeField
                    enabled: pin.text.length >= 4 && screen.picked !== undefined
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.controlHeightXl - Theme.spacingXs
                    onClicked: screen.tryLogin()
                }
            }
        }
    }
}
