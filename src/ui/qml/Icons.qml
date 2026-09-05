pragma Singleton
import QtQuick

/// The icon set, as SVG path data.
///
/// Icons are stored as paths rather than files so they inherit the current text
/// color through a single fill/stroke property, which is what lets one icon serve
/// both themes and every state without duplicated assets. All are drawn on a
/// 24x24 grid with a 1.6-1.8 stroke weight, matching the design.
QtObject {
    readonly property string file:
        "M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z M14 2v6h6"

    readonly property string search:
        "M11 4a7 7 0 1 0 0 14 7 7 0 0 0 0-14z M21 21l-4.3-4.3"

    readonly property string git:
        "M6 3.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5z"
        + " M6 15.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5z"
        + " M18 9.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5z"
        + " M6 8.5V15.5 M8.3 6.8C12 7.2 15.5 8.8 15.8 10.2"

    readonly property string bug:
        "M11 8h2a4 4 0 0 1 4 4v3a4 4 0 0 1-4 4h-2a4 4 0 0 1-4-4v-3a4 4 0 0 1 4-4z"
        + " M9 8V6a3 3 0 0 1 6 0v2 M4 12h3 M17 12h3"
        + " M5 18l2-2 M19 18l-2-2 M5 8l2 2 M19 8l-2 2"

    readonly property string puzzle:
        "M10 3v2a2 2 0 1 1 4 0V3h4v4h-2a2 2 0 1 0 0 4h2v4h-4v-2a2 2 0 1 0-4 0v2H6v-4H4"
        + "a2 2 0 1 1 0-4h2V7H4V3z"

    readonly property string settings:
        "M12 9a3 3 0 1 0 0 6 3 3 0 0 0 0-6z"
        + " M19.4 15a1.7 1.7 0 0 0 .34 1.87l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06"
        + "a1.7 1.7 0 0 0-1.87-.34 1.7 1.7 0 0 0-1 1.56V21a2 2 0 1 1-4 0v-.09"
        + "a1.7 1.7 0 0 0-1-1.56 1.7 1.7 0 0 0-1.87.34l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06"
        + "a1.7 1.7 0 0 0 .34-1.87 1.7 1.7 0 0 0-1.56-1H3a2 2 0 1 1 0-4h.09"
        + "A1.7 1.7 0 0 0 4.65 9a1.7 1.7 0 0 0-.34-1.87l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06"
        + "a1.7 1.7 0 0 0 1.87.34H9a1.7 1.7 0 0 0 1-1.56V3a2 2 0 1 1 4 0v.09"
        + "a1.7 1.7 0 0 0 1 1.56 1.7 1.7 0 0 0 1.87-.34l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06"
        + "a1.7 1.7 0 0 0-.34 1.87V9a1.7 1.7 0 0 0 1.56 1H21a2 2 0 1 1 0 4h-.09"
        + "a1.7 1.7 0 0 0-1.56 1z"

    readonly property string sun:
        "M12 7.5a4.5 4.5 0 1 0 0 9 4.5 4.5 0 0 0 0-9z"
        + " M12 2v3 M12 19v3 M4.2 4.2l2 2 M17.8 17.8l2 2"
        + " M2 12h3 M19 12h3 M4.2 19.8l2-2 M17.8 6.2l2-2"

    readonly property string moon:
        "M21 12.6A9 9 0 1 1 11.4 3a7 7 0 0 0 9.6 9.6z"

    readonly property string chevronRight: "M9 6l6 6-6 6"
    readonly property string chevronDown: "M6 9l6 6 6-6"
    readonly property string close: "M18 6L6 18M6 6l12 12"
}
