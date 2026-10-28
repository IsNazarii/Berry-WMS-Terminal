pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BerryWms.Ui
import "screens"

ApplicationWindow {
    id: window
    width: 1024
    height: 700
    minimumWidth: 1024
    minimumHeight: 600
    // A terminal on a laptop: the whole screen; the size above is what "restore" gives.
    // Full screen hides the desktop panels too, but some operators want them, so it is an option.
    visibility: uiSettings.fullScreen ? Window.FullScreen : Window.Maximized
    title: qsTr("Berry WMS — термінал приймальника")

    // Our style (ui/style) draws from Theme; controls it does not override
    // fall back to Basic, which draws from the palette — so it gets the tokens too.
    color: Theme.bg
    font.family: Theme.fontUi
    font.pixelSize: Theme.fontSizeBody
    font.features: Theme.fontFeatures
    palette {
        window: Theme.bg
        windowText: Theme.text
        base: Theme.surface
        alternateBase: Theme.surfaceAlt
        text: Theme.text
        button: Theme.surface
        buttonText: Theme.text
        highlight: Theme.accent
        highlightedText: Theme.textOnAccent
        placeholderText: Theme.textMuted
        toolTipBase: Theme.surface
        toolTipText: Theme.text
        light: Theme.surface
        midlight: Theme.surfaceAlt
        mid: Theme.border
        dark: Theme.border
        shadow: Theme.border
        brightText: Theme.textOnAccent
        link: Theme.accent
    }

    // Read by main.cpp at startup (key "ui/scale"); a change applies after a restart.
    Settings {
        id: uiSettings
        category: "ui"
        property real scale: 1.25
        property bool fullScreen: false
    }

    Shortcut {
        sequence: "F11"
        onActivated: uiSettings.fullScreen = !uiSettings.fullScreen
    }
    // The scale this run was started with (set by main.cpp), to tell the operator a restart is pending.
    required property real startScale

    header: AppBar {
        id: appBar
        visible: Terminal.loggedIn
        uiScale: uiSettings.scale
        fullScreen: uiSettings.fullScreen
        startScale: window.startScale
        onScalePicked: factor => uiSettings.scale = factor
        onFullScreenToggled: uiSettings.fullScreen = !uiSettings.fullScreen
    }

    // One key per screen, in the order of the tabs; F2 (new receipt / supplier) stays with the screens.
    Shortcut { sequence: "F5"; enabled: Terminal.loggedIn; onActivated: appBar.currentIndex = 0 }
    Shortcut { sequence: "F6"; enabled: Terminal.loggedIn; onActivated: appBar.currentIndex = 1 }
    Shortcut { sequence: "F7"; enabled: Terminal.loggedIn; onActivated: appBar.currentIndex = 2 }
    Shortcut { sequence: "F8"; enabled: Terminal.loggedIn; onActivated: appBar.currentIndex = 3 }
    Shortcut { sequence: "F9"; enabled: Terminal.loggedIn; onActivated: appBar.currentIndex = 4 }

    // Nothing is recorded without knowing who did it, so the screens wait for a login.
    Connections {
        target: Terminal
        function onOperatorChanged() { appBar.currentIndex = 0 }
    }

    LoginScreen {
        anchors.fill: parent
        visible: !Terminal.loggedIn
    }

    StackLayout {
        anchors.fill: parent
        visible: Terminal.loggedIn
        currentIndex: appBar.currentIndex

        SessionScreen {}
        ReceivingScreen { onSessionRequested: appBar.currentIndex = 0 }
        AllocationScreen { onSessionRequested: appBar.currentIndex = 0 }
        WarehouseScreen {}
        CashScreen {}
    }
}
