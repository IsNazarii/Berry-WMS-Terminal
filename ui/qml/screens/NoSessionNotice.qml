import QtQuick
import BerryWms.Ui

// Shown instead of a screen that needs an open session: says why and leads
// to the session screen, where the day is opened deliberately.
EmptyState {
    id: notice
    signal sessionRequested()

    anchors.centerIn: parent
    icon: "calendar"
    title: qsTr("Сесію не відкрито")
    actionText: qsTr("До сесії  ·  F5")
    onActionTriggered: notice.sessionRequested()
}
