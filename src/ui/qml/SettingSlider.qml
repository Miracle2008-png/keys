import QtQuick
import Keys.Ui

/// A bounded numeric value, with the current number shown beside it.
///
/// The number is always visible rather than appearing in a tooltip while
/// dragging: these are values a user wants to set exactly (a font size of 14,
/// not "somewhere past the middle").
Item {
    id: root

    required property real value
    required property real minimum
    required property real maximum
    required property bool isInteger

    /// Emitted continuously while dragging, so the editor reflows live and the
    /// user can see the size they are choosing. Settings are written to disk on
    /// exit, so this costs no I/O.
    signal moved(real value)

    implicitWidth: 200
    implicitHeight: 24

    readonly property real _span: Math.max(0.0001, maximum - minimum)
    readonly property real _fraction:
        Math.max(0, Math.min(1, (value - minimum) / _span))

    function _quantize(raw) {
        const clamped = Math.max(root.minimum, Math.min(root.maximum, raw));
        // An integer setting must not be handed 4.37: the schema would reject it
        // and the control would appear to do nothing.
        return root.isInteger ? Math.round(clamped) : Math.round(clamped * 10) / 10;
    }

    function _valueAt(x) {
        return root._quantize(root.minimum + (x / Math.max(1, track.width)) * root._span);
    }

    Text {
        id: readout

        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 34
        horizontalAlignment: Text.AlignRight
        text: root.isInteger ? root.value : root.value.toFixed(1)
        color: Theme.textSecondary
        font.family: Fonts.mono
        font.pointSize: Metrics.fontSizeSmall
    }

    Item {
        id: track

        anchors.left: parent.left
        anchors.right: readout.left
        anchors.rightMargin: Metrics.spacingSmall
        anchors.verticalCenter: parent.verticalCenter
        height: parent.height

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            height: 4
            radius: 2
            color: Theme.bgHover
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width * root._fraction
            height: 4
            radius: 2
            color: Theme.accent
        }

        Rectangle {
            id: knob

            width: 14
            height: 14
            radius: 7
            anchors.verticalCenter: parent.verticalCenter
            x: parent.width * root._fraction - width / 2
            color: Theme.accent
            border.width: 2
            border.color: Theme.bgElevated
            scale: knobMouse.pressed ? 1.15 : 1

            Behavior on scale {
                NumberAnimation {
                    duration: App.fastAnimationDuration
                    easing.type: Easing.OutQuad
                }
            }
        }

        MouseArea {
            id: knobMouse

            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor

            // Click anywhere on the track to jump there, then keep dragging -
            // which is what a user expects and what makes the control usable
            // without hitting a 14px knob first.
            onPressed: (mouse) => root.moved(root._valueAt(mouse.x))
            onPositionChanged: (mouse) => {
                if (pressed) {
                    root.moved(root._valueAt(mouse.x));
                }
            }
        }
    }
}
