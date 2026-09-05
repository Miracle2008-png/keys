#pragma once

#include "terminal/TerminalScreen.h"

#include <QByteArray>
#include <QString>
#include <QStringDecoder>

#include <vector>

namespace keys::terminal {

/// Interprets the byte stream a shell produces and drives a TerminalScreen.
///
/// **What it implements.** The sequences that ordinary shell sessions actually
/// emit: SGR colour and attributes, cursor positioning and movement, erase in
/// line and display, and the OSC title sequence. Together these cover a prompt,
/// coloured output, a progress line that redraws itself, and the screen clearing
/// a `clear` produces.
///
/// **What it does not.** Alternate screen buffers, scrolling regions, mouse
/// reporting, and the more exotic private modes. A full-screen program like vim
/// therefore will not render correctly yet. That is a deliberate boundary rather
/// than an oversight: those features are a substantial amount of state, and the
/// integrated terminal in an IDE is overwhelmingly used for builds, git and
/// package managers. Unknown sequences are consumed and discarded rather than
/// printed as garbage, so an unsupported program produces a blank area instead
/// of corrupting the screen.
///
/// **Incremental by construction.** Output arrives in arbitrary chunks, so an
/// escape sequence can be split across two reads. The parser is a state machine
/// that keeps its partial sequence between calls, and a UTF-8 decoder that keeps
/// partial code points, so a multi-byte character split across a read boundary
/// is not corrupted.
class VtParser {
public:
    explicit VtParser(TerminalScreen& screen);

    /// Feeds bytes. May be called with any chunk size, including one byte.
    void parse(const QByteArray& data);

    /// Clears parser state, for a session reset.
    void reset();

    /// The title the shell last set through OSC 0 or 2, empty if none.
    [[nodiscard]] const QString& title() const { return m_title; }

    /// The style currently in effect, exposed for testing.
    [[nodiscard]] const CellStyle& currentStyle() const { return m_style; }

private:
    enum class State {
        Ground,        ///< ordinary text
        Escape,        ///< saw ESC
        CsiEntry,      ///< saw ESC [ - collecting parameters
        OscString,     ///< saw ESC ] - collecting a string until BEL or ST
        OscEscape,     ///< inside OSC, saw ESC - looking for the \ of ST
    };

    /// Applies a completed CSI sequence.
    void dispatchCsi(char finalByte);

    /// Applies an SGR (Select Graphic Rendition) sequence: colours and
    /// attributes.
    void applyGraphicRendition();

    /// Applies a completed OSC string.
    void dispatchOsc();

    /// Flushes pending printable text to the screen. Buffering rather than
    /// writing per character means a line of output becomes one write and one
    /// run rather than eighty.
    void flushText();

    /// The numeric parameters of the sequence being collected, with `defaultTo`
    /// substituted where a parameter was omitted.
    [[nodiscard]] int parameter(size_t index, int defaultTo) const;

    TerminalScreen& m_screen;

    State m_state = State::Ground;

    /// Printable text not yet written to the screen.
    QString m_pending;

    /// Parameters of the CSI sequence being collected.
    std::vector<int> m_parameters;

    /// True while a parameter's digits are being read, so an omitted parameter
    /// can be told from a zero.
    bool m_parameterStarted = false;

    /// Private-mode marker, such as the '?' in `ESC [ ? 25 l`.
    char m_privateMarker = 0;

    QString m_oscString;
    QString m_title;

    CellStyle m_style;

    /// Keeps partial code points between chunks, so a multi-byte character
    /// split across a read boundary is decoded correctly rather than replaced.
    QStringDecoder m_decoder;
};

} // namespace keys::terminal
