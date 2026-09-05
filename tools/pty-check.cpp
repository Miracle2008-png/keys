// A standalone check that ConPTY really starts a shell and its output reaches
// the parser.
//
// Run before building any terminal UI: if this fails, nothing above it can work,
// and a UI would only obscure where the problem is. The ConPTY lifecycle has
// several ways to fail silently — the child starting but staying attached to the
// parent's console being the worst, because the shell looks like it worked.

#include "process/Pty.h"
#include "terminal/TerminalScreen.h"
#include "terminal/VtParser.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

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
    QTimer::singleShot(1500, [&] {
        out << "[still running: " << (pty->isRunning() ? "yes" : "no") << "]\n";
        const auto wrote = pty->write(QByteArray("echo KEYS_PTY_OK\r\n"));
        out << "[write " << (wrote ? "ok" : "FAILED") << "]\n";
        out.flush();
    });

    QTimer::singleShot(4000, [&] {
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
