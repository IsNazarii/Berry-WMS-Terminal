pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import QtQuick.Templates as T
import BerryWms.Ui

// Over every screen: who we are, where to go, what state the day is in and who works.
// The operator reads the session and the scale at a glance instead of in a footer.
ToolBar {
    id: bar

    // The screen shown below; the screens' StackLayout follows it.
    property alias currentIndex: tabs.currentIndex
    // The window's settings, shown in the menu; a choice goes back through the signals.
    required property real uiScale
    required property bool fullScreen
    // The scale this run was started with (set by main.cpp), to tell the operator a restart is pending.
    required property real startScale
    // Six tabs: below this the bar keeps icons and names only (measured, with a long operator name).
    readonly property bool compact: width < 1480
    // Down to the minimum window (1024) the brand keeps only its mark, or the menu falls off the edge.
    readonly property bool tight: width < 1180

    signal scalePicked(real factor)
    signal fullScreenToggled()

    // A rounded status pill; the session one also takes the operator to the session screen.
    component Chip: T.AbstractButton {
        id: chip
        property color dotColor: "transparent"
        property color textColor: Theme.textMuted

        implicitWidth: implicitContentWidth + leftPadding + rightPadding
        implicitHeight: Theme.avatarSize
        horizontalPadding: Theme.spacingM
        spacing: Theme.spacingS
        font.pixelSize: Theme.fontSizeBody
        icon.width: Theme.iconSizeSmall
        icon.height: Theme.iconSizeSmall

        contentItem: Row {
            spacing: chip.spacing

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                visible: chip.dotColor.a > 0
                width: Theme.statusDotSize
                height: Theme.statusDotSize
                radius: width / 2
                color: chip.dotColor
            }
            IconImage {
                anchors.verticalCenter: parent.verticalCenter
                visible: chip.icon.source.toString() !== ""
                source: chip.icon.source
                sourceSize: Qt.size(chip.icon.width, chip.icon.height)
                color: chip.textColor
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: text !== ""
                text: chip.text
                font: chip.font
                color: chip.textColor
            }
        }
        background: Rectangle {
            radius: height / 2
            color: chip.hovered ? Theme.surfaceAlt : Theme.bg
            border.width: chip.visualFocus ? 2 : 0
            border.color: Theme.accent
        }
    }

    // Not "scale": that is every Item's own transform.
    component ScaleItem: MenuItem {
        required property real factor
        text: qsTr("%1%").arg(Math.round(factor * 100))
        checkable: true
        checked: bar.uiScale === factor
        onTriggered: bar.scalePicked(factor)
    }

    // What the box tare field on the receiving screen starts with.
    Dialog {
        id: tareDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 360
        modal: true
        title: qsTr("Тара ящика за замовчуванням")
        standardButtons: Dialog.Cancel

        function save() {
            tareError.text = Terminal.setDefaultBoxTare(tareField.text)
            if (tareError.text === "")
                close()
        }

        onOpened: {
            tareField.text = Terminal.defaultBoxTare
            tareError.text = ""
            tareField.selectAll()
            tareField.forceActiveFocus()
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.spacingS

            Label {
                text: qsTr("Вага порожнього ящика, кг. Нею починається поле «Тара ящика» на «Прийомі»; "
                           + "для іншого ящика її можна змінити там же.")
                color: Theme.textMuted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            TextField {
                id: tareField
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                Layout.fillWidth: true
                onTextEdited: tareError.text = ""
                onAccepted: tareDialog.save()
            }
            Label {
                id: tareError
                visible: text !== ""
                color: Theme.danger
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Зберегти")
                highlighted: true
                enabled: tareField.text.trim() !== ""
                Layout.fillWidth: true
                onClicked: tareDialog.save()
            }
        }
    }

    // Which scale this terminal reads. Picking switches at once, so the live reading
    // below confirms the port before the dialog is closed.
    Dialog {
        id: scaleDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 480
        modal: true
        title: qsTr("Ваги")
        standardButtons: Dialog.Close

        onOpened: {
            portBox.model = [{ path: "", description: qsTr("Симулятор ваг") }].concat(Terminal.scalePorts())
            portBox.currentIndex = portBox.indexOfValue(Terminal.scalePort)
            portBox.forceActiveFocus()
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.spacingS

            Label {
                text: qsTr("Порт, до якого під'єднано ваги (ІЕ-04, 4800 8N1). Без порту — симулятор.")
                color: Theme.textMuted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            ComboBox {
                id: portBox
                textRole: "description"
                valueRole: "path"
                Layout.fillWidth: true
                onActivated: Terminal.setScalePort(currentValue)
            }
            Label {
                visible: text !== ""
                text: Terminal.scalePort
                font.pixelSize: Theme.fontSizeCaption
                color: Theme.textMuted
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
            Label {
                text: Terminal.scales.online
                      ? qsTr("Зараз: %1 кг%2").arg(Terminal.scales.display)
                                             .arg(Terminal.scales.stable ? qsTr(", стабільно") : "")
                      : qsTr("Немає ваги: перевірте кабель, живлення ваг і права на порт (група dialout).")
                color: Terminal.scales.online ? Theme.ok : Theme.danger
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: Theme.spacingXl

        RowLayout {
            spacing: Theme.spacingS

            BrandMark {}
            ColumnLayout {
                visible: !bar.tight
                spacing: 0

                Label {
                    text: qsTr("Berry WMS")
                    font.pixelSize: Theme.fontSizeField
                    font.weight: Font.Bold
                }
                Label {
                    visible: !bar.compact
                    text: qsTr("термінал оператора")
                    font.pixelSize: Theme.fontSizeCaption
                    color: Theme.textMuted
                }
            }
        }

        // Operator workflow goes left to right: open the day, receive, load, ship from stock, pay.
        TabBar {
            id: tabs

            TabButton {
                text: qsTr("Сесія")
                icon.source: Theme.icon("calendar")
                hint: "F5"
                compact: bar.compact
            }
            TabButton {
                text: qsTr("Прийом")
                icon.source: Theme.icon("weight")
                hint: "F6"
                compact: bar.compact
            }
            TabButton {
                text: qsTr("Розподіл")
                icon.source: Theme.icon("truck")
                hint: "F7"
                compact: bar.compact
                // Receipts with pallets nobody has placed yet.
                badge: Terminal.allocationSources.length
            }
            TabButton {
                text: qsTr("Склад")
                icon.source: Theme.icon("warehouse")
                hint: "F8"
                compact: bar.compact
            }
            TabButton {
                text: qsTr("Каса")
                icon.source: Theme.icon("wallet")
                hint: "F9"
                compact: bar.compact
            }
            // Not a step of the day: looking back at what was loaded where.
            TabButton {
                text: qsTr("Історія")
                icon.source: Theme.icon("history")
                hint: "F10"
                compact: bar.compact
            }
        }

        Item { Layout.fillWidth: true }

        RowLayout {
            spacing: Theme.spacingS

            Chip {
                text: bar.compact ? "" : Terminal.scales.online ? Terminal.scales.driverName
                                                                : qsTr("%1 — немає зв'язку").arg(Terminal.scales.driverName)
                dotColor: Terminal.scales.online ? Theme.ok : Theme.danger
                textColor: Terminal.scales.online ? Theme.textMuted : Theme.danger
                hoverEnabled: false
                focusPolicy: Qt.NoFocus
            }
            Chip {
                text: Terminal.sessionOpen ? Terminal.sessionTitle : qsTr("Сесію не відкрито")
                icon.source: Theme.icon("calendar")
                textColor: Terminal.sessionOpen ? Theme.text : Theme.danger
                onClicked: bar.currentIndex = 0
            }
            RowLayout {
                spacing: Theme.spacingS

                Rectangle {
                    implicitWidth: Theme.avatarSize
                    implicitHeight: Theme.avatarSize
                    radius: width / 2
                    color: Theme.accent

                    Label {
                        anchors.centerIn: parent
                        text: Terminal.operatorName.charAt(0)
                        font.weight: Font.DemiBold
                        color: Theme.textOnAccent
                    }
                }
                Label {
                    visible: !bar.compact
                    text: Terminal.operatorName
                    font.weight: Font.Medium
                    // A long name must not push the menu off the bar.
                    elide: Text.ElideRight
                    Layout.maximumWidth: 140
                }
            }

            // Settings of the terminal and the way out, rarely used, so out of sight.
            ToolButton {
                icon.source: Theme.icon("more")
                implicitWidth: Theme.controlHeight
                implicitHeight: Theme.controlHeight
                Accessible.name: qsTr("Меню")
                onClicked: menu.open()

                Menu {
                    id: menu
                    y: parent.height
                    x: parent.width - width

                    // Set once per terminal: laptops differ, and Qt only takes a scale at startup.
                    Menu {
                        title: bar.startScale === bar.uiScale
                               ? qsTr("Масштаб %1%").arg(Math.round(bar.uiScale * 100))
                               : qsTr("Масштаб %1% — після перезапуску").arg(Math.round(bar.uiScale * 100))
                        ScaleItem { factor: 1 }
                        ScaleItem { factor: 1.25 }
                        ScaleItem { factor: 1.5 }
                        ScaleItem { factor: 1.75 }
                    }
                    MenuItem {
                        text: qsTr("Ваги: %1…").arg(Terminal.scales.driverName)
                        onTriggered: scaleDialog.open()
                    }
                    Menu {
                        title: qsTr("Папір чека: %1").arg(Terminal.receiptPaper)
                        MenuItem {
                            text: "A4"
                            checkable: true
                            checked: Terminal.receiptPaper === "A4"
                            onTriggered: Terminal.setReceiptPaper("A4")
                        }
                        MenuItem {
                            text: "A5"
                            checkable: true
                            checked: Terminal.receiptPaper === "A5"
                            onTriggered: Terminal.setReceiptPaper("A5")
                        }
                    }
                    // Crates differ between points, so the usual one is a setting of the terminal.
                    MenuItem {
                        text: qsTr("Тара ящика: %1 кг…").arg(Terminal.defaultBoxTare)
                        onTriggered: tareDialog.open()
                    }
                    // In full screen there is no window frame, so this is the way back too.
                    MenuItem {
                        text: bar.fullScreen ? qsTr("Вікно (F11)") : qsTr("Повний екран (F11)")
                        onTriggered: bar.fullScreenToggled()
                    }
                    // A shed in daylight wants light, an evening shift wants dark.
                    MenuItem {
                        text: Theme.dark ? qsTr("Світла тема") : qsTr("Темна тема")
                        onTriggered: Theme.dark = !Theme.dark
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: qsTr("Вийти")
                        onTriggered: Terminal.logout()
                    }
                }
            }
        }
    }
}
