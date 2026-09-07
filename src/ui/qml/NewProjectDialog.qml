import QtQuick
import Keys.Ui

/// Creates a project folder and opens it.
///
/// **It says where the folder will go.** The dialog this replaces asked for a
/// name and nothing else, so from the welcome screen - with no project open -
/// the folder resolved against an empty root and went nowhere. A name on its
/// own is not enough information to create anything.
///
/// **The location is editable and browsable.** Defaults to the user's documents
/// folder, which is somewhere they can certainly write, rather than whatever
/// directory the application happened to be launched from.
KeysDialog {
    id: root

    property string location: ""

    /// The full path as it will be created, shown under the fields so there is
    /// never a question about what the two inputs add up to.
    readonly property string fullPath:
        location.length === 0 || nameField.text.length === 0
            ? ""
            : location + "/" + nameField.text

    signal projectCreated(string path)

    width: 520
    title: qsTr("New Project")

    header: Text {
        text: root.title
        color: Theme.textPrimary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeLarge
        font.weight: Font.Medium
        padding: Metrics.spacingMedium
    }

    contentItem: Column {
        spacing: Metrics.spacingMedium

        Column {
            width: parent.width
            spacing: 4

            Text {
                text: qsTr("Name")
                color: Theme.textSecondary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeLabel
            }

            SearchField {
                id: nameField

                width: parent.width
                placeholder: qsTr("my-project")

                // Enter creates, which is what someone types a name and expects.
                onAccepted: root.create()
                onCancelled: root.reject()
            }
        }

        Column {
            width: parent.width
            spacing: 4

            Text {
                text: qsTr("Location")
                color: Theme.textSecondary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeLabel
            }

            Row {
                width: parent.width
                spacing: Metrics.spacingSmall

                SearchField {
                    id: locationField

                    width: parent.width - browseButton.width - Metrics.spacingSmall
                    text: root.location
                    onTextEdited: root.location = text
                }

                DialogButton {
                    id: browseButton

                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Browse")
                    onClicked: locationPicker.open()
                }
            }
        }

        // What the two fields add up to. Cheap to draw and it removes the whole
        // class of "where did my folder go".
        Text {
            width: parent.width
            visible: root.fullPath.length > 0
            text: qsTr("Creates %1").arg(root.fullPath)
            color: Theme.textTertiary
            font.family: Fonts.mono
            font.pointSize: Metrics.fontSizeLabel
            elide: Text.ElideMiddle
        }
    }

    FolderPicker {
        id: locationPicker

        title: qsTr("Where should the project go?")
        onFolderAccepted: (path) => {
            root.location = path;
            locationField.text = path;
        }
    }

    function create() {
        const created = App.createProject(root.location, nameField.text);
        if (created.length > 0) {
            root.projectCreated(created);
            root.close();
        }
        // On failure the controller reports through errorOccurred and the
        // dialog stays open with what was typed still in it.
    }

    // A themed footer rather than standardButtons, for the reason NameDialog
    // records: Basic draws two identical grey blocks, giving the confirming
    // action no more weight than cancelling.
    footer: Item {
        implicitHeight: 56

        Row {
            anchors.right: parent.right
            anchors.rightMargin: Metrics.spacingMedium
            anchors.verticalCenter: parent.verticalCenter
            spacing: Metrics.spacingSmall

            DialogButton {
                text: qsTr("Cancel")
                onClicked: root.reject()
            }

            DialogButton {
                text: qsTr("Create")
                primary: true
                enabled: nameField.text.trim().length > 0
                         && root.location.length > 0
                opacity: enabled ? 1.0 : 0.4
                onClicked: root.accept()
            }
        }
    }

    /// Named `start` rather than `open`, which Dialog already defines - and
    /// shadowing that stops the dialog opening at all.
    function start() {
        root.location = App.defaultProjectLocation();
        locationField.text = root.location;
        nameField.text = "";
        root.open();
    }

    onOpened: nameField.forceActiveFocus()
    onAccepted: root.create()
}
