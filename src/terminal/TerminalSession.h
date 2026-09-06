#pragma once

#include "core/Result.h"
#include "process/Pty.h"
#include "terminal/TerminalScreen.h"
#include "terminal/VtParser.h"

#include <QObject>
#include <QTimer>

#include <memory>

namespace keys::terminal {

/// One terminal: a shell, its screen, and the parser between them.
///
/// Ties the three together and adds the one thing neither has: **output
/// batching**. A build emits thousands of small writes, and repainting per write
/// would spend the whole frame budget on a screen nobody can read that fast.
/// Output is parsed as it arrives - so the model is always current - but the
/// change notification is coalesced, which is what keeps the UI responsive while
/// a compiler is running.
class TerminalSession : public QObject {
    Q_OBJECT

public:
    explicit TerminalSession(QObject* parent = nullptr);
    ~TerminalSession() override;

    /// Starts a shell in `workingDirectory`.
    core::Status start(const QString& workingDirectory);

    /// Sends the user's typing to the shell.
    void sendInput(const QByteArray& data);
    void sendText(const QString& text);

    /// Tells the shell the view changed size.
    void resize(int columns, int rows);

    void terminate();

    [[nodiscard]] bool isRunning() const;

    [[nodiscard]] const TerminalScreen& screen() const { return m_screen; }

    /// How many lines of history this terminal keeps. Set from the preference
    /// when the session is created.
    void setMaxScrollback(int lines) { m_screen.setMaxScrollback(lines); }

    /// The title the shell set, or the shell's own name if it set none.
    [[nodiscard]] QString title() const;

signals:
    /// The screen changed. Emitted at most once per coalescing interval, not
    /// once per write.
    void screenChanged();

    void finished(int exitCode);
    void errorOccurred(const QString& message);
    void titleChanged();

public:
    /// How long output is batched before the view is told. Around one frame at
    /// 60Hz: long enough to absorb a burst, short enough that typing still feels
    /// immediate.
    static constexpr int kRepaintIntervalMs = 16;

private:
    TerminalScreen m_screen;
    VtParser m_parser;

    std::unique_ptr<process::Pty> m_pty;

    QTimer m_repaintTimer;
    QString m_shellName;
    QString m_lastTitle;
};

} // namespace keys::terminal
