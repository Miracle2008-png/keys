pragma Singleton
import QtQuick

/// Font families used across the interface.
///
/// The design specifies a system UI face for chrome and JetBrains Mono for code
/// and shortcut keycaps. Each falls back through a stack so the app looks correct
/// before any bundled font loads, and on systems where it is missing entirely.
QtObject {
    /// Chrome, labels, menus. The platform's own UI face keeps Keys feeling
    /// native rather than like a ported web app.
    readonly property string ui: {
        switch (Qt.platform.os) {
        case "windows": return "Segoe UI";
        case "osx":     return "SF Pro Text";
        default:        return "Inter";
        }
    }

    /// Code, terminal output, keycaps.
    readonly property string mono: {
        switch (Qt.platform.os) {
        case "windows": return "Cascadia Mono";
        case "osx":     return "SF Mono";
        default:        return "JetBrains Mono";
        }
    }
}
