// A standalone check that ConPTY really starts a shell and its output reaches
// the parser.
//
// **Run it detached, not from a shell.** ConPTY behaves differently depending
// on whether the parent process owns a console. From a console parent - and
// from an MSYS/Git-Bash pty in particular - the child joins the parent's
// console instead of the pseudo-console, and only the ~16 bytes ConPTY writes
// itself ever reach the pipe. That looks exactly like a broken PTY layer and is
// not one: keys.exe is a WIN32 GUI process with no console, where the same code
// delivers the shell's full output. Measured both ways with a minimal program
// independent of Keys: 16 bytes from a console parent, 222 from a GUI parent,
// same code, same machine.
//
//     cmake --build build --target keys_pty_check
//     powershell -Command "Start-Process build/bin/keys_pty_check.exe"
//
// It writes its report beside the executable rather than to stdout, because a
// detached process has nowhere to print.
//
// Run before building any terminal UI: if this fails *when run detached*,
// nothing above it can work.
#include "process/Pty.h"
#include "terminal/TerminalScreen.h"
#include "terminal/VtParser.h"

#include <QCoreApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QTimer>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    // Detached, so stdout goes nowhere. The report is a file beside the binary.
    QFile report(QCoreApplication::applicationDirPath()
                 + QStringLiteral("/pty-check-report.txt"));
    if (!report.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return 2;
    }
    QTextStream out(&report);

    keys::terminal::TerminalScreen screen;
    screen.resize(80, 24);
    keys::terminal::VtParser parser(screen);

    auto pty = keys::process::Pty::create();
    out << "shell: " << keys::process::Pty::defaultShell() << "\n";
    out.flush();

    int totalBytes = 0;

    QObject::connect(pty.get(), &keys::process::Pty::outputReceived,
                     [&](const QByteArray& data) {
                         totalBytes += data.size();
                         out << "[read " << data.size() << " bytes] "
                             << QString::fromLatin1(data.toHex(' ').left(150)) << "\n";
                         out.flush();
                         parser.parse(data);
                     });

    QObject::connect(pty.get(), &keys::process::Pty::finished,
                     [&](int code) {
                         out << "[shell exited " << code << "]\n";
                         out.flush();
                     });

    keys::process::PtyOptions options;
    options.columns = 80;
    options.rows = 24;

    const auto status = pty->start(options);
    if (!status) {
        out << "FAILED to start: " << status.error().toString() << "\n";
        return 1;
    }
    out << "started, running=" << (pty->isRunning() ? "yes" : "no") << "\n";
    out.flush();

    // Give the shell a moment to print its banner and prompt, then run a command
    // whose output is unmistakable.
    QTimer::singleShot(2500, [&] {
        out << "[still running: " << (pty->isRunning() ? "yes" : "no") << "]\n";
        const auto wrote = pty->write(QByteArray("echo KEYS_PTY_OK\r\n"));
        out << "[write " << (wrote ? "ok" : "FAILED") << "]\n";
        out.flush();
    });

    QTimer::singleShot(6000, [&] {
        out << "--- total bytes read: " << totalBytes << " ---\n";
        out << "--- screen ---\n";
        for (int i = 0; i < screen.totalLines(); ++i) {
            const QString line = screen.lineAt(i).text().trimmed();
            if (!line.isEmpty()) {
                out << "  " << line << "\n";
            }
        }
        out << "--- end ---\n";
        out.flush();

        pty->terminate();
        app.quit();
    });

    return app.exec();
}
