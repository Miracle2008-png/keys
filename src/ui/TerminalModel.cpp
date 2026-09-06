#include "ui/TerminalModel.h"

#include "core/Log.h"
#include "terminal/TerminalScreen.h"
#include "terminal/TerminalSession.h"
#include "ui/Theme.h"

#include <QClipboard>
#include <QColor>
#include <QGuiApplication>
#include <QQmlEngine>
#include <Qt>

namespace keys::ui {
namespace {

TerminalModel* g_instance = nullptr;

/// Escapes the four characters that would otherwise be read as markup.
///
/// Spaces need care as well: StyledText collapses runs of them the way HTML
/// does, and a terminal is nothing but aligned columns of spaces. Every space
/// becomes a non-breaking space, which is the same fix the editor needed.
QString escapeForDisplay(const QString& text)
{
    QString escaped;
    escaped.reserve(text.size() + 16);

    for (const QChar character : text) {
        switch (character.unicode()) {
        case u'&':  escaped += QLatin1String("&amp;");  break;
        case u'<':  escaped += QLatin1String("&lt;");   break;
        case u'>':  escaped += QLatin1String("&gt;");   break;
        case u'"':  escaped += QLatin1String("&quot;"); break;
        case u' ':  escaped += QLatin1String("&nbsp;"); break;
        default:    escaped += character;               break;
        }
    }
    return escaped;
}

} // namespace

/// One terminal and the shell behind it.
struct TerminalModel::Session {
    std::unique_ptr<terminal::TerminalSession> session;
    QString title;
};

TerminalModel::TerminalModel(const Theme& theme, QObject* parent)
    : QAbstractListModel(parent), m_theme(theme)
{
    // A theme change re-resolves every palette index, so the cached markup has
    // to be rebuilt - otherwise the terminal keeps yesterday's colours.
    connect(&m_theme, &Theme::changed, this, &TerminalModel::refresh);
}

TerminalModel::~TerminalModel() = default;

int TerminalModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_lines.size());
}

QVariant TerminalModel::data(const QModelIndex& index, int role) const
{
    const auto row = static_cast<size_t>(index.row());
    if (index.row() < 0 || row >= m_lines.size()) {
        return {};
    }

    switch (role) {
    case MarkupRole:
        return m_lines.at(row);
    case PlainTextRole:
        return m_plain.at(row);
    default:
        return {};
    }
}

QHash<int, QByteArray> TerminalModel::roleNames() const
{
    return {
        {MarkupRole, "markup"},
        {PlainTextRole, "plainText"},
    };
}

void TerminalModel::setCurrentSession(int index)
{
    if (index == m_current || index < 0 || index >= sessionCount()) {
        return;
    }
    m_current = index;
    refresh();
    emit sessionsChanged();
}

QString TerminalModel::title() const
{
    return titleAt(m_current);
}

QString TerminalModel::titleAt(int index) const
{
    if (index < 0 || index >= sessionCount()) {
        return {};
    }
    const Session& entry = *m_sessions.at(static_cast<size_t>(index));
    return entry.session->title();
}

bool TerminalModel::isRunning() const
{
    if (m_current < 0 || m_current >= sessionCount()) {
        return false;
    }
    return m_sessions.at(static_cast<size_t>(m_current))->session->isRunning();
}

int TerminalModel::cursorLine() const
{
    if (m_current < 0 || m_current >= sessionCount()) {
        return 0;
    }
    return m_sessions.at(static_cast<size_t>(m_current))->session->screen().cursorLine();
}

int TerminalModel::cursorColumn() const
{
    if (m_current < 0 || m_current >= sessionCount()) {
        return 0;
    }
    return m_sessions.at(static_cast<size_t>(m_current))->session->screen().cursorColumn();
}

bool TerminalModel::cursorVisible() const
{
    if (m_current < 0 || m_current >= sessionCount()) {
        return false;
    }
    return m_sessions.at(static_cast<size_t>(m_current))->session->screen().cursorVisible();
}

void TerminalModel::setWorkingDirectory(const QString& path)
{
    m_workingDirectory = path;
}

void TerminalModel::openSession()
{
    if (m_workingDirectory.isEmpty()) {
        emit errorOccurred(tr("Open a project before starting a terminal."));
        return;
    }

    auto entry = std::make_unique<Session>();
    entry->session = std::make_unique<terminal::TerminalSession>();

    terminal::TerminalSession* session = entry->session.get();

    // Only the visible session repaints the view. A build scrolling in a hidden
    // tab still runs and still fills its screen; rebuilding markup nobody is
    // looking at would spend the frame budget on it.
    connect(session, &terminal::TerminalSession::screenChanged, this, [this, session] {
        if (m_current >= 0 && m_current < sessionCount()
            && m_sessions.at(static_cast<size_t>(m_current))->session.get() == session) {
            refresh();
        }
    });

    connect(session, &terminal::TerminalSession::titleChanged,
            this, &TerminalModel::sessionsChanged);

    connect(session, &terminal::TerminalSession::errorOccurred,
            this, &TerminalModel::errorOccurred);

    connect(session, &terminal::TerminalSession::finished, this,
            [this, session](int exitCode) {
                for (size_t i = 0; i < m_sessions.size(); ++i) {
                    if (m_sessions.at(i)->session.get() == session) {
                        emit sessionFinished(static_cast<int>(i), exitCode);
                        break;
                    }
                }
                emit sessionsChanged();
            });

    if (const core::Status status = session->start(m_workingDirectory); !status) {
        emit errorOccurred(status.error().toString());
        return;
    }

    beginResetModel();
    m_sessions.push_back(std::move(entry));
    m_current = sessionCount() - 1;
    endResetModel();

    refresh();
    emit sessionsChanged();
}

void TerminalModel::closeSession(int index)
{
    if (index < 0 || index >= sessionCount()) {
        return;
    }

    // Terminated explicitly rather than left to the destructor, so the shell is
    // gone before the tab disappears and cannot outlive the UI that owned it.
    m_sessions.at(static_cast<size_t>(index))->session->terminate();

    beginResetModel();
    m_sessions.erase(m_sessions.begin() + index);

    // Selects the neighbour rather than resetting to the first: closing the
    // third of four should leave the user near where they were.
    if (m_sessions.empty()) {
        m_current = -1;
    } else if (m_current >= sessionCount()) {
        m_current = sessionCount() - 1;
    }
    endResetModel();

    refresh();
    emit sessionsChanged();
}

void TerminalModel::sendText(const QString& text)
{
    if (m_current < 0 || m_current >= sessionCount()) {
        return;
    }
    m_sessions.at(static_cast<size_t>(m_current))->session->sendText(text);
}

void TerminalModel::sendKey(int key, int modifiers)
{
    if (m_current < 0 || m_current >= sessionCount()) {
        return;
    }

    terminal::TerminalSession& session =
        *m_sessions.at(static_cast<size_t>(m_current))->session;

    const bool control = (modifiers & Qt::ControlModifier) != 0;

    // Control characters first: Ctrl+C has to reach the shell as 0x03, not as
    // the letter C, or nothing can ever be interrupted.
    if (control && key >= Qt::Key_A && key <= Qt::Key_Z) {
        const char code = static_cast<char>(key - Qt::Key_A + 1);
        session.sendInput(QByteArray(1, code));
        return;
    }

    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        session.sendInput(QByteArray("\r"));
        return;
    case Qt::Key_Backspace:
        // DEL, not BS. Nearly every shell expects 0x7f here, and sending 0x08
        // moves the cursor left without deleting anything.
        session.sendInput(QByteArray(1, '\x7f'));
        return;
    case Qt::Key_Tab:
        session.sendInput(QByteArray("\t"));
        return;
    case Qt::Key_Escape:
        session.sendInput(QByteArray("\x1b"));
        return;
    case Qt::Key_Up:
        session.sendInput(QByteArray("\x1b[A"));
        return;
    case Qt::Key_Down:
        session.sendInput(QByteArray("\x1b[B"));
        return;
    case Qt::Key_Right:
        session.sendInput(QByteArray("\x1b[C"));
        return;
    case Qt::Key_Left:
        session.sendInput(QByteArray("\x1b[D"));
        return;
    case Qt::Key_Home:
        session.sendInput(QByteArray("\x1b[H"));
        return;
    case Qt::Key_End:
        session.sendInput(QByteArray("\x1b[F"));
        return;
    case Qt::Key_Delete:
        session.sendInput(QByteArray("\x1b[3~"));
        return;
    case Qt::Key_PageUp:
        session.sendInput(QByteArray("\x1b[5~"));
        return;
    case Qt::Key_PageDown:
        session.sendInput(QByteArray("\x1b[6~"));
        return;
    default:
        break;
    }
}

void TerminalModel::resizeTo(int columns, int rows)
{
    if (m_current < 0 || m_current >= sessionCount() || columns < 1 || rows < 1) {
        return;
    }
    m_sessions.at(static_cast<size_t>(m_current))->session->resize(columns, rows);
}

QString TerminalModel::plainText() const
{
    QString all;
    for (const QString& line : m_plain) {
        all += line;
        all += QLatin1Char('\n');
    }
    return all;
}

void TerminalModel::copyAll() const
{
    if (QClipboard* clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(plainText());
    }
}

void TerminalModel::refresh()
{
    beginResetModel();

    m_lines.clear();
    m_plain.clear();

    if (m_current >= 0 && m_current < sessionCount()) {
        const terminal::TerminalScreen& screen =
            m_sessions.at(static_cast<size_t>(m_current))->session->screen();

        const int total = screen.totalLines();
        m_lines.reserve(static_cast<size_t>(total));
        m_plain.reserve(static_cast<size_t>(total));

        for (int line = 0; line < total; ++line) {
            m_lines.push_back(markupFor(line));

            // Trailing spaces are stripped from the copyable text but not from
            // the markup: the grid is padded to its full width, and pasting a
            // command with 60 trailing spaces would be surprising.
            QString plain = screen.lineAt(line).text();
            while (plain.endsWith(QLatin1Char(' '))) {
                plain.chop(1);
            }
            m_plain.push_back(plain);
        }
    }

    endResetModel();

    ++m_revision;
    emit screenChanged();
}

QString TerminalModel::markupFor(int line) const
{
    const terminal::TerminalScreen& screen =
        m_sessions.at(static_cast<size_t>(m_current))->session->screen();

    QString markup;
    for (const terminal::StyledRun& run : screen.lineAt(line).runs) {
        const QString text = escapeForDisplay(run.text);
        if (text.isEmpty()) {
            continue;
        }

        // Inverse swaps the two, which is how a terminal draws a selection or a
        // highlighted prompt segment.
        //
        // Only the foreground is drawn. StyledText has no background attribute,
        // and faking one - an underline, a different foreground - would be a
        // wrong answer presented as a right one. A run with a background reads
        // as ordinary text until the view draws real backgrounds behind the
        // runs, which is the honest gap and is recorded as such.
        const int foreground = run.style.inverse ? run.style.background : run.style.foreground;

        QString open;
        QString close;

        if (foreground >= 0 || run.style.inverse) {
            open += QLatin1String("<font color=\"") + colorFor(foreground, true)
                  + QLatin1String("\">");
            close.prepend(QLatin1String("</font>"));
        }
        if (run.style.bold) {
            open += QLatin1String("<b>");
            close.prepend(QLatin1String("</b>"));
        }
        if (run.style.italic) {
            open += QLatin1String("<i>");
            close.prepend(QLatin1String("</i>"));
        }
        if (run.style.underline) {
            open += QLatin1String("<u>");
            close.prepend(QLatin1String("</u>"));
        }

        markup += open + text + close;
    }
    return markup;
}

QString TerminalModel::colorFor(int index, bool foreground) const
{
    // The theme rather than a fixed ANSI table: the standard blue on a
    // near-black background is close to unreadable, and a light theme would be
    // worse. These map onto colours already chosen to work in this palette.
    const QColor color = [&]() -> QColor {
        switch (index) {
        case 0:  return m_theme.bgChrome();       // black
        case 1:  return m_theme.red();
        case 2:  return m_theme.green();
        case 3:  return m_theme.yellow();
        case 4:  return m_theme.accent();         // blue
        case 5:  return m_theme.synKeyword();     // magenta
        case 6:  return m_theme.synType();        // cyan
        case 7:  return m_theme.textSecondary();  // white
        case 8:  return m_theme.textTertiary();   // bright black
        case 9:  return m_theme.red();
        case 10: return m_theme.green();
        case 11: return m_theme.yellow();
        case 12: return m_theme.accentHover();
        case 13: return m_theme.synKeyword();
        case 14: return m_theme.synType();
        case 15: return m_theme.textPrimary();    // bright white
        default:
            break;
        }

        // 256-colour and anything unrecognised fall back to the default, which
        // is legible, rather than to a computed colour that might not be.
        return foreground ? m_theme.textPrimary() : m_theme.bgEditor();
    }();

    return color.name(QColor::HexRgb);
}

void TerminalModel::setInstance(TerminalModel* instance)
{
    g_instance = instance;
}

TerminalModel* TerminalModel::create(QQmlEngine*, QJSEngine*)
{
    Q_ASSERT_X(g_instance, "TerminalModel::create",
               "TerminalModel::setInstance was not called");

    QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
    return g_instance;
}

} // namespace keys::ui
