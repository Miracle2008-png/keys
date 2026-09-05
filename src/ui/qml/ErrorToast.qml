import QtQuick
import Keys.Ui

/// A transient message for an operation the user initiated that failed.
///
/// Errors surface here rather than in a modal: opening the wrong folder is a
/// small mistake, and a dialog demanding acknowledgement for it would interrupt
/// far more than the error warrants. It dismisses itself, and can be dismissed
/// early by clicking it.
Rectangle {
    id: root

    property alias text: label.text

    width: Math.min(label.implicitWidth + Metrics.spacingLarge * 2, 520)
    height: label.implicitHeight + Metrics.spacingMedium * 2
    radius: Metrics.radiusMedium
    color: Theme.bgElevated
    border.width: 1
    border.color: Theme.red
    visible: opacity > 0
    opacity: 0

    // Fades rather than appearing abruptly, and respects the animation setting:
    // at "off" it simply appears and disappears.
    Behavior on opacity {
        NumberAnimation {
            duration: App.animationDuration
            easing.type: Easing.OutCubic
        }
    }

    Row {
        anchors.centerIn: parent
        spacing: Metrics.spacingSmall

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 6
            height: 6
            radius: 3
            color: Theme.red
        }

        Text {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, 460)
            color: Theme.textPrimary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeBody
            wrapMode: Text.WordWrap
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.dismiss()
    }

    Timer {
        id: dismissTimer
        // Long enough to read a path, short enough not to linger.
        interval: 6000
        onTriggered: root.dismiss()
    }

    function show(message) {
        label.text = message;
        opacity = 1;
        dismissTimer.restart();
    }

    function dismiss() {
        opacity = 0;
        dismissTimer.stop();
    }
}
