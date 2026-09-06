import QtQuick
import QtQuick.Controls
import Keys.Ui

/// The source control view: a commit box above the changed files.
///
/// The message box is at the top and the changes below it, so committing reads
/// top to bottom: write what you did, check what is going in, commit.
Item {
    id: root

    // ---- States that are not a list of changes ----

    Text {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Metrics.spacingMedium
        visible: !SourceControl.gitAvailable
        text: qsTr("Git was not found on your PATH. Install git to use source control.")
        color: Theme.textTertiary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeBody
        wrapMode: Text.WordWrap
    }

    Text {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Metrics.spacingMedium
        visible: SourceControl.gitAvailable && !SourceControl.hasRepository
        text: qsTr("This project is not a git repository.")
        color: Theme.textTertiary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeBody
        wrapMode: Text.WordWrap
    }

    // ---- The repository ----

    Item {
        anchors.fill: parent
        visible: SourceControl.gitAvailable && SourceControl.hasRepository

        Column {
            id: header

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Metrics.spacingMedium
            anchors.rightMargin: Metrics.spacingMedium
            spacing: Metrics.spacingSmall

            // Branch, and how far it has diverged from its upstream.
            Row {
                width: parent.width
                spacing: 6

                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    source: Icons.git
                    size: 13
                    color: Theme.textSecondary
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: SourceControl.branch
                    color: Theme.textSecondary
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeBody
                    elide: Text.ElideMiddle
                    width: Math.min(implicitWidth, parent.width - 90)
                }

                // Shown only with an upstream: without one, ahead and behind are
                // meaningless rather than zero.
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: SourceControl.hasUpstream
                             && (SourceControl.ahead > 0 || SourceControl.behind > 0)
                    text: (SourceControl.behind > 0 ? "↓" + SourceControl.behind : "")
                          + (SourceControl.ahead > 0 && SourceControl.behind > 0 ? " " : "")
                          + (SourceControl.ahead > 0 ? "↑" + SourceControl.ahead : "")
                    color: Theme.textTertiary
                    font.family: Fonts.mono
                    font.pointSize: Metrics.fontSizeSmall
                }
            }

            // An in-progress merge or rebase changes what the user can do, so it
            // is stated rather than left to be inferred from a failed commit.
            Rectangle {
                width: parent.width
                height: visible ? operationLabel.implicitHeight + 12 : 0
                visible: SourceControl.operationName.length > 0
                radius: Metrics.radiusSmall
                color: Theme.yellow
                opacity: 0.15
            }

            Text {
                id: operationLabel

                width: parent.width
                visible: SourceControl.operationName.length > 0
                text: qsTr("A %1 is in progress.").arg(SourceControl.operationName)
                color: Theme.yellow
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            // ---- Commit message ----

            Rectangle {
                width: parent.width
                height: 68
                radius: Metrics.radiusSmall
                color: Theme.bgChrome
                border.width: 1
                border.color: messageInput.activeFocus ? Theme.accent : Theme.border

                Behavior on border.color {
                    ColorAnimation { duration: App.fastAnimationDuration }
                }

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: Metrics.spacingSmall
                    clip: true

                    TextArea {
                        id: messageInput

                        placeholderText: qsTr("Message")
                        placeholderTextColor: Theme.textTertiary
                        color: Theme.textPrimary
                        font.family: Fonts.ui
                        font.pointSize: Metrics.fontSizeBody
                        wrapMode: TextArea.Wrap
                        selectionColor: Theme.selection
                        selectedTextColor: Theme.textPrimary
                        background: null

                        // Ctrl+Enter commits, which is the shortcut every editor
                        // uses for exactly this box.
                        Keys.onPressed: (event) => {
                            if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                                && (event.modifiers & Qt.ControlModifier)) {
                                root.commit();
                                event.accepted = true;
                            }
                        }
                    }
                }
            }

            PrimaryButton {
                width: parent.width
                text: SourceControl.stagedCount > 0
                      ? qsTr("Commit %1 file%2").arg(SourceControl.stagedCount)
                            .arg(SourceControl.stagedCount === 1 ? "" : "s")
                      : qsTr("Commit")
                enabled: SourceControl.canCommit && messageInput.text.trim().length > 0
                onClicked: root.commit()
            }
        }

        // ---- Changed files ----

        Text {
            id: cleanLabel

            anchors.top: header.bottom
            anchors.topMargin: Metrics.spacingLarge
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Metrics.spacingMedium
            anchors.rightMargin: Metrics.spacingMedium
            visible: SourceControl.count === 0
            text: qsTr("No changes.")
            color: Theme.textTertiary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeBody
        }

        ListView {
            id: list

            anchors.top: header.bottom
            anchors.topMargin: Metrics.spacingMedium
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 6
            anchors.rightMargin: 6

            model: SourceControl
            clip: true
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            visible: SourceControl.count > 0

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Column {
                id: row

                required property int index
                required property string path
                required property string fileName
                required property string directory
                required property int section
                required property bool isSectionStart
                required property string statusLetter
                required property string statusLabel
                required property bool isStaged
                required property bool isConflicted

                width: list.width

                // The section heading, drawn by its first row - the same
                // approach the palette and settings use.
                Item {
                    width: parent.width
                    height: row.isSectionStart ? 30 : 0
                    visible: row.isSectionStart

                    SectionLabel {
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 4
                        text: row.isStaged ? qsTr("Staged Changes")
                                           : qsTr("Changes")
                    }

                    // Stage-all / unstage-all, on the section rather than
                    // duplicated onto every row.
                    IconButton {
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.bottom: parent.bottom
                        size: 20
                        source: row.isStaged ? Icons.minus : Icons.plus
                        tooltip: row.isStaged ? qsTr("Unstage all") : qsTr("Stage all")
                        onClicked: row.isStaged ? SourceControl.unstageAll()
                                                : SourceControl.stageAll()
                    }
                }

                Rectangle {
                    id: rowBackground

                    width: parent.width
                    height: 26
                    radius: 6
                    color: hover.hovered ? Theme.bgHover : "transparent"

                    // A HoverHandler over the whole row rather than a MouseArea's
                    // containsMouse: the click target has to sit *under* the
                    // action buttons, so its hover area would exclude them - and
                    // the buttons would vanish the moment the cursor reached one.
                    HoverHandler {
                        id: hover
                        cursorShape: Qt.PointingHandCursor
                    }

                    // Declared first so it sits beneath the buttons; a MouseArea
                    // declared after them would swallow their clicks.
                    MouseArea {
                        anchors.fill: parent
                        anchors.rightMargin: 62
                        onClicked: SourceControl.activate(row.path)
                    }

                    Row {
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.right: actions.left
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: row.fileName
                            color: row.isConflicted ? Theme.red : Theme.textSecondary
                            font.family: Fonts.ui
                            font.pointSize: Metrics.fontSizeBody
                            elide: Text.ElideMiddle
                            width: Math.min(implicitWidth, parent.width - 40)
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: row.directory.length > 0
                            text: row.directory
                            color: Theme.textTertiary
                            font.family: Fonts.ui
                            font.pointSize: Metrics.fontSizeSmall
                            elide: Text.ElideLeft
                            width: Math.min(implicitWidth,
                                            Math.max(0, parent.width - 120))
                        }
                    }

                    // Actions appear on hover, so the list reads as file names
                    // at rest rather than as rows of buttons.
                    Row {
                        id: actions

                        anchors.right: statusLetter.left
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2
                        visible: hover.hovered

                        IconButton {
                            size: 20
                            visible: !row.isStaged
                            source: Icons.discard
                            tooltip: qsTr("Discard changes")
                            onClicked: discardDialog.confirm(row.path)
                        }

                        IconButton {
                            size: 20
                            source: row.isStaged ? Icons.minus : Icons.plus
                            tooltip: row.isStaged ? qsTr("Unstage") : qsTr("Stage")
                            onClicked: row.isStaged ? SourceControl.unstage(row.path)
                                                    : SourceControl.stage(row.path)
                        }
                    }

                    Text {
                        id: statusLetter

                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: row.statusLetter
                        color: row.isConflicted ? Theme.red
                             : row.statusLetter === "U" ? Theme.green
                             : Theme.textTertiary
                        font.family: Fonts.mono
                        font.pointSize: Metrics.fontSizeSmall
                    }
                }
            }
        }
    }

    function commit() {
        if (!SourceControl.canCommit || messageInput.text.trim().length === 0) {
            return;
        }
        SourceControl.commit(messageInput.text);
        messageInput.text = "";
    }

    // Discarding cannot be undone, so it is confirmed - the one destructive
    // action in this panel.
    DiscardConfirmDialog {
        id: discardDialog
        onDiscardConfirmed: (path) => SourceControl.discard(path)
    }
}
