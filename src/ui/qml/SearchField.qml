import QtQuick
import Keys.Ui

/// A single-line text field for the find bar.
///
/// Reports edits through `textEdited` rather than binding two ways: the view
/// model owns the query, and a two-way binding would fight it every time the
/// model normalised what was typed.
Rectangle {
    id: root

    property string placeholder: ""
    property alias text: input.text

    /// Drawn in the error colour, for a query that cannot compile.
    property bool invalid: false

    signal textEdited(string value)
    signal accepted()
    signal cancelled()

    height: 28
    radius: Metrics.radiusSmall
    color: Theme.bgElevated
    border.width: 1
    border.color: root.invalid ? Theme.red
                 : input.activeFocus ? Theme.focusRing
                 : Theme.border

    Behavior on border.color {
        ColorAnimation { duration: App.fastAnimationDuration }
    }

    TextInput {
        id: input

        anchors.fill: parent
        anchors.leftMargin: Metrics.spacingSmall
        anchors.rightMargin: Metrics.spacingSmall
        verticalAlignment: TextInput.AlignVCenter
        color: Theme.textPrimary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeBody
        clip: true
        selectByMouse: true
        selectionColor: Theme.selection
        selectedTextColor: Theme.textPrimary

        // Only user edits are reported. Assigning `text` from the model would
        // otherwise echo straight back and loop.
        onTextEdited: root.textEdited(text)
        onAccepted: root.accepted()

        Keys.onEscapePressed: root.cancelled()

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.placeholder
            color: Theme.textTertiary
            font: parent.font
            visible: parent.text.length === 0
        }
    }

    function selectAll() { input.selectAll(); }
    function forceActiveFocus() { input.forceActiveFocus(); }
}
