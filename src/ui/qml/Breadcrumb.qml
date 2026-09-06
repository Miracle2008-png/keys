import QtQuick
import Keys.Ui

/// The path to the open file, as clickable segments.
///
/// **Where am I.** A tab shows a filename; two files called `CMakeLists.txt`
/// look identical in the bar, and in a project with fifteen of them that is a
/// daily problem. The breadcrumb answers it without the user reaching for the
/// explorer, which is why every JetBrains IDE and VS Code carries one.
///
/// **Relative to the project.** The absolute path is mostly the user's home
/// directory repeated on every line - noise around the part that matters.
/// Segments start at the project root, which is where the user's mental model
/// starts too.
///
/// **Quiet.** It sits between the tabs and the code and must not compete with
/// either: small, dim, and brighter only under the pointer.
Item {
    id: root

    /// The editor whose document is described.
    required property var editor

    /// Emitted when a folder segment is clicked, so the explorer can reveal it.
    signal folderActivated(string path)

    readonly property var segments: {
        if (!editor || !editor.hasDocument || editor.path.length === 0) {
            return [];
        }

        const root_ = App.projectRoot;
        let relative = editor.path;
        if (root_.length > 0 && relative.startsWith(root_)) {
            relative = relative.substring(root_.length);
        }
        // Split on both separators without a regex: Qt paths use forward
        // slashes but a path that came from the platform may not, and a
        // character class holding a backslash is easy to get wrong.
        const parts = relative.split("/")
                              .map(part => part.split("\\"))
                              .reduce((all, group) => all.concat(group), [])
                              .filter(part => part.length > 0);

        // Each segment carries the path up to and including itself, so clicking
        // one can reveal exactly that folder.
        const result = [];
        let accumulated = root_;
        for (let i = 0; i < parts.length; ++i) {
            accumulated = accumulated + "/" + parts[i];
            result.push({
                name: parts[i],
                path: accumulated,
                isLast: i === parts.length - 1
            });
        }
        return result;
    }

    height: segments.length > 0 ? Metrics.breadcrumbHeight : 0
    visible: height > 0
    clip: true

    Behavior on height {
        NumberAnimation {
            duration: App.fastAnimationDuration
            easing.type: Easing.OutCubic
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bgEditor
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Metrics.spacingMedium
        anchors.right: parent.right
        anchors.rightMargin: Metrics.spacingMedium
        anchors.verticalCenter: parent.verticalCenter
        spacing: 0

        Repeater {
            model: root.segments

            delegate: Row {
                required property var modelData

                spacing: 0

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: parent.modelData.name
                    // The filename is the point; the folders leading to it are
                    // context, so only the last segment gets full weight.
                    color: parent.modelData.isLast
                           ? Theme.textSecondary
                           : (segmentMouse.containsMouse ? Theme.textSecondary
                                                         : Theme.textTertiary)
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeSmall

                    Behavior on color {
                        ColorAnimation { duration: App.fastAnimationDuration }
                    }

                    MouseArea {
                        id: segmentMouse

                        anchors.fill: parent
                        anchors.margins: -2
                        hoverEnabled: !parent.parent.modelData.isLast
                        enabled: !parent.parent.modelData.isLast
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.folderActivated(parent.parent.modelData.path)
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !parent.modelData.isLast
                    text: "  ›  "
                    color: Theme.textTertiary
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeSmall
                }
            }
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.borderFaint
    }
}
