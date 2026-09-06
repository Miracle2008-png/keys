import QtQuick
import QtQuick.Controls
import Keys.Ui

/// One row of the problems list: a file heading, or a diagnostic under it.
Rectangle {
    id: root

    property int rowKind: Problems.FileRow
    property string filePath: ""
    property string rowFileName: ""
    property string rowDirectory: ""
    property int rowProblemCount: 0
    property bool rowCollapsed: false
    property int rowSeverity: Problems.ErrorSeverity
    property string rowMessage: ""
    property string rowSource: ""
    property int rowLine: 0
    property int rowColumn: 0
    property int rowWorstSeverity: Problems.ErrorSeverity
    property bool selected: false

    signal activated()

    readonly property bool isFile: root.rowKind === Problems.FileRow

    /// One colour per severity, so the row's status is readable before its text
    /// is. Hints share the tertiary text colour rather than getting a hue of
    /// their own - a hint is not a status, it is a suggestion.
    function colorForSeverity(severity) {
        switch (severity) {
        case Problems.ErrorSeverity:   return Theme.red;
        case Problems.WarningSeverity: return Theme.yellow;
        case Problems.InfoSeverity:    return Theme.accent;
        default:                       return Theme.textTertiary;
        }
    }

    height: Metrics.rowHeight
    color: root.selected ? Theme.selection
         : mouse.containsMouse ? Theme.bgHover
         : "transparent"

    Behavior on color {
        ColorAnimation { duration: App.fastAnimationDuration }
    }

    // ---- A file heading ---------------------------------------------------

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Metrics.spacingMedium
        anchors.right: countLabel.left
        anchors.rightMargin: Metrics.spacingSmall
        anchors.verticalCenter: parent.verticalCenter
        spacing: 5
        visible: root.isFile

        Icon {
            anchors.verticalCenter: parent.verticalCenter
            source: Icons.chevronDown
            size: 12
            color: Theme.textTertiary

            rotation: root.rowCollapsed ? -90 : 0
            Behavior on rotation {
                NumberAnimation {
                    duration: App.fastAnimationDuration
                    easing.type: Easing.OutCubic
                }
            }
        }

        // The file's worst severity, so a folded file still says how bad it is.
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 7; height: 7; radius: 3.5
            color: root.colorForSeverity(root.rowWorstSeverity)
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.rowFileName
            color: Theme.textPrimary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeBody
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.rowDirectory
            color: Theme.textTertiary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeLabel
            elide: Text.ElideLeft
            width: Math.min(implicitWidth, root.width * 0.35)
            visible: root.rowDirectory.length > 0
        }
    }

    Text {
        id: countLabel

        anchors.right: parent.right
        anchors.rightMargin: Metrics.spacingMedium
        anchors.verticalCenter: parent.verticalCenter
        visible: root.isFile
        text: root.rowProblemCount
        color: Theme.textTertiary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeLabel
    }

    // ---- A problem --------------------------------------------------------

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Metrics.spacingMedium + 17
        anchors.right: parent.right
        anchors.rightMargin: Metrics.spacingMedium
        anchors.verticalCenter: parent.verticalCenter
        spacing: Metrics.spacingSmall
        visible: !root.isFile

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 6; height: 6; radius: 3
            color: root.colorForSeverity(root.rowSeverity)
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.rowLine + ":" + root.rowColumn
            color: Theme.textTertiary
            font.family: Fonts.mono
            font.pointSize: Metrics.fontSizeLabel
            horizontalAlignment: Text.AlignRight
            width: 52
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 6 - 52 - sourceLabel.width
                   - Metrics.spacingSmall * 3
            text: root.rowMessage
            color: Theme.textSecondary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeLabel
            elide: Text.ElideRight
        }

        // Which tool said it. Quiet, but a message from clang-tidy and one from
        // the compiler are answered differently.
        Text {
            id: sourceLabel

            anchors.verticalCenter: parent.verticalCenter
            text: root.rowSource
            color: Theme.textTertiary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeLabel
            visible: root.rowSource.length > 0
        }
    }

    MouseArea {
        id: mouse

        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.activated()
    }

    ToolTip.visible: mouse.containsMouse && !root.isFile && root.rowMessage.length > 0
    ToolTip.text: root.rowMessage
    ToolTip.delay: 700
}
