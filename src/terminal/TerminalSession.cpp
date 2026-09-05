#include "terminal/TerminalSession.h"

#include "core/Log.h"

#include <QFileInfo>

using keys::core::Ok;
using keys::core::Status;

namespace keys::terminal {

TerminalSession::TerminalSession(QObject* parent)
    : QObject(parent), m_parser(m_screen)
{
    m_repaintTimer.setSingleShot(true);
    m_repaintTimer.setInterval(kRepaintIntervalMs);
    connect(&m_repaintTimer, &QTimer::timeout, this, [this] {
        emit screenChanged();

        // The title arrives inside the output stream, so it is checked on the
        // same beat rather than on a separate signal from the parser.
        if (m_parser.title() != m_lastTitle) {
            m_lastTitle = m_parser.title();
            emit titleChanged();
        }
    });
}

TerminalSession::~TerminalSession()
{
    // The PTY's destructor terminates the shell; being explicit here documents
    // that closing a terminal really does end its process rather than leaking a
    // detached shell.
    terminate();
}

Status TerminalSession::start(const QString& workingDirectory)
{
    m_pty = process::Pty::create(this);

    connect(m_pty.get(), &process::Pty::outputReceived, this,
            [this](const QByteArray& data) {
                // Parsed immediately so the model is always current; the view is
                // told on the timer, so a burst of output costs one repaint
                // rather than thousands.
                m_parser.parse(data);
                if (!m_repaintTimer.isActive()) {
                    m_repaintTimer.start();
                }
            });

    connect(m_pty.get(), &process::Pty::finished, this, [this](int exitCode) {
        // Flush whatever the shell wrote on its way out before reporting the
        // exit, or its last line would never be shown.
        emit screenChanged();
        emit finished(exitCode);
    });

    connect(m_pty.get(), &process::Pty::errorOccurred,
            this, &TerminalSession::errorOccurred);

    process::PtyOptions options;
    options.workingDirectory = workingDirectory;
    options.columns = m_screen.columns();
    options.rows = m_screen.rows();

    const QString shell = process::Pty::defaultShell();
    m_shellName = QFileInfo(shell).completeBaseName();

    const Status status = m_pty->start(options);
    if (!status) {
        m_pty.reset();
        return status;
    }

    qCDebug(lcProcess) << "terminal session started with" << shell;
    return Ok();
}

void TerminalSession::sendInput(const QByteArray& data)
{
    if (m_pty) {
        m_pty->write(data);
    }
}

void TerminalSession::sendText(const QString& text)
{
    sendInput(text.toUtf8());
}

void TerminalSession::resize(int columns, int rows)
{
    if (columns == m_screen.columns() && rows == m_screen.rows()) {
        return;
    }

    m_screen.resize(columns, rows);
    if (m_pty) {
        m_pty->resize(columns, rows);
    }
    emit screenChanged();
}

void TerminalSession::terminate()
{
    if (m_pty) {
        m_pty->terminate();
    }
}

bool TerminalSession::isRunning() const
{
    return m_pty && m_pty->isRunning();
}

QString TerminalSession::title() const
{
    // A shell that sets no title still needs a name on its tab.
    return m_parser.title().isEmpty() ? m_shellName : m_parser.title();
}

} // namespace keys::terminal
