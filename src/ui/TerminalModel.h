#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QString>

#include <memory>
#include <vector>

namespace keys::terminal {
class TerminalSession;
struct CellStyle;
}

namespace keys::ui {

class Theme;

/// The terminal panel: one shell per tab, and the lines each one has drawn.
///
/// **A list of lines, not a grid of cells.** An 80x24 terminal is 1920 cells,
/// and a scrollback of a few thousand lines is hundreds of thousands. One QML
/// item per cell is not affordable. The screen already stores each line as runs
/// of uniform style, so a line becomes one string of markup and one Text item,
/// which is what makes a full scrollback cheap enough to scroll.
///
/// **Sessions outlive the view.** A build keeps running while the panel is
/// closed, and closing the panel must not kill it. The sessions live here; the
/// panel is a window onto them.
class TerminalModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(Terminal)
    QML_SINGLETON

    /// How many terminals are open, and which one the panel is showing.
    Q_PROPERTY(int sessionCount READ sessionCount NOTIFY sessionsChanged)
    Q_PROPERTY(int currentSession READ currentSession WRITE setCurrentSession
                   NOTIFY sessionsChanged)

    /// The visible session's state, for the tab strip and the header.
    Q_PROPERTY(QString title READ title NOTIFY sessionsChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY sessionsChanged)

    /// Where the caret is, in the coordinates this model reports lines in.
    Q_PROPERTY(int cursorLine READ cursorLine NOTIFY screenChanged)
    Q_PROPERTY(int cursorColumn READ cursorColumn NOTIFY screenChanged)
    Q_PROPERTY(bool cursorVisible READ cursorVisible NOTIFY screenChanged)

    /// Bumped whenever the screen changes. QML bindings that call Q_INVOKABLE
    /// functions have nothing to depend on otherwise, and would never
    /// re-evaluate - the same trap the editor hit.
    Q_PROPERTY(int revision READ revision NOTIFY screenChanged)

public:
    enum Roles {
        MarkupRole = Qt::UserRole + 1,  ///< the line as styled markup
        PlainTextRole,                  ///< the same line, for copying
    };

    explicit TerminalModel(const Theme& theme, QObject* parent = nullptr);
    ~TerminalModel() override;

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int sessionCount() const { return static_cast<int>(m_sessions.size()); }
    [[nodiscard]] int currentSession() const { return m_current; }
    void setCurrentSession(int index);

    [[nodiscard]] QString title() const;
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] int cursorLine() const;
    [[nodiscard]] int cursorColumn() const;
    [[nodiscard]] bool cursorVisible() const;
    [[nodiscard]] int revision() const { return m_revision; }

    /// The title of one session, for the tab strip.
    Q_INVOKABLE [[nodiscard]] QString titleAt(int index) const;

    /// Starts a shell in the project directory. Does nothing without a project:
    /// a terminal opened in whatever directory the application happened to
    /// start in is a trap rather than a convenience.
    Q_INVOKABLE void openSession();

    /// Closes one, terminating its shell.
    Q_INVOKABLE void closeSession(int index);

    /// Sends the user's typing to the visible shell.
    Q_INVOKABLE void sendText(const QString& text);

    /// Sends a key that is not a character - Return, Backspace, the arrows -
    /// as the escape sequence a terminal expects.
    Q_INVOKABLE void sendKey(int key, int modifiers);

    /// Tells the shell the view changed size, so programs that draw to the
    /// full width lay out correctly.
    Q_INVOKABLE void resizeTo(int columns, int rows);

    /// Every visible line, joined, for Copy All.
    Q_INVOKABLE [[nodiscard]] QString plainText() const;

    /// Puts the whole buffer on the clipboard. Here rather than as a general
    /// clipboard method on AppController: the terminal is the only thing that
    /// copies its own scrollback, and a general one invites copying anything
    /// from anywhere in QML.
    Q_INVOKABLE void copyAll() const;

    /// The directory a new terminal should start in.
    void setWorkingDirectory(const QString& path);

    static void setInstance(TerminalModel* instance);
    static TerminalModel* create(QQmlEngine*, QJSEngine*);

signals:
    void sessionsChanged();
    void screenChanged();

    /// A shell exited. The panel reports it rather than silently closing the
    /// tab: a shell that died is something the user wants to see.
    void sessionFinished(int index, int exitCode);

    void errorOccurred(const QString& message);

private:
    /// Rebuilds the cached markup for the visible session.
    void refresh();

    /// One line's runs as markup, with the palette resolved against the theme.
    [[nodiscard]] QString markupFor(int line) const;

    /// A palette index resolved to a colour the theme defines. Terminal colours
    /// come from the theme rather than a fixed ANSI table so a light theme does
    /// not end up drawing dark blue on near-black.
    [[nodiscard]] QString colorFor(int index, bool foreground) const;

    struct Session;

    const Theme& m_theme;
    std::vector<std::unique_ptr<Session>> m_sessions;
    int m_current = -1;
    int m_revision = 0;

    QString m_workingDirectory;

    /// The visible session's lines, as markup. Cached rather than built in
    /// data(): a ListView asks for the same row repeatedly while scrolling, and
    /// rebuilding the markup each time would make scrolling cost more than
    /// drawing.
    std::vector<QString> m_lines;
    std::vector<QString> m_plain;
};

} // namespace keys::ui
