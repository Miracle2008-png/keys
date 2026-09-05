import QtQuick
import Keys.Ui

/// The small tracked uppercase heading the design uses above each group.
///
/// Its own component because the combination - size, tracking, capitalisation
/// and colour - has to stay identical everywhere for the hierarchy to read.
Text {
    color: Theme.textTertiary
    font.family: Fonts.ui
    font.pointSize: Metrics.fontSizeLabel
    font.capitalization: Font.AllUppercase
    font.letterSpacing: 0.6
}
