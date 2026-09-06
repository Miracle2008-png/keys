import QtQuick
import QtQuick.Controls
import Keys.Ui

/// Asks for a new name for the symbol at the caret.
///
/// Seeded with the current name and fully selected, so typing replaces it and
/// Enter commits - a rename is usually a small change to a name the user is
/// already looking at, not a fresh one typed from nothing.
KeysDialog {
    id: root

    width: 320
    title: qsTr("Rename Symbol")

    /// Emitted with the new name. Empty names are refused here rather than
    /// reaching the language server, which would answer with an error the user
    /// would have to read to learn what they already know.
    signal nameAccepted(string name)

    header: Text {
        text: root.title
        color: Theme.textPrimary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeMedium
        font.weight: Font.DemiBold
        padding: Metrics.spacingMedium
    }

    contentItem: Rectangle {
        implicitHeight: 30
        radius: Metrics.radiusSmall
        color: Theme.bgSurface
        border.width: 1
        border.color: input.activeFocus ? Theme.accent : Theme.border

        TextInput {
            id: input

            anchors.fill: parent
            anchors.leftMargin: Metrics.spacingSmall
            anchors.rightMargin: Metrics.spacingSmall
            verticalAlignment: TextInput.AlignVCenter
            color: Theme.textPrimary
            font.family: Fonts.mono
            font.pointSize: Metrics.fontSizeBody
            selectByMouse: true
            selectionColor: Theme.accentSoft
            selectedTextColor: Theme.textPrimary

            onAccepted: root.accept()
        }
    }

    footer: Row {
        spacing: Metrics.spacingSmall
        padding: Metrics.spacingMedium
        layoutDirection: Qt.RightToLeft

        DialogButton {
            text: qsTr("Rename")
            primary: true
            onClicked: root.accept()
        }

        DialogButton {
            text: qsTr("Cancel")
            onClicked: root.close()
        }
    }

    function open(currentName) {
        input.text = currentName;
        root.visible = true;
        input.forceActiveFocus();
        input.selectAll();
    }

    function accept() {
        const name = input.text.trim();
        if (name.length > 0) {
            root.nameAccepted(name);
        }
        root.close();
    }
}
