pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import BerryWms.Ui

// Three berries on a stem: the app's mark, drawn from the theme so it follows
// light and dark. Laid out on a 28-unit grid, as in the mockup, and scaled.
// On a dark ground pass light `berries` (left, right, bottom) and a `stemColor` that shows on it.
Item {
    id: mark

    property color color: Theme.accent
    property list<color> berries: [color, Qt.lighter(color, 1.35), Qt.darker(color, 1.35)]
    property color stemColor: Theme.ok
    property real size: Theme.brandMarkSize
    readonly property real unit: size / 28

    implicitWidth: size
    implicitHeight: size

    component Berry: Rectangle {
        required property real cx
        required property real cy
        x: (cx - 5) * mark.unit
        y: (cy - 5) * mark.unit
        width: 10 * mark.unit
        height: width
        radius: width / 2
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: mark.stemColor
            strokeWidth: 2 * mark.unit
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            startX: 14 * mark.unit
            startY: 3 * mark.unit
            PathCubic {
                x: 19 * mark.unit
                y: 6 * mark.unit
                control1X: 15 * mark.unit
                control1Y: 5 * mark.unit
                control2X: 17 * mark.unit
                control2Y: 6 * mark.unit
            }
        }
    }
    Berry { cx: 10; cy: 11; color: mark.berries[0] }
    Berry { cx: 18; cy: 11; color: mark.berries[1] }
    Berry { cx: 14; cy: 18; color: mark.berries[2] }
}
