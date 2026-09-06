import QtQuick
import QtQuick.Controls
import Keys.Ui

/// The terminal: a tab strip and the shell's screen under it.
///
/// **The screen is a list, not a grid.** Each line is one Text item drawing a
/// run of markup, which is what makes a full scrollback affordable - a cell per
/// item would be tens of thousands of items for an ordinary session.
///
/// **Keys go to the shell, not to Qt.** A terminal has no shortcuts of its own:
/// Ctrl+C interrupts, Tab completes, the arrows walk history. Everything the
/// panel does not need for itself is forwarded, which is why the key handling
/// is explicit rather than left to the default focus behaviour.
Item {
    id: root

    /// Focuses the screen so typing reaches the shell.
    function takeFocus() {
        screen.forceActiveFocus();
    }

    // ---- Tab strip --------------------------------------------------------

    Rectangle {
        id: tabStrip

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 30
        color: Theme.bgChromeSunken

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.borderFaint
        }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: Metrics.spacingSmall
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Repeater {
                model: Terminal.sessionCount

                Rectangle {
                    required property int index

                    readonly property bool current: Terminal.currentSession === index

                    width: label.implicitWidth + closeButton.width + 20
                    height: 24
                    radius: Metrics.radiusSmall
                    color: current ? Theme.bgEditor
                         : tabMouse.containsMouse ? Theme.bgHover
                         : "transparent"

                    Behavior on color {
                        ColorAnimation { duration: App.fastAnimationDuration }
                    }

                    // The active tab is marked by a line, not by a fill: the
                    // fill also says "hovered", and one signal should not mean
                    // two things.
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width - 12
                        height: 2
                        radius: 1
                        color: Theme.activeIndicator
                        opacity: parent.current ? 1 : 0

                        Behavior on opacity {
                            NumberAnimation { duration: App.fastAnimationDuration }
                        }
                    }

                    Text {
                        id: label

                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: Terminal.titleAt(parent.index)
                        color: parent.current ? Theme.textPrimary : Theme.textSecondary
                        font.family: Fonts.ui
                        font.pointSize: Metrics.fontSizeLabel
                    }

                    IconButton {
                        id: closeButton

                        anchors.right: parent.right
                        anchors.rightMargin: 2
                        anchors.verticalCenter: parent.verticalCenter
                        source: Icons.close
                        size: 18
                        tooltip: qsTr("Close terminal")
                        onClicked: Terminal.closeSession(parent.index)
                    }

                    MouseArea {
                        id: tabMouse

                        anchors.fill: parent
                        anchors.rightMargin: closeButton.width
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Terminal.currentSession = parent.index
                    }
                }
            }
        }

        IconButton {
            anchors.right: parent.right
            anchors.rightMargin: Metrics.spacingSmall
            anchors.verticalCenter: parent.verticalCenter
            source: Icons.plus
            tooltip: qsTr("New terminal")
            onClicked: {
                Terminal.openSession();
                root.takeFocus();
            }
        }
    }

    // ---- Nothing open -----------------------------------------------------

    Column {
        anchors.centerIn: parent
        spacing: Metrics.spacingMedium
        visible: Terminal.sessionCount === 0

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: App.hasProject ? qsTr("No terminal is running.")
                                 : qsTr("Open a project to start a terminal.")
            color: Theme.textTertiary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeBody
        }

        PrimaryButton {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("New Terminal")
            visible: App.hasProject
            onClicked: {
                Terminal.openSession();
                root.takeFocus();
            }
        }
    }

    // ---- The screen -------------------------------------------------------

    ListView {
        id: screen

        anchors.top: tabStrip.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Metrics.spacingSmall

        visible: Terminal.sessionCount > 0
        clip: true
        model: Terminal
        focus: true
        spacing: 0

        // A terminal follows its cursor, not its last row. The grid is padded
        // to its full height with blank lines, so scrolling to the end would
        // park the view below everything the shell has written - which looks
        // exactly like a terminal that produced no output.
        //
        // Deliberately not animated: a build scrolling smoothly would be
        // unreadable, and the eye follows a jump better than a glide.
        function followCursor() {
            positionViewAtIndex(Math.min(Terminal.cursorLine, count - 1),
                                ListView.Contain);
        }

        onCountChanged: followCursor()

        Connections {
            target: Terminal
            function onScreenChanged() { screen.followCursor(); }
        }

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        // A fixed row height, because a terminal is a grid. Letting Text size
        // itself collapses every blank line to nothing, so the rows stop lining
        // up with the shell that drew them - and positionViewAtEnd then scrolls
        // past the output into the empty tail, which reads as a blank terminal.
        readonly property int rowHeight: Math.ceil(metrics.height)

        TextMetrics {
            id: metrics
            font.family: Fonts.mono
            font.pointSize: Terminal.fontSize
            text: "Xg"
        }

        delegate: Text {
            required property string markup

            width: screen.width
            height: screen.rowHeight
            text: markup
            textFormat: Text.StyledText
            color: Theme.textPrimary
            font.family: Fonts.mono
            font.pointSize: Terminal.fontSize
            renderType: Text.NativeRendering
            verticalAlignment: Text.AlignVCenter
        }

        // Everything typed goes to the shell. Handled here rather than through
        // per-key Shortcuts so that Ctrl+C reaches the shell as an interrupt
        // instead of being swallowed as a copy accelerator.
        Keys.onPressed: (event) => {
            if (Terminal.sessionCount === 0) {
                return;
            }

            // Ctrl+Shift+C copies, because the shell's Ctrl+C cannot also.
            if (event.key === Qt.Key_C
                && (event.modifiers & Qt.ControlModifier)
                && (event.modifiers & Qt.ShiftModifier)) {
                Terminal.copyAll();
                event.accepted = true;
                return;
            }

            if (event.text.length > 0
                && !(event.modifiers & Qt.ControlModifier)
                && event.key !== Qt.Key_Return
                && event.key !== Qt.Key_Enter
                && event.key !== Qt.Key_Backspace
                && event.key !== Qt.Key_Tab
                && event.key !== Qt.Key_Escape) {
                Terminal.sendText(event.text);
            } else {
                Terminal.sendKey(event.key, event.modifiers);
            }
            event.accepted = true;
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onClicked: screen.forceActiveFocus()
        }
    }
}
