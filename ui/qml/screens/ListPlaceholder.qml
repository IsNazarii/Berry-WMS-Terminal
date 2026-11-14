import QtQuick
import QtQuick.Controls

// What an empty side list says in place of its rows ("Відкритих чеків немає").
// Put inside the ListView; the list decides when it shows.
Label {
    anchors.centerIn: parent
    color: Theme.textMuted
}
