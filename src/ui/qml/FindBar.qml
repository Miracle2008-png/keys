import QtQuick
import QtQuick.Controls
import Keys.Ui

/// Find and replace within the open file.
///
/// **A bar, not a dialog.** A modal find covers the text you are searching and
/// has to be dismissed before you can act on a result. This sits at the top of
/// the editor, leaves the document visible and editable, and closes on Escape.
///
/// **Counted, not merely stepped.** "3 of 47" is most of what makes a find
/// useful: it tells you whether the term is rare, whether you have been round
/// the file, and whether the query was a typo. A find that could only move to
/// the next hit could never say.
///
/// **Replace is the same bar.** Finding and replacing are one task, so the
/// replace row expands beneath the find row rather than opening a second thing
/// to manage.
Rectangle {
    id: root

    /// The editor being searched.
    required property var editor

    readonly property bool open: editor && editor.findOpen

    /// Kept here rather than on the view model: a replacement is a draft the
    /// user is composing, not state the document needs to know about until
    /// they act on it.
    property string replacement: ""

    height: open ? (editor.replaceOpen ? 76 : 40) : 0
    visible: height > 0
    clip: true
    color: Theme.bgSurface

    Behavior on height {
        NumberAnimation {
            duration: App.fastAnimationDuration
            easing.type: Easing.OutCubic
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }

    /// Puts the caret in the find field. Called when the bar opens, so the user
    /// can type immediately rather than clicking first.
    function takeFocus() {
        // Seeded once on open - from a selection, if openFind() took one - and
        // owned by the field from then on.
        queryField.text = root.editor ? root.editor.findQuery : "";
        queryField.forceActiveFocus();
        queryField.selectAll();
    }

    // ---- Find row ----------------------------------------------------------

    Row {
        id: findRow

        anchors.left: parent.left
        anchors.leftMargin: Metrics.spacingMedium
        anchors.right: parent.right
        anchors.rightMargin: Metrics.spacingMedium
        anchors.top: parent.top
        height: 40
        spacing: Metrics.spacingSmall

        SearchField {
            id: queryField

            anchors.verticalCenter: parent.verticalCenter
            width: 260
            placeholder: qsTr("Find")

            // Deliberately not bound to findQuery. A binding here is overwritten
            // by the model on every keystroke - the field reports the edit, the
            // model notifies, and the binding writes the old value straight back
            // over what was just typed, so the field stays empty. The bar seeds
            // it in takeFocus() instead and the model follows from there.
            // A malformed regex is shown rather than silently matching nothing.
            invalid: root.editor && !root.editor.findQueryValid

            onTextEdited: (value) => root.editor.setFindQuery(value)
            onAccepted: root.editor.findNext()
            onCancelled: root.close()
        }

        // "3 of 47", or nothing at all when there is no query to count.
        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: 68
            text: {
                if (!root.editor || root.editor.findQuery.length === 0) {
                    return "";
                }
                if (!root.editor.findQueryValid) {
                    return qsTr("Bad pattern");
                }
                if (root.editor.findCount === 0) {
                    return qsTr("No results");
                }
                return qsTr("%1 of %2").arg(root.editor.findCurrent)
                                       .arg(root.editor.findCount);
            }
            color: root.editor && !root.editor.findQueryValid ? Theme.red
                                                              : Theme.textTertiary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeLabel
            elide: Text.ElideRight
        }

        FindToggle {
            anchors.verticalCenter: parent.verticalCenter
            label: "Aa"
            tip: qsTr("Match case")
            active: root.editor && root.editor.findCaseSensitive
            onToggled: root.editor.setFindCaseSensitive(!root.editor.findCaseSensitive)
        }

        FindToggle {
            anchors.verticalCenter: parent.verticalCenter
            label: "␣"
            tip: qsTr("Whole word")
            active: root.editor && root.editor.findWholeWord
            onToggled: root.editor.setFindWholeWord(!root.editor.findWholeWord)
        }

        FindToggle {
            anchors.verticalCenter: parent.verticalCenter
            label: ".*"
            tip: qsTr("Regular expression")
            active: root.editor && root.editor.findRegex
            onToggled: root.editor.setFindRegex(!root.editor.findRegex)
        }

        FindToggle {
            anchors.verticalCenter: parent.verticalCenter
            label: "↑"
            tip: qsTr("Previous match")
            enabled: root.editor && root.editor.findCount > 0
            onToggled: root.editor.findPrevious()
        }

        FindToggle {
            anchors.verticalCenter: parent.verticalCenter
            label: "↓"
            tip: qsTr("Next match")
            enabled: root.editor && root.editor.findCount > 0
            onToggled: root.editor.findNext()
        }
    }

    // The close button sits at the far right, away from the toggles so it is
    // not hit by accident while cycling through matches.
    FindToggle {
        anchors.right: parent.right
        anchors.rightMargin: Metrics.spacingMedium
        anchors.top: parent.top
        anchors.topMargin: 8
        label: "×"
        tip: qsTr("Close")
        onToggled: root.close()
    }

    // ---- Replace row -------------------------------------------------------

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Metrics.spacingMedium
        anchors.top: findRow.bottom
        height: 36
        spacing: Metrics.spacingSmall
        visible: root.editor && root.editor.replaceOpen

        SearchField {
            id: replaceField

            anchors.verticalCenter: parent.verticalCenter
            width: 260
            placeholder: qsTr("Replace")

            onTextEdited: (value) => root.replacement = value
            onAccepted: root.replaceOne()
            onCancelled: root.close()
        }

        WelcomeButton {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Replace")
            onClicked: root.replaceOne()
        }

        WelcomeButton {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Replace All")
            onClicked: root.replaceEverything()
        }
    }

    // ---- Actions -----------------------------------------------------------

    function close() {
        root.editor.closeFind();
        editorFocusRequested();
    }

    function replaceOne() {
        if (root.editor.findCount > 0) {
            root.editor.replaceCurrent(root.replacement);
        }
    }

    function replaceEverything() {
        if (root.editor.findCount > 0) {
            root.editor.replaceAll(root.replacement);
        }
    }

    /// Asks the pane to put the caret back in the document - closing the bar
    /// should leave the user typing where they were, not nowhere.
    signal editorFocusRequested()
}
