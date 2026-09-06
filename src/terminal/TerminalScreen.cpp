#include "terminal/TerminalScreen.h"

#include <algorithm>

namespace keys::terminal {

QString TerminalLine::text() const
{
    QString result;
    for (const StyledRun& run : runs) {
        result += run.text;
    }
    return result;
}

int TerminalLine::length() const
{
    int total = 0;
    for (const StyledRun& run : runs) {
        total += static_cast<int>(run.text.size());
    }
    return total;
}

TerminalScreen::TerminalScreen()
{
    reset();
}

void TerminalScreen::setMaxScrollback(int lines)
{
    // A terminal with no history is not useful, and one with unbounded history
    // is a leak. The floor is a screenful.
    m_maxScrollback = std::max(24, lines);
}

void TerminalScreen::reset()
{
    m_lines.clear();

    // A terminal always has its visible grid, even before anything is written -
    // the cursor has to sit somewhere.
    m_lines.resize(static_cast<size_t>(m_rows));

    m_cursorRow = 0;
    m_cursorColumn = 0;
    m_cursorVisible = true;
}

void TerminalScreen::resize(int columns, int rows)
{
    m_columns = std::max(1, columns);
    m_rows = std::max(1, rows);

    // Grow the buffer if the grid no longer fits. Existing lines are kept as
    // they are: reflowing wrapped output on resize is what a full terminal does,
    // but it needs per-line "this was wrapped" tracking, and getting it wrong
    // scrambles the scrollback. Keeping lines intact is the honest simpler
    // behaviour until that is built.
    if (static_cast<int>(m_lines.size()) < m_rows) {
        m_lines.resize(static_cast<size_t>(m_rows));
    }

    m_cursorRow = std::clamp(m_cursorRow, 0, m_rows - 1);
    m_cursorColumn = std::clamp(m_cursorColumn, 0, m_columns - 1);
}

int TerminalScreen::totalLines() const
{
    return static_cast<int>(m_lines.size());
}

int TerminalScreen::viewportTop() const
{
    // The visible grid is always the last `rows` lines; everything before is
    // scrollback.
    return std::max(0, totalLines() - m_rows);
}

const TerminalLine& TerminalScreen::lineAt(int index) const
{
    static const TerminalLine empty;
    if (index < 0 || index >= totalLines()) {
        return empty;
    }
    return m_lines.at(static_cast<size_t>(index));
}

int TerminalScreen::cursorLine() const
{
    return viewportTop() + m_cursorRow;
}

TerminalLine& TerminalScreen::lineForWriting(int absoluteLine)
{
    if (absoluteLine >= totalLines()) {
        m_lines.resize(static_cast<size_t>(absoluteLine) + 1);
    }
    return m_lines.at(static_cast<size_t>(absoluteLine));
}

void TerminalScreen::replaceInLine(TerminalLine& line, int column,
                                   const QString& text, const CellStyle& style)
{
    if (text.isEmpty()) {
        return;
    }

    // Flatten to plain text plus a parallel style per character, splice, then
    // re-run. Terminal lines are at most a few hundred cells, so the copy is
    // trivial - and expressing the write as a splice rather than as run surgery
    // removes a whole class of off-by-one bugs at the run boundaries.
    QString characters;
    std::vector<CellStyle> styles;
    characters.reserve(line.length() + text.size());
    styles.reserve(static_cast<size_t>(line.length()) + text.size());

    for (const StyledRun& run : line.runs) {
        characters += run.text;
        styles.insert(styles.end(), static_cast<size_t>(run.text.size()), run.style);
    }

    // Writing past the end pads with spaces, so a program that positions the
    // cursor and writes does not leave a ragged line.
    while (characters.size() < column) {
        characters += QLatin1Char(' ');
        styles.push_back(CellStyle{});
    }

    for (int i = 0; i < text.size(); ++i) {
        const int at = column + i;
        if (at < characters.size()) {
            characters[at] = text.at(i);
            styles.at(static_cast<size_t>(at)) = style;
        } else {
            characters += text.at(i);
            styles.push_back(style);
        }
    }

    // Re-run: adjacent characters sharing a style become one run, so a line
    // written one character at a time does not accumulate hundreds of them.
    line.runs.clear();
    for (int i = 0; i < characters.size(); ++i) {
        if (!line.runs.empty() && line.runs.back().style == styles.at(static_cast<size_t>(i))) {
            line.runs.back().text += characters.at(i);
        } else {
            line.runs.push_back(StyledRun{QString(characters.at(i)),
                                          styles.at(static_cast<size_t>(i))});
        }
    }
}

void TerminalScreen::writeText(const QString& text, const CellStyle& style)
{
    if (text.isEmpty()) {
        return;
    }

    int index = 0;
    while (index < text.size()) {
        // Write as much as fits on the current line, then wrap.
        const int available = m_columns - m_cursorColumn;
        if (available <= 0) {
            lineFeed();
            carriageReturn();
            continue;
        }

        const int take = std::min(available, static_cast<int>(text.size()) - index);
        const QString chunk = text.mid(index, take);

        replaceInLine(lineForWriting(cursorLine()), m_cursorColumn, chunk, style);

        m_cursorColumn += take;
        index += take;

        // Wrapping happens on the next write rather than immediately, so a line
        // written exactly to the right edge does not leave a blank line behind.
        if (m_cursorColumn >= m_columns && index < text.size()) {
            lineFeed();
            carriageReturn();
        }
    }
}

void TerminalScreen::lineFeed()
{
    if (m_cursorRow < m_rows - 1) {
        ++m_cursorRow;
        lineForWriting(cursorLine());
        return;
    }

    // At the bottom: the grid scrolls, which means appending a line and letting
    // the viewport follow it.
    m_lines.emplace_back();
    trimScrollback();
}

void TerminalScreen::carriageReturn()
{
    m_cursorColumn = 0;
}

void TerminalScreen::setCursor(int row, int column)
{
    // Clamped rather than rejected: a program may address a cell that a resize
    // has since removed, and refusing would leave the cursor somewhere stale.
    m_cursorRow = std::clamp(row, 0, m_rows - 1);
    m_cursorColumn = std::clamp(column, 0, m_columns - 1);
}

void TerminalScreen::moveCursor(int deltaRows, int deltaColumns)
{
    setCursor(m_cursorRow + deltaRows, m_cursorColumn + deltaColumns);
}

void TerminalScreen::backspace()
{
    if (m_cursorColumn > 0) {
        --m_cursorColumn;
    }
}

void TerminalScreen::tab()
{
    // Eight-column stops, the universal default.
    constexpr int kTabWidth = 8;
    const int next = ((m_cursorColumn / kTabWidth) + 1) * kTabWidth;
    m_cursorColumn = std::min(next, m_columns - 1);
}

void TerminalScreen::eraseInLine(int mode)
{
    TerminalLine& line = lineForWriting(cursorLine());
    const QString existing = line.text();

    switch (mode) {
    case 0: {
        // To the end of the line - by far the most common, emitted after every
        // prompt redraw.
        const QString kept = existing.left(m_cursorColumn);
        line.runs.clear();
        if (!kept.isEmpty()) {
            line.runs.push_back(StyledRun{kept, CellStyle{}});
        }
        break;
    }
    case 1: {
        // To the beginning, replaced by spaces so the rest keeps its column.
        const QString kept = existing.mid(m_cursorColumn);
        line.runs.clear();
        line.runs.push_back(
            StyledRun{QString(m_cursorColumn, QLatin1Char(' ')), CellStyle{}});
        if (!kept.isEmpty()) {
            line.runs.push_back(StyledRun{kept, CellStyle{}});
        }
        break;
    }
    default:
        line.runs.clear();
        break;
    }
}

void TerminalScreen::eraseInDisplay(int mode)
{
    const int top = viewportTop();

    switch (mode) {
    case 0:
        // From the cursor to the end of the screen.
        eraseInLine(0);
        for (int i = cursorLine() + 1; i < totalLines(); ++i) {
            m_lines.at(static_cast<size_t>(i)).runs.clear();
        }
        break;

    case 1:
        // From the beginning of the screen to the cursor.
        for (int i = top; i < cursorLine(); ++i) {
            m_lines.at(static_cast<size_t>(i)).runs.clear();
        }
        eraseInLine(1);
        break;

    default:
        // The whole visible screen. Scrollback survives, because `clear` in a
        // shell should not destroy the history the user scrolled back to read.
        for (int i = top; i < totalLines(); ++i) {
            m_lines.at(static_cast<size_t>(i)).runs.clear();
        }
        m_cursorRow = 0;
        m_cursorColumn = 0;
        break;
    }
}

void TerminalScreen::trimScrollback()
{
    const int limit = m_maxScrollback + m_rows;
    if (totalLines() <= limit) {
        return;
    }

    // Erase from the front in one operation rather than one line at a time.
    const int excess = totalLines() - limit;
    m_lines.erase(m_lines.begin(), m_lines.begin() + excess);
}

} // namespace keys::terminal
