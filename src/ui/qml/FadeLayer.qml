import QtQuick
import Keys.Ui

/// Shows and hides its contents with a cross-fade.
///
/// Toggling `visible` swaps one panel for another in a single frame, which
/// reads as the window being replaced rather than changing. A short fade makes
/// the switch continuous - the user follows one thing becoming another instead
/// of losing their place.
///
/// `visible` still ends up false once faded out, so a hidden panel costs no
/// input handling and no hover testing. That matters here: the sidebar holds
/// five panels and only one is ever wanted.
Item {
    id: root

    /// Whether the contents should be shown. Drives the fade; `visible`
    /// follows it once the animation has run.
    property bool shown: false

    visible: opacity > 0
    opacity: shown ? 1 : 0

    Behavior on opacity {
        NumberAnimation {
            duration: App.fastAnimationDuration
            easing.type: Easing.OutCubic
        }
    }
}
