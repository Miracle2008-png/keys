import QtQuick
import QtQuick.Controls
import Keys.Ui

/// Project-wide search: a query, its options, and the matches grouped by file.
///
/// **The options are hidden until asked for.** Case, whole word and regular
/// expression are three toggles most searches never touch, and the glob fields
/// are two more. Shown always, they crowd out the results in a 248px sidebar;
/// behind a disclosure, the common case is a field and a list.
Item {
    id: root

    /// Focuses the field and selects what is in it, so the panel is ready to
    /// type into when it is opened by shortcut rather than by clicking.
    function takeFocus() {
        field.forceActiveFocus();
        field.selectAll();
    }

    // ---- No project -------------------------------------------------------

    Text {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Metrics.spacingMedium
        visible: !App.hasProject
        text: qsTr("Open a project to search it.")
        color: Theme.textTertiary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeBody
        wrapMode: Text.WordWrap
    }

    Item {
        anchors.fill: parent
        visible: App.hasProject

        // ---- Query and options --------------------------------------------

        Column {
            id: header

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Metrics.spacingMedium
            anchors.rightMargin: Metrics.spacingMedium
            anchors.topMargin: Metrics.spacingSmall
            spacing: Metrics.spacingSmall

            Row {
                width: parent.width
                spacing: 4

                SearchField {
                    id: field

                    width: parent.width - optionsToggle.width - 4
                    placeholder: qsTr("Search in project")
                    invalid: ProjectSearch.patternError.length > 0

                    onTextEdited: ProjectSearch.query = text
                    onAccepted: ProjectSearch.searchNow()

                    Keys.onDownPressed: {
                        // Straight from the field into the results, so a search
                        // can be driven without reaching for the mouse.
                        if (results.count > 0) {
                            results.currentIndex = 0;
                            results.forceActiveFocus();
                        }
                    }
                }

                FindToggle {
                    id: optionsToggle

                    anchors.verticalCenter: parent.verticalCenter
                    label: "⚙"
                    tip: qsTr("Search options")
                    active: root.optionsOpen
                    onToggled: root.optionsOpen = !root.optionsOpen
                }
            }

            // The options, revealed rather than always present. Height is
            // animated so the results slide down instead of jumping.
            Item {
                width: parent.width
                height: root.optionsOpen ? options.implicitHeight : 0
                clip: true
                opacity: root.optionsOpen ? 1 : 0

                Behavior on height {
                    NumberAnimation {
                        duration: App.animationDuration
                        easing.type: Easing.OutCubic
                    }
                }
                Behavior on opacity {
                    NumberAnimation {
                        duration: App.fastAnimationDuration
                        easing.type: Easing.OutQuad
                    }
                }

                Column {
                    id: options

                    width: parent.width
                    spacing: Metrics.spacingSmall

                    Row {
                        spacing: 4

                        FindToggle {
                            label: "Aa"
                            tip: qsTr("Match case")
                            active: ProjectSearch.caseSensitive
                            onToggled: ProjectSearch.caseSensitive = !ProjectSearch.caseSensitive
                        }

                        FindToggle {
                            label: "␣"
                            tip: qsTr("Whole word")
                            active: ProjectSearch.wholeWord
                            onToggled: ProjectSearch.wholeWord = !ProjectSearch.wholeWord
                        }

                        FindToggle {
                            label: ".*"
                            tip: qsTr("Regular expression")
                            active: ProjectSearch.regularExpression
                            onToggled: ProjectSearch.regularExpression = !ProjectSearch.regularExpression
                        }
                    }

                    SearchField {
                        width: parent.width
                        placeholder: qsTr("Include: *.cpp, src/**")
                        onTextEdited: ProjectSearch.includeGlobs = text
                    }

                    SearchField {
                        width: parent.width
                        placeholder: qsTr("Exclude: build/**")
                        onTextEdited: ProjectSearch.excludeGlobs = text
                    }
                }
            }

            // ---- What the search is doing ---------------------------------

            Text {
                width: parent.width
                visible: text.length > 0
                color: ProjectSearch.patternError.length > 0
                           ? Theme.red : Theme.textTertiary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeLabel
                wrapMode: Text.WordWrap
                elide: Text.ElideRight

                text: {
                    if (ProjectSearch.patternError.length > 0) {
                        return ProjectSearch.patternError;
                    }
                    if (ProjectSearch.searching) {
                        return qsTr("Searching…");
                    }
                    if (!ProjectSearch.hasSearched) {
                        return "";
                    }
                    if (ProjectSearch.matchCount === 0) {
                        return qsTr("No matches");
                    }

                    const matches = ProjectSearch.matchCount === 1
                                  ? qsTr("1 match") : qsTr("%1 matches").arg(ProjectSearch.matchCount);
                    const files = ProjectSearch.fileCount === 1
                                ? qsTr("1 file") : qsTr("%1 files").arg(ProjectSearch.fileCount);

                    // Says so when the list is capped, rather than presenting a
                    // truncated list as if it were everything.
                    return ProjectSearch.truncated
                         ? qsTr("%1 in %2, showing the first found").arg(matches).arg(files)
                         : qsTr("%1 in %2").arg(matches).arg(files);
                }
            }
        }

        // ---- Results ------------------------------------------------------

        ListView {
            id: results

            anchors.top: header.bottom
            anchors.topMargin: Metrics.spacingSmall
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            clip: true
            model: ProjectSearch
            currentIndex: -1
            keyNavigationEnabled: true

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Keys.onReturnPressed: if (currentIndex >= 0) ProjectSearch.activate(currentIndex)
            Keys.onEnterPressed: if (currentIndex >= 0) ProjectSearch.activate(currentIndex)

            delegate: SearchResultRow {
                required property int index
                required property int kind
                required property string path
                required property string fileName
                required property string directory
                required property int matchCount
                required property bool collapsed
                required property int line
                required property string lineText
                required property int matchStart
                required property int matchLength

                width: results.width
                rowIndex: index
                rowKind: kind
                filePath: path
                rowFileName: fileName
                rowDirectory: directory
                rowMatchCount: matchCount
                rowCollapsed: collapsed
                rowLine: line
                rowLineText: lineText
                rowMatchStart: matchStart
                rowMatchLength: matchLength
                selected: results.currentIndex === index

                onActivated: {
                    results.currentIndex = index;
                    ProjectSearch.activate(index);
                }
            }
        }
    }

    /// Whether the option row and glob fields are showing.
    property bool optionsOpen: false
}
