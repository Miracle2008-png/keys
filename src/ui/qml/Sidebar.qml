import QtQuick
import Keys.Ui

/// The sidebar: a header and the panel for the active view, with a drag handle
/// on its trailing edge.
///
/// Collapsing animates width to zero rather than toggling visibility, so the
/// editor grows into the space smoothly instead of jumping. The animation is
/// interruptible - clicking twice quickly reverses mid-flight rather than queuing.
Item {
    id: root

    /// Collapsed with no project open. Every panel the sidebar can show needs a
    /// project, so without one it is an empty frame taking a third of the window
    /// away from the welcome screen - which is the one thing the user can act on
    /// at that moment.
    readonly property bool expanded: App.sidebarVisible && App.hasProject

    /// True while the user is dragging the resize handle. Animation is suppressed
    /// during a drag: easing toward a target that moves every frame lags behind
    /// the cursor, which reads as the window fighting the user.
    property bool resizing: false

    width: expanded ? App.sidebarWidth : 0
    clip: true

    Behavior on width {
        enabled: !root.resizing
        NumberAnimation {
            duration: App.animationDuration
            easing.type: Easing.OutCubic
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bgSurface

        // Not rendered at all once the sidebar has finished collapsing.
        //
        // The Column below is held at the sidebar's full width on purpose, so
        // its content does not reflow while the panel animates - which means it
        // overflows a collapsed sidebar by its entire width. `clip` does not
        // contain it: a clip rectangle of zero width is degenerate, so Qt skips
        // the clip and the content paints straight over the editor. That was
        // the black band covering the workbench whenever no project was open.
        //
        // Tied to width rather than to `expanded` so the content stays visible
        // for the whole collapse animation and disappears only once there is no
        // room left for it.
        visible: root.width > 0
        clip: true

        Rectangle {
            anchors.right: parent.right
            width: 1
            height: parent.height
            color: Theme.border
        }

        // Anchored to the sidebar's full width rather than the animating parent,
        // so content does not reflow during the collapse animation.
        Column {
            width: App.sidebarWidth
            height: parent.height
            spacing: 0

            Text {
                x: Metrics.spacingMedium
                topPadding: 12
                bottomPadding: Metrics.spacingSmall
                text: root.viewTitle
                color: Theme.textTertiary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeLabel
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 0.5
            }

            // The panel for the active view. Views whose milestone has not
            // landed say so plainly rather than showing a mock.
            Item {
                width: parent.width
                height: parent.height - Metrics.spacingLarge - Metrics.spacingSmall

                ExplorerPanel {
                    anchors.fill: parent
                    visible: App.activeView === "explorer" && App.hasProject
                }

                SourceControlPanel {
                    anchors.fill: parent
                    visible: App.activeView === "sourceControl" && App.hasProject
                }

                RunPanel {
                    anchors.fill: parent
                    visible: App.activeView === "debug" && App.hasProject
                }

                ExtensionsPanel {
                    anchors.fill: parent
                    visible: App.activeView === "extensions" && App.hasProject
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: Metrics.spacingMedium
                    anchors.right: parent.right
                    anchors.rightMargin: Metrics.spacingMedium
                    anchors.top: parent.top
                    anchors.topMargin: Metrics.spacingSmall
                    visible: !App.hasProject
                    text: qsTr("No project is open.")
                    color: Theme.textTertiary
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeBody
                    wrapMode: Text.WordWrap
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: Metrics.spacingMedium
                    anchors.right: parent.right
                    anchors.rightMargin: Metrics.spacingMedium
                    anchors.top: parent.top
                    anchors.topMargin: Metrics.spacingSmall
                    visible: App.hasProject
                             && App.activeView !== "explorer"
                             && App.activeView !== "sourceControl"
                             && App.activeView !== "debug"
                             && App.activeView !== "extensions"
                    text: qsTr("Not implemented yet.")
                    color: Theme.textTertiary
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeBody
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    readonly property string viewTitle: {
        switch (App.activeView) {
        case "search":        return qsTr("Search");
        case "sourceControl": return qsTr("Source Control");
        case "debug":         return qsTr("Run and Debug");
        case "extensions":    return qsTr("Extensions");
        default:              return qsTr("Explorer");
        }
    }

    // Resize handle. Sits over the border, wider than the line it straddles so it
    // is comfortable to grab without a visible seam.
    MouseArea {
        anchors.right: parent.right
        anchors.rightMargin: -2
        width: 6
        height: parent.height
        enabled: root.expanded
        cursorShape: Qt.SizeHorCursor
        acceptedButtons: Qt.LeftButton

        property int pressWidth: 0
        property real pressX: 0

        onPressed: (mouse) => {
            pressWidth = App.sidebarWidth;
            pressX = mapToItem(null, mouse.x, mouse.y).x;
            root.resizing = true;
        }

        onPositionChanged: (mouse) => {
            if (!pressed)
                return;
            const dx = mapToItem(null, mouse.x, mouse.y).x - pressX;
            // The controller clamps to the design's min/max, so the sidebar
            // cannot be dragged to an unusable width.
            App.setSidebarWidth(pressWidth + dx);
        }

        onReleased: root.resizing = false
        onCanceled: root.resizing = false
    }
}
