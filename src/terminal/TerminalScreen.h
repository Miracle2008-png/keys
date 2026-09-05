#pragma once

#include <QString>

#include <vector>

namespace keys::terminal {

/// How one run of characters is drawn.
///
/// Colours are stored as palette indices rather than QColor so the screen model
/// stays free of the theme: the UI resolves an index against the active theme
/// when it draws, and a theme change repaints without the screen being touched.
/// -1 means "the default", which is what most output uses.
struct CellStyle {
    int foreground = -1;
    int background = -1;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool inverse = false;

    [[nodiscard]] bool operator==(const CellStyle& other) const = default;
};

/// A run of characters sharing one style. Lines are stored as runs rather than
/// per-character styles because terminal output is overwhelmingly uniform — a
/// whole line of plain text is one run, and a coloured diff line is three or
/// four. Per-cell styling would multiply memory by the size of CellStyle for no
/// gain.
struct StyledRun {
    QString text;
    CellStyle style;
};

/// One line of the terminal.
struct TerminalLine {
    std::vector<StyledRun> runs;

    /// The line's text with styling discarded, for selection and searching.
    [[nodiscard]] QString text() const;

    [[nodiscard]] int length() const;
};

/// The terminal's grid and scrollback.
///
/// Holds what the shell has drawn: a fixed-size visible grid plus a bounded
/// history of lines that have scrolled off the top. It knows nothing about
/// escape sequences — VtParser interprets those and drives this through a small
/// set of operations, which keeps the parsing testable separately from the
/// screen state it produces.
class TerminalScreen {
public:
    TerminalScreen();

    void resize(int columns, int rows);

    [[nodiscard]] int columns() const { return m_columns; }
    [[nodiscard]] int rows() const { return m_rows; }

    /// Total lines available, scrollback included. This is what a view scrolls
    /// through.
    [[nodiscard]] int totalLines() const;

    /// A line by absolute index, counting scrollback from zero.
    [[nodiscard]] const TerminalLine& lineAt(int index) const;

    /// Where the cursor is, in absolute line coordinates.
    [[nodiscard]] int cursorLine() const;

    /// The cursor's row within the visible grid, which is what terminal
    /// sequences address.
    [[nodiscard]] int cursorRow() const { return m_cursorRow; }
    [[nodiscard]] int cursorColumn() const { return m_cursorColumn; }
    [[nodiscard]] bool cursorVisible() const { return m_cursorVisible; }

    // ---- Operations the parser drives -------------------------------------

    /// Writes text at the cursor, overwriting what is there and wrapping at the
    /// right edge.
    void writeText(const QString& text, const CellStyle& style);

    /// Moves to the start of the next line, scrolling if at the bottom.
    void lineFeed();
    void carriageReturn();

    /// Moves the cursor. Coordinates are relative to the visible grid and are
    /// clamped to it, because a program may address a cell that a resize has
    /// since removed.
    void setCursor(int row, int column);
    void moveCursor(int deltaRows, int deltaColumns);

    void setCursorVisible(bool visible) { m_cursorVisible = visible; }

    /// Erases part of the display. `mode` follows the ED sequence: 0 to the end
    /// of the screen, 1 to the beginning, 2 the whole visible screen.
    void eraseInDisplay(int mode);

    /// Erases part of the current line, following EL: 0 to the end, 1 to the
    /// beginning, 2 the whole line.
    void eraseInLine(int mode);

    /// Removes one character to the left, as a shell's own backspace does.
    void backspace();

    /// Advances to the next tab stop, every eight columns.
    void tab();

    /// Clears everything including scrollback.
    void reset();

    /// How many lines of history to keep. Bounded because a build's output is
    /// unbounded, and an editor that grows without limit while a test suite runs
    /// is one that eventually stops responding.
    static constexpr int kMaxScrollback = 5000;

private:
    /// The visible grid's first line, as an index into m_lines.
    [[nodiscard]] int viewportTop() const;

    /// Ensures a line exists at an absolute index, extending the grid if needed.
    TerminalLine& lineForWriting(int absoluteLine);

    /// Splits a line's runs so a write at `column` can replace exactly the cells
    /// it covers.
    void replaceInLine(TerminalLine& line, int column, const QString& text,
                       const CellStyle& style);

    /// Drops the oldest history once the limit is exceeded.
    void trimScrollback();

    std::vector<TerminalLine> m_lines;

    int m_columns = 80;
    int m_rows = 24;

    /// Cursor position within the visible grid.
    int m_cursorRow = 0;
    int m_cursorColumn = 0;

    bool m_cursorVisible = true;
};

} // namespace keys::terminal
