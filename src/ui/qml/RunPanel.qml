import QtQuick
import QtQuick.Controls
import Keys.Ui

/// Run and Debug: the tasks a project can run, and the problems from the last
/// build.
///
/// Debugging is milestone 13. This view carries what exists — running tasks and
/// navigating their errors — and says nothing about what does not.
Item {
    id: root

    Text {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Metrics.spacingMedium
        visible: Runner.tasks.length === 0 && !Debugger.configured
        text: qsTr("No tasks for this project. Keys offers defaults for CMake, "
                   + "Cargo, Go, npm and Python projects.")
        color: Theme.textTertiary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeBody
        wrapMode: Text.WordWrap
    }

    Column {
        id: content

        anchors.fill: parent
        anchors.leftMargin: Metrics.spacingMedium
        anchors.rightMargin: Metrics.spacingMedium
        spacing: Metrics.spacingSmall
        visible: Runner.tasks.length > 0 || Debugger.configured

        // ---- Debug ----
        //
        // Shown only when Keys has an adapter for this kind of project: a
        // control that can never work is worse than an absent one.

        Item {
            width: parent.width
            height: 30
            visible: Debugger.configured

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                IconButton {
                    size: 22
                    source: Debugger.active ? Icons.stop : Icons.play
                    tooltip: Debugger.active ? qsTr("Stop debugging")
                                             : qsTr("Start debugging")
                    onClicked: Debugger.active ? Debugger.stop() : Debugger.start()
                }

                // Execution controls appear only during a session, and each is
                // enabled from the session state rather than deciding for
                // itself - so none offers what the adapter would refuse.
                IconButton {
                    size: 22
                    visible: Debugger.active
                    enabled: Debugger.stopped
                    source: Icons.play
                    tooltip: qsTr("Continue")
                    onClicked: Debugger.resume()
                }

                IconButton {
                    size: 22
                    visible: Debugger.active
                    enabled: Debugger.stopped
                    source: Icons.stepOver
                    tooltip: qsTr("Step over")
                    onClicked: Debugger.stepOver()
                }

                IconButton {
                    size: 22
                    visible: Debugger.active
                    enabled: Debugger.stopped
                    source: Icons.stepInto
                    tooltip: qsTr("Step into")
                    onClicked: Debugger.stepInto()
                }

                IconButton {
                    size: 22
                    visible: Debugger.active
                    enabled: Debugger.stopped
                    source: Icons.stepOut
                    tooltip: qsTr("Step out")
                    onClicked: Debugger.stepOut()
                }
            }

            Text {
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                visible: Debugger.active
                text: Debugger.stopped && Debugger.stopReason.length > 0
                      ? Debugger.stateLabel + " \u00b7 " + Debugger.stopReason
                      : Debugger.stateLabel
                color: Debugger.stopped ? Theme.yellow : Theme.textTertiary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeSmall
                elide: Text.ElideLeft
                width: Math.min(implicitWidth, parent.width - 160)
            }
        }

        // ---- Call stack ----

        Item {
            width: parent.width
            height: 22
            visible: Debugger.stack.length > 0

            SectionLabel {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Call Stack")
            }
        }

        ListView {
            id: stackList

            width: parent.width
            height: Math.min(contentHeight, 140)
            visible: Debugger.stack.length > 0
            model: Debugger.stack
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                id: frameRow

                required property int index
                required property var modelData

                width: stackList.width
                height: 26
                radius: Metrics.radiusSmall
                color: frameHover.hovered && frameRow.modelData.hasSource
                       ? Theme.bgHover : "transparent"

                HoverHandler {
                    id: frameHover
                    cursorShape: frameRow.modelData.hasSource ? Qt.PointingHandCursor
                                                              : Qt.ArrowCursor
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.right: frameLocation.left
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    text: frameRow.modelData.name
                    // A frame with no source is dimmed: it is real, and part of
                    // the call chain, but there is nothing to open.
                    color: frameRow.modelData.hasSource ? Theme.textSecondary
                                                        : Theme.textTertiary
                    font.family: Fonts.mono
                    font.pointSize: Metrics.fontSizeSmall
                    elide: Text.ElideMiddle
                }

                Text {
                    id: frameLocation

                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    visible: frameRow.modelData.hasSource
                    text: frameRow.modelData.fileName + ":" + frameRow.modelData.line
                    color: Theme.textTertiary
                    font.family: Fonts.mono
                    font.pointSize: Metrics.fontSizeLabel
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: frameRow.modelData.hasSource
                    onClicked: Debugger.selectFrame(frameRow.index)
                }
            }
        }

        // ---- Variables ----

        Item {
            width: parent.width
            height: 22
            visible: Debugger.variables.length > 0

            SectionLabel {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Variables")
            }
        }

        ListView {
            id: variableList

            width: parent.width
            height: Math.min(contentHeight, 160)
            visible: Debugger.variables.length > 0
            model: Debugger.variables
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Item {
                id: variableRow

                required property var modelData

                width: variableList.width
                height: 22

                Text {
                    id: variableName

                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: variableRow.modelData.name
                    color: Theme.synKeyword
                    font.family: Fonts.mono
                    font.pointSize: Metrics.fontSizeLabel
                    width: Math.min(implicitWidth, parent.width * 0.4)
                    elide: Text.ElideRight
                }

                Text {
                    anchors.left: variableName.right
                    anchors.leftMargin: 8
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: variableRow.modelData.value
                    color: Theme.textSecondary
                    font.family: Fonts.mono
                    font.pointSize: Metrics.fontSizeLabel
                    elide: Text.ElideRight
                }
            }
        }

        // ---- Tasks ----

        Column {
            width: parent.width
            spacing: 2

            Repeater {
                model: Runner.tasks

                delegate: Rectangle {
                    id: taskRow

                    required property int index
                    required property var modelData

                    width: parent.width
                    height: 30
                    radius: Metrics.radiusSmall
                    color: taskHover.hovered ? Theme.bgHover : "transparent"

                    HoverHandler {
                        id: taskHover
                        cursorShape: Runner.running ? Qt.ArrowCursor
                                                      : Qt.PointingHandCursor
                    }

                    Icon {
                        id: taskIcon

                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        source: taskRow.modelData.kind === "Run" ? Icons.play
                                                                 : Icons.hammer
                        size: 13
                        color: Runner.running ? Theme.textTertiary : Theme.textSecondary
                    }

                    Text {
                        anchors.left: taskIcon.right
                        anchors.leftMargin: 8
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: taskRow.modelData.name
                        color: Runner.running ? Theme.textTertiary : Theme.textSecondary
                        font.family: Fonts.ui
                        font.pointSize: Metrics.fontSizeBody
                        elide: Text.ElideRight
                    }

                    MouseArea {
                        anchors.fill: parent
                        // Refused rather than queued while something runs: two
                        // builds writing to one output directory corrupt each
                        // other, so this is a decision for the user to make.
                        enabled: !Runner.running
                        onClicked: Runner.runTask(taskRow.index)
                    }
                }
            }
        }

        // ---- Status ----

        Item {
            width: parent.width
            height: 30
            visible: Runner.summary.length > 0

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.right: stopButton.left
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                text: Runner.summary
                color: Runner.lastRunFailed ? Theme.red : Theme.textTertiary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeSmall
                elide: Text.ElideRight
            }

            IconButton {
                id: stopButton

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                size: 22
                visible: Runner.running
                source: Icons.stop
                tooltip: qsTr("Stop")
                onClicked: Runner.stop()
            }
        }

        // ---- Problems ----

        Item {
            width: parent.width
            height: 24
            visible: Runner.problems.length > 0

            SectionLabel {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Problems")
            }

            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: (Runner.errorCount > 0
                       ? Runner.errorCount + qsTr(" error") + (Runner.errorCount === 1 ? "" : "s")
                       : "")
                      + (Runner.errorCount > 0 && Runner.warningCount > 0 ? ", " : "")
                      + (Runner.warningCount > 0
                         ? Runner.warningCount + qsTr(" warning") + (Runner.warningCount === 1 ? "" : "s")
                         : "")
                color: Runner.errorCount > 0 ? Theme.red : Theme.yellow
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeSmall
            }
        }

        ListView {
            id: problemList

            width: parent.width
            height: Math.min(contentHeight, root.height * 0.45)
            visible: Runner.problems.length > 0

            model: Runner.problems
            clip: true
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                id: problemRow

                required property int index
                required property var modelData

                width: problemList.width
                height: 36
                radius: Metrics.radiusSmall
                color: problemHover.hovered ? Theme.bgHover : "transparent"

                HoverHandler {
                    id: problemHover
                    cursorShape: Qt.PointingHandCursor
                }

                // A coloured bar rather than an icon: at this size a glyph is
                // unreadable, and severity only needs to be distinguishable.
                Rectangle {
                    id: severityBar

                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 3
                    height: 20
                    radius: 1.5
                    color: problemRow.modelData.severity === 0 ? Theme.red
                         : problemRow.modelData.severity === 1 ? Theme.yellow
                         : Theme.textTertiary
                }

                Text {
                    anchors.left: severityBar.right
                    anchors.leftMargin: 8
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.top: parent.top
                    anchors.topMargin: 3
                    text: problemRow.modelData.message
                    color: Theme.textSecondary
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeSmall
                    elide: Text.ElideRight
                }

                Text {
                    anchors.left: severityBar.right
                    anchors.leftMargin: 8
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 3
                    text: problemRow.modelData.fileName
                          + (problemRow.modelData.line > 0
                             ? ":" + problemRow.modelData.line : "")
                    color: Theme.textTertiary
                    font.family: Fonts.mono
                    font.pointSize: Metrics.fontSizeLabel
                    elide: Text.ElideLeft
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: Runner.openProblem(problemRow.index)
                }
            }
        }

        // ---- Output ----

        Item {
            width: parent.width
            height: 24
            visible: Runner.count > 0

            SectionLabel {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Output")
            }

            IconButton {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                size: 20
                source: Icons.trash
                tooltip: qsTr("Clear output")
                onClicked: Runner.clear()
            }
        }

        Rectangle {
            width: parent.width
            height: Math.max(0, content.height - y - Metrics.spacingMedium)
            visible: Runner.count > 0
            color: Theme.bgEditor
            radius: Metrics.radiusSmall
            clip: true

            ListView {
                id: output

                anchors.fill: parent
                anchors.margins: 6

                model: Runner
                clip: true
                reuseItems: true
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Text {
                    id: outputLine

                    // Named differently from the Text's own `text` property,
                    // which the role would otherwise shadow into a self-reference.
                    required property string lineText
                    required property bool isError
                    required property int severity
                    required property bool isCommand

                    width: output.width
                    text: outputLine.lineText
                    // Errors and warnings are coloured by what the parser found
                    // rather than by which stream they arrived on: plenty of
                    // tools write ordinary progress to stderr.
                    color: outputLine.severity === 0 ? Theme.red
                         : outputLine.severity === 1 ? Theme.yellow
                         : outputLine.isCommand ? Theme.accent
                         : Theme.synPlain
                    font.family: Fonts.mono
                    font.pointSize: Metrics.fontSizeSmall
                    wrapMode: Text.NoWrap
                    elide: Text.ElideNone
                }

                // Follows the tail while output arrives, which is what a build
                // console is for.
                Connections {
                    target: Runner
                    function onLineAppended() {
                        output.positionViewAtEnd();
                    }
                }
            }
        }
    }
}
