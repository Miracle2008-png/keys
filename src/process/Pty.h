#pragma once

#include "core/Result.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

namespace keys::process {

/// How a pseudo-terminal is started.
struct PtyOptions {
    /// The shell to run. Empty means the platform's default, resolved by
    /// defaultShell().
    QString program;
    QStringList arguments;

    /// Where the shell starts. Empty means the process's own directory.
    QString workingDirectory;

    /// Extra environment entries, applied over the inherited environment. Used
    /// for the variables that tell a shell it is inside an editor.
    QStringList environment;

    /// Initial size. A shell asks the terminal for this to lay out its prompt,
    /// so starting at a plausible size avoids a visible reflow on the first
    /// frame.
    int columns = 80;
    int rows = 24;
};

/// A pseudo-terminal: a child process attached to a virtual console.
///
/// **Why a real PTY.** A shell behaves differently when it detects a pipe rather
/// than a terminal — no prompt, no colour, no line editing, and interactive
/// programs refuse to run at all. Piping stdin and stdout would produce
/// something that looks like a terminal for `echo` and fails for everything a
/// developer actually uses. The brief rules that out, and it is the right call.
///
/// **Platform split.** This is the interface; PtyWindows implements it over
/// ConPTY (`CreatePseudoConsole`). A Unix backend over `forkpty` implements the
/// same interface later without the terminal module changing, which is why the
/// VT parsing and screen model live above this line rather than inside it.
class Pty : public QObject {
    Q_OBJECT

public:
    explicit Pty(QObject* parent = nullptr) : QObject(parent) {}
    ~Pty() override = default;

    /// Creates the platform's implementation.
    [[nodiscard]] static std::unique_ptr<Pty> create(QObject* parent = nullptr);

    /// The shell to run when none is configured. Prefers PowerShell on Windows
    /// and the user's $SHELL elsewhere, falling back to something that always
    /// exists.
    [[nodiscard]] static QString defaultShell();

    /// Starts the shell. Fails if the console or the process cannot be created;
    /// the object is left unstarted in that case.
    virtual core::Status start(const PtyOptions& options) = 0;

    /// Sends input to the shell, as the user's keystrokes.
    virtual core::Status write(const QByteArray& data) = 0;

    /// Tells the shell the window changed size, so it can reflow its prompt and
    /// any full-screen program can redraw.
    virtual core::Status resize(int columns, int rows) = 0;

    /// Ends the session, terminating the child if it has not exited.
    virtual void terminate() = 0;

    [[nodiscard]] virtual bool isRunning() const = 0;

signals:
    /// Output from the shell, as raw bytes. Parsing is the terminal module's
    /// job; this layer moves bytes and nothing else.
    void outputReceived(const QByteArray& data);

    /// The shell exited. `exitCode` is the process's own, or -1 if it could not
    /// be determined.
    void finished(int exitCode);

    /// Something went wrong that the user should see - the shell could not be
    /// started, or the pipe broke unexpectedly.
    void errorOccurred(const QString& message);
};

} // namespace keys::process
