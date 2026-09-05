#include "process/PtyWindows.h"

#include "core/Log.h"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>

// Kept out of the header: windows.h defines macros that collide with ordinary
// identifiers throughout the rest of Keys.
//
// NOMINMAX because windows.h otherwise defines min and max as macros, which
// breaks every use of std::min and std::max in this file - and in anything that
// includes it after.
// WIN32_LEAN_AND_MEAN drops the parts of the Win32 API Keys does not use, which
// is most of them.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>

using keys::core::ErrorCode;
using keys::core::Err;
using keys::core::Ok;
using keys::core::Status;

namespace keys::process {
namespace {

/// The message for the last Windows error, for an error a user might see.
QString lastErrorMessage()
{
    const DWORD code = GetLastError();
    if (code == 0) {
        return {};
    }

    LPWSTR buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
            | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);

    QString message = length > 0
                          ? QString::fromWCharArray(buffer, static_cast<int>(length)).trimmed()
                          : QStringLiteral("Windows error %1").arg(code);
    if (buffer) {
        LocalFree(buffer);
    }
    return message;
}

} // namespace

// ---- PtyReader --------------------------------------------------------------

PtyReader::PtyReader(void* pipe, QObject* parent) : QThread(parent), m_pipe(pipe) {}

void PtyReader::run()
{
    // 4 KiB is comfortably more than a shell emits per write, so a burst of
    // output rarely needs more than one read, while staying small enough that
    // interactive output reaches the screen promptly rather than pooling.
    constexpr DWORD kBufferSize = 4096;
    char buffer[kBufferSize];

    while (!m_stop.load(std::memory_order_acquire)) {
        DWORD read = 0;
        const BOOL ok = ReadFile(m_pipe, buffer, kBufferSize, &read, nullptr);

        if (!ok || read == 0) {
            // The console closed its end: the shell has exited, or the session
            // is being torn down. Either way this thread is done.
            break;
        }

        emit dataRead(QByteArray(buffer, static_cast<int>(read)));
    }

    emit pipeClosed();
}

// ---- PtyWindows -------------------------------------------------------------

struct PtyWindows::Impl {
    HPCON console = nullptr;

    /// Keys' ends of the two pipes. The console's ends are closed as soon as
    /// CreatePseudoConsole has duplicated them.
    HANDLE writeToChild = INVALID_HANDLE_VALUE;
    HANDLE readFromChild = INVALID_HANDLE_VALUE;

    PROCESS_INFORMATION process{};
    std::vector<char> attributeBuffer;
};

PtyWindows::PtyWindows(QObject* parent)
    : Pty(parent), m_impl(std::make_unique<Impl>())
{
}

PtyWindows::~PtyWindows()
{
    terminate();
}

Status PtyWindows::start(const PtyOptions& options)
{
    if (isRunning()) {
        return Err(ErrorCode::AlreadyExists,
                   QStringLiteral("This terminal session is already running"));
    }

    // Step 1: the two pipes. `consoleRead` and `consoleWrite` are the console's
    // ends; Keys keeps `writeToChild` and `readFromChild`.
    HANDLE consoleRead = INVALID_HANDLE_VALUE;
    HANDLE consoleWrite = INVALID_HANDLE_VALUE;

    if (!CreatePipe(&consoleRead, &m_impl->writeToChild, nullptr, 0)
        || !CreatePipe(&m_impl->readFromChild, &consoleWrite, nullptr, 0)) {
        const QString message = lastErrorMessage();
        cleanup();
        return Err(ErrorCode::IoError,
                   QStringLiteral("Could not create the terminal pipes: %1").arg(message));
    }

    // Step 2: the pseudo-console.
    const COORD size{static_cast<SHORT>(std::max(1, options.columns)),
                     static_cast<SHORT>(std::max(1, options.rows))};

    const HRESULT created =
        CreatePseudoConsole(size, consoleRead, consoleWrite, 0, &m_impl->console);

    // ConPTY duplicates the handles it is given, and a lingering copy stops the
    // pipe from ever reporting end-of-file - so the console's ends are closed
    // straight away, whether or not creation succeeded.
    CloseHandle(consoleRead);
    CloseHandle(consoleWrite);

    if (FAILED(created)) {
        cleanup();
        return Err(ErrorCode::NotSupported,
                   QStringLiteral("Could not create a pseudo-console (0x%1). "
                                  "ConPTY requires Windows 10 1809 or newer.")
                       .arg(static_cast<quint32>(created), 8, 16, QLatin1Char('0')));
    }

    // Step 3: the child, with the console attached through a thread-attribute
    // list. This is what makes it a terminal session rather than a process with
    // redirected stdio.
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(STARTUPINFOEXW);

    // The std handles are left at their defaults deliberately. ConPTY supplies
    // the child's console through the attribute list, and setting
    // STARTF_USESTDHANDLES here would override that with ordinary redirection -
    // giving the child a pipe instead of a terminal, which is exactly the
    // behaviour this module exists to avoid.
    //
    // Nor is handle inheritance used: ConPTY duplicates the pipe handles into
    // the child itself. Marking them inheritable, or suppressing the parent's
    // std handles, were both tried and both made things worse - the first let
    // the shell write to Keys' own stdout, the second starved it of a console
    // entirely so it exited immediately.

    SIZE_T attributeSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeSize);
    m_impl->attributeBuffer.resize(attributeSize);
    startup.lpAttributeList =
        reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(m_impl->attributeBuffer.data());

    if (!InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &attributeSize)) {
        const QString message = lastErrorMessage();
        cleanup();
        return Err(ErrorCode::IoError,
                   QStringLiteral("Could not prepare the terminal process: %1").arg(message));
    }

    if (!UpdateProcThreadAttribute(startup.lpAttributeList, 0,
                                   PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                   m_impl->console, sizeof(HPCON),
                                   nullptr, nullptr)) {
        const QString message = lastErrorMessage();
        cleanup();
        return Err(ErrorCode::IoError,
                   QStringLiteral("Could not attach the pseudo-console: %1").arg(message));
    }

    const QString program = options.program.isEmpty() ? defaultShell() : options.program;

    // Native separators, and quoted. Qt returns paths with forward slashes,
    // which most Windows file APIs accept - but CreateProcessW resolves the
    // program through the command line, where a forward-slash path is not
    // recognised. It launches *something*, which exits immediately, and the
    // only symptom is a terminal that produces one line and dies.
    //
    // Quoting matters for the same reason: "Program Files" would otherwise be
    // parsed as a program named "Program" with an argument "Files".
    QString commandLine = QLatin1Char('"') + QDir::toNativeSeparators(program)
                          + QLatin1Char('"');
    for (const QString& argument : options.arguments) {
        commandLine += QLatin1Char(' ') + argument;
    }

    // CreateProcessW may modify the command line it is given, so it gets a
    // writable copy rather than a pointer into a QString's buffer.
    std::wstring commandLineBuffer = commandLine.toStdWString();

    const QString workingDirectory =
        options.workingDirectory.isEmpty() ? QDir::currentPath() : options.workingDirectory;
    const std::wstring workingDirectoryBuffer =
        QDir::toNativeSeparators(workingDirectory).toStdWString();

    // The environment tells the shell it is inside Keys, which lets a prompt or
    // a script adapt - the same convention other editors use.
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("TERM_PROGRAM"), QStringLiteral("Keys"));
    for (const QString& entry : options.environment) {
        const int split = entry.indexOf(QLatin1Char('='));
        if (split > 0) {
            environment.insert(entry.left(split), entry.mid(split + 1));
        }
    }

    // A Windows environment block is a sequence of NUL-terminated strings ended
    // by an extra NUL.
    std::wstring environmentBlock;
    for (const QString& key : environment.keys()) {
        // Materialised into a QString first: QT_USE_QSTRINGBUILDER makes the
        // concatenation an expression template, which has no toStdWString.
        const QString entry = key + QLatin1Char('=') + environment.value(key);
        environmentBlock += entry.toStdWString();
        environmentBlock.push_back(L'\0');
    }
    environmentBlock.push_back(L'\0');

    const BOOL started = CreateProcessW(
        nullptr,
        commandLineBuffer.data(),
        nullptr, nullptr,

        // No handle inheritance. ConPTY duplicates the pipe handles into the
        // child itself when the pseudo-console is attached through the
        // attribute list, so nothing needs to be inherited - and inheriting
        // Keys' own std handles would let the shell write to them instead.
        FALSE,

        // EXTENDED_STARTUPINFO_PRESENT is what makes CreateProcessW read the
        // attribute list at all. Deliberately no DETACHED_PROCESS or
        // CREATE_NEW_CONSOLE: either would conflict with the pseudo-console the
        // attribute list attaches.
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        environmentBlock.data(),
        workingDirectoryBuffer.c_str(),

        // The *extended* structure, cast down - not &startup.StartupInfo.
        // CreateProcessW reads lpAttributeList only when it is given the
        // STARTUPINFOEX; passing the inner STARTUPINFOW compiles and starts the
        // process, but the pseudo-console is never attached, so the child
        // inherits the parent's console and its output never reaches the pipe.
        // This is the single most common ConPTY mistake and it fails silently.
        reinterpret_cast<LPSTARTUPINFOW>(&startup),
        &m_impl->process);

    if (!started) {
        const QString message = lastErrorMessage();
        cleanup();
        return Err(ErrorCode::ProcessFailed,
                   QStringLiteral("Could not start %1: %2").arg(program, message));
    }

    m_exitReported = false;

    // Step 4: read the console's output on its own thread. Queued connections
    // carry the bytes to whichever thread owns this object.
    m_reader = new PtyReader(m_impl->readFromChild, this);
    connect(m_reader, &PtyReader::dataRead, this, &Pty::outputReceived,
            Qt::QueuedConnection);
    connect(m_reader, &PtyReader::pipeClosed, this, &PtyWindows::reportExit,
            Qt::QueuedConnection);
    m_reader->start();

    qCDebug(lcProcess) << "terminal started:" << program << "in" << workingDirectory;
    return Ok();
}

Status PtyWindows::write(const QByteArray& data)
{
    if (m_impl->writeToChild == INVALID_HANDLE_VALUE) {
        return Err(ErrorCode::NotSupported,
                   QStringLiteral("This terminal is not running"));
    }

    DWORD written = 0;
    if (!WriteFile(m_impl->writeToChild, data.constData(),
                   static_cast<DWORD>(data.size()), &written, nullptr)) {
        return Err(ErrorCode::IoError,
                   QStringLiteral("Could not send input to the shell: %1")
                       .arg(lastErrorMessage()));
    }
    return Ok();
}

Status PtyWindows::resize(int columns, int rows)
{
    if (!m_impl->console) {
        return Err(ErrorCode::NotSupported,
                   QStringLiteral("This terminal is not running"));
    }

    const COORD size{static_cast<SHORT>(std::max(1, columns)),
                     static_cast<SHORT>(std::max(1, rows))};

    const HRESULT result = ResizePseudoConsole(m_impl->console, size);
    if (FAILED(result)) {
        return Err(ErrorCode::IoError, QStringLiteral("Could not resize the terminal"));
    }
    return Ok();
}

bool PtyWindows::isRunning() const
{
    if (!m_impl->process.hProcess) {
        return false;
    }
    return WaitForSingleObject(m_impl->process.hProcess, 0) == WAIT_TIMEOUT;
}

void PtyWindows::reportExit()
{
    // The reader signals on both a clean exit and a teardown, so this guards
    // against reporting twice.
    if (m_exitReported) {
        return;
    }
    m_exitReported = true;

    int exitCode = -1;
    if (m_impl->process.hProcess) {
        // A short wait rather than none: the pipe closes fractionally before
        // the process record is final, and reporting -1 for a clean exit would
        // make every session look like it failed.
        WaitForSingleObject(m_impl->process.hProcess, 250);

        DWORD code = 0;
        if (GetExitCodeProcess(m_impl->process.hProcess, &code) && code != STILL_ACTIVE) {
            exitCode = static_cast<int>(code);
        }
    }

    qCDebug(lcProcess) << "terminal exited with" << exitCode;
    emit finished(exitCode);
}

void PtyWindows::terminate()
{
    if (m_impl->process.hProcess && isRunning()) {
        TerminateProcess(m_impl->process.hProcess, 0);
    }
    cleanup();
}

void PtyWindows::cleanup()
{
    // Order matters. Closing the console while the child still holds its end is
    // the classic ConPTY hang: ClosePseudoConsole waits for the child to finish
    // draining, which it cannot do if its input pipe is still open.
    if (m_impl->writeToChild != INVALID_HANDLE_VALUE) {
        CloseHandle(m_impl->writeToChild);
        m_impl->writeToChild = INVALID_HANDLE_VALUE;
    }

    if (m_reader) {
        m_reader->requestStop();
    }

    if (m_impl->console) {
        ClosePseudoConsole(m_impl->console);
        m_impl->console = nullptr;
    }

    // Closing the console releases the reader's blocking ReadFile, so the
    // thread can now be joined without hanging.
    if (m_reader) {
        m_reader->wait(2000);
        delete m_reader;
        m_reader = nullptr;
    }

    if (m_impl->readFromChild != INVALID_HANDLE_VALUE) {
        CloseHandle(m_impl->readFromChild);
        m_impl->readFromChild = INVALID_HANDLE_VALUE;
    }

    if (!m_impl->attributeBuffer.empty()) {
        DeleteProcThreadAttributeList(
            reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(m_impl->attributeBuffer.data()));
        m_impl->attributeBuffer.clear();
    }

    if (m_impl->process.hThread) {
        CloseHandle(m_impl->process.hThread);
        m_impl->process.hThread = nullptr;
    }
    if (m_impl->process.hProcess) {
        CloseHandle(m_impl->process.hProcess);
        m_impl->process.hProcess = nullptr;
    }
}

// ---- Factory ----------------------------------------------------------------

std::unique_ptr<Pty> Pty::create(QObject* parent)
{
    return std::make_unique<PtyWindows>(parent);
}

QString Pty::defaultShell()
{
    // PowerShell first: it is what a Windows developer expects, and it handles
    // VT sequences properly. cmd.exe always exists as a fallback.
    for (const QString& candidate : {QStringLiteral("pwsh.exe"),
                                     QStringLiteral("powershell.exe")}) {
        const QString found = QStandardPaths::findExecutable(candidate);
        if (!found.isEmpty()) {
            return found;
        }
    }

    const QString comspec = qEnvironmentVariable("COMSPEC");
    return comspec.isEmpty() ? QStringLiteral("cmd.exe") : comspec;
}

} // namespace keys::process
