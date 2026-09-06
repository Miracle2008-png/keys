pragma Singleton
import QtQuick

/// Font families used across the interface.
///
/// Keys bundles both faces (see `resources/fonts/`) and registers them before
/// the QML engine starts, so these names resolve on every machine rather than
/// depending on what happens to be installed. The product should look the same
/// on a fresh Windows install as on a developer's machine with three hundred
/// fonts on it.
QtObject {
    /// Chrome, labels, menus, panels.
    ///
    /// Inter is drawn for interfaces at small sizes - a tall x-height and open
    /// apertures, so a 12px label stays legible where a text face would blur.
    /// It is what JetBrains' own IDEs use, which is why Keys reads as kin to
    /// them rather than as a web page in a window.
    readonly property string ui: "Inter"

    /// Code, terminal output, keycaps, paths.
    ///
    /// JetBrains Mono is designed for reading code for hours: a taller
    /// x-height than most monospaced faces, and letterforms shaped so the pairs
    /// that matter in source - `l` and `1`, `O` and `0`, `rn` and `m` - cannot
    /// be confused at a glance.
    readonly property string mono: "JetBrains Mono"

    /// Fallbacks, for `font.families` where a stack is wanted.
    ///
    /// A bundled face should always register, but if one does not, Qt walks
    /// these in order rather than substituting something arbitrary - and a
    /// wrong monospaced fallback is immediately visible in code.
    readonly property var uiStack: [ui, "Segoe UI", "SF Pro Text", "sans-serif"]
    readonly property var monoStack: [mono, "Cascadia Mono", "SF Mono",
                                      "Consolas", "monospace"]
}
