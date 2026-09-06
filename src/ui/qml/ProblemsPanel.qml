import QtQuick
import QtQuick.Controls
import Keys.Ui

/// Everything the language servers have complained about, grouped by file.
///
/// Lives in the bottom dock beside the terminal rather than in the sidebar: a
/// diagnostic message is a sentence, and a sentence in a 248px column is three
/// lines of elision.
Item {
    id: root

    // ---- Header: the counts, and the filter -------------------------------

    Rectangle {
        id: header

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
            anchors.leftMargin: Metrics.spacingMedium
            anchors.verticalCenter: parent.verticalCenter
            spacing: Metrics.spacingMedium

            // The counts describe the project, not the filtered view - so it is
            // always possible to tell that errors are being hidden.
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 5
                visible: Problems.errorCount > 0

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 7; height: 7; radius: 3.5
                    color: Theme.red
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: Problems.errorCount === 1
                          ? qsTr("1 error") : qsTr("%1 errors").arg(Problems.errorCount)
                    color: Theme.textSecondary
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeLabel
                }
            }

            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 5
                visible: Problems.warningCount > 0

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 7; height: 7; radius: 3.5
                    color: Theme.yellow
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: Problems.warningCount === 1
                          ? qsTr("1 warning")
                          : qsTr("%1 warnings").arg(Problems.warningCount)
                    color: Theme.textSecondary
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeLabel
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: Problems.errorCount === 0 && Problems.warningCount === 0
                text: Problems.hasAnalysed ? qsTr("No problems")
                                           : qsTr("Nothing analysed yet")
                color: Theme.textTertiary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeLabel
            }
        }

        FindToggle {
            anchors.right: parent.right
            anchors.rightMargin: Metrics.spacingSmall
            anchors.verticalCenter: parent.verticalCenter
            label: "!"
            tip: qsTr("Errors and warnings only")
            active: Problems.errorsAndWarningsOnly
            onToggled: Problems.errorsAndWarningsOnly = !Problems.errorsAndWarningsOnly
        }
    }

    // ---- Nothing to show --------------------------------------------------

    Text {
        anchors.centerIn: parent
        visible: Problems.fileCount === 0
        color: Theme.textTertiary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeBody
        horizontalAlignment: Text.AlignHCenter

        text: !App.hasProject
              ? qsTr("Open a project to see its problems.")
              : Problems.hasAnalysed
                ? qsTr("No problems found.")
                : qsTr("Open a file to have it analysed.")
    }

    // ---- The list ---------------------------------------------------------

    ListView {
        id: list

        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        clip: true
        model: Problems
        currentIndex: -1
        keyNavigationEnabled: true
        visible: Problems.fileCount > 0

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Keys.onReturnPressed: if (currentIndex >= 0) Problems.activate(currentIndex)
        Keys.onEnterPressed: if (currentIndex >= 0) Problems.activate(currentIndex)

        delegate: ProblemRow {
            required property int index
            required property int kind
            required property string path
            required property string fileName
            required property string directory
            required property int problemCount
            required property bool collapsed
            required property int severity
            required property string message
            required property string source
            required property int line
            required property int column
            required property int worstSeverity

            width: list.width
            rowKind: kind
            filePath: path
            rowFileName: fileName
            rowDirectory: directory
            rowProblemCount: problemCount
            rowCollapsed: collapsed
            rowSeverity: severity
            rowMessage: message
            rowSource: source
            rowLine: line
            rowColumn: column
            rowWorstSeverity: worstSeverity
            selected: list.currentIndex === index

            onActivated: {
                list.currentIndex = index;
                Problems.activate(index);
            }
        }
    }
}
