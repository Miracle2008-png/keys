import QtQuick
import QtQuick.Controls
import Keys.Ui

/// One row of the search results: either a file heading or a match under it.
///
/// One component for both kinds because they share the hover, selection and
/// activation behaviour exactly; only the content differs. Two components would
/// mean maintaining that behaviour twice.
Rectangle {
    id: root

    // Set by the delegate from the model's roles. Named apart from the role
    // names so a required property and its binding cannot shadow each other.
    property int rowIndex: 0
    property int rowKind: ProjectSearch.FileRow
    property string filePath: ""
    property string rowFileName: ""
    property string rowDirectory: ""
    property int rowMatchCount: 0
    property bool rowCollapsed: false
    property int rowLine: 0
    property string rowLineText: ""
    property int rowMatchStart: 0
    property int rowMatchLength: 0
    property bool selected: false

    signal activated()

    readonly property bool isFile: root.rowKind === ProjectSearch.FileRow

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
        anchors.right: countBadge.left
        anchors.rightMargin: Metrics.spacingSmall
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4
        visible: root.isFile

        // One glyph turned, rather than two glyphs swapped: a rotation reads as
        // one control changing state, a swap reads as a flicker.
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
            width: Math.min(implicitWidth, root.width - 140)
            visible: root.rowDirectory.length > 0
        }
    }

    Text {
        id: countBadge

        anchors.right: parent.right
        anchors.rightMargin: Metrics.spacingMedium
        anchors.verticalCenter: parent.verticalCenter
        visible: root.isFile
        text: root.rowMatchCount
        color: Theme.textTertiary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeLabel
    }

    // ---- A match ----------------------------------------------------------

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Metrics.spacingMedium + 14
        anchors.right: parent.right
        anchors.rightMargin: Metrics.spacingMedium
        anchors.verticalCenter: parent.verticalCenter
        spacing: Metrics.spacingSmall
        visible: !root.isFile

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.rowLine
            color: Theme.textTertiary
            font.family: Fonts.mono
            font.pointSize: Metrics.fontSizeLabel
            horizontalAlignment: Text.AlignRight
            width: 30
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 30 - Metrics.spacingSmall
            color: Theme.textSecondary
            font.family: Fonts.mono
            font.pointSize: Metrics.fontSizeLabel
            elide: Text.ElideRight
            textFormat: Text.StyledText

            // The matched span is marked in the accent so the eye lands on it
            // rather than on the line. Built as markup because a Text can only
            // colour a range that way - and the pieces are escaped first, since
            // source code is full of angle brackets and ampersands.
            text: {
                const line = root.rowLineText;
                const start = Math.max(0, Math.min(root.rowMatchStart, line.length));
                const end = Math.max(start, Math.min(start + root.rowMatchLength, line.length));

                // Leading indentation is dropped: in a 248px panel it would push
                // the match out of sight, and the line number already says where
                // the match is.
                const before = line.substring(0, start).replace(/^\s+/, "");
                const hit = line.substring(start, end);
                const after = line.substring(end);

                return escapeMarkup(before)
                     + "<font color=\"" + Theme.accent + "\">" + escapeMarkup(hit) + "</font>"
                     + escapeMarkup(after);
            }

            function escapeMarkup(text) {
                return text.replace(/&/g, "&amp;")
                           .replace(/</g, "&lt;")
                           .replace(/>/g, "&gt;");
            }
        }
    }

    MouseArea {
        id: mouse

        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.activated()
    }

    ToolTip.visible: mouse.containsMouse && root.isFile
    ToolTip.text: root.filePath
    ToolTip.delay: 700
}
