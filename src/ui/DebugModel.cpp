#include "ui/DebugModel.h"

#include "core/Log.h"

#include <QDir>
#include <QFileInfo>
#include <QVariantMap>

#include <algorithm>

using keys::debugger::DebugConfig;
using keys::debugger::DebugState;
using keys::debugger::StackFrame;
using keys::debugger::Variable;

namespace keys::ui {
namespace {

/// The instance main() publishes for QML; see Theme.cpp for the reasoning.
DebugModel* g_instance = nullptr;

} // namespace

void DebugModel::setInstance(DebugModel* instance)
{
    g_instance = instance;
}

DebugModel* DebugModel::create(QQmlEngine* engine, QJSEngine* scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    Q_ASSERT_X(g_instance, "DebugModel::create",
               "DebugModel::setInstance was not called");

    QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
    return g_instance;
}

DebugModel::DebugModel(debugger::DebugSession& session, project::Project& project,
                       workspace::EditorLayout& editors, QObject* parent)
    : QObject(parent), m_session(session), m_project(project), m_editors(editors)
{
    connect(&m_project, &project::Project::opened, this, [this] { reloadConfiguration(); });
    connect(&m_project, &project::Project::closed, this, [this] { reloadConfiguration(); });

    connect(&m_session, &debugger::DebugSession::stateChanged,
            this, &DebugModel::stateChanged);
    connect(&m_session, &debugger::DebugSession::stackChanged,
            this, &DebugModel::stackChanged);
    connect(&m_session, &debugger::DebugSession::variablesChanged,
            this, &DebugModel::variablesChanged);

    connect(&m_session, &debugger::DebugSession::stopped, this,
            [this](const debugger::StopInfo&) {
                // Breakpoints are set once the adapter is ready, which is after
                // the first stop for adapters that stop on entry.
                pushAllBreakpoints();
            });

    connect(&m_session, &debugger::DebugSession::stackChanged, this, [this] {
        // Opening where execution stopped is the whole point of stopping.
        const std::vector<StackFrame>& stack = m_session.stack();
        if (!stack.empty() && stack.front().hasSource()) {
            emit locationRequested(stack.front().sourcePath, stack.front().line);
        }
    });

    connect(&m_session, &debugger::DebugSession::breakpointsVerified, this,
            [this](const QString& path, const std::vector<debugger::Breakpoint>& verified) {
                // The adapter may have moved them to the next executable line.
                // Showing them where the user clicked would be a lie.
                QList<int> lines;
                lines.reserve(static_cast<qsizetype>(verified.size()));
                for (const debugger::Breakpoint& breakpoint : verified) {
                    if (breakpoint.verified) {
                        lines.append(breakpoint.effectiveLine());
                    }
                }
                if (!lines.isEmpty()) {
                    m_breakpoints.insert(path, lines);
                    emit breakpointsChanged();
                }
            });

    connect(&m_session, &debugger::DebugSession::failed, this, &DebugModel::notice);

    connect(&m_session, &debugger::DebugSession::outputReceived, this,
            [this](const QString& text, const QString&) { emit output(text); });

    connect(&m_session, &debugger::DebugSession::sessionEnded, this, [this] {
        emit stateChanged();
        emit stackChanged();
    });

    reloadConfiguration();
}

void DebugModel::reloadConfiguration()
{
    m_config = DebugConfig{};

    if (m_project.isOpen()) {
        // Only kinds with a conventional adapter get one. Nothing is guessed:
        // an adapter that is not installed reports so when started, and a
        // configuration Keys invented for an unusual project would fail in ways
        // the user could not act on.
        switch (m_project.kind()) {
        case project::ProjectKind::Cpp:
            // CodeLLDB, the usual adapter for native code on every platform.
            m_config.name = QStringLiteral("lldb");
            m_config.adapterProgram = QStringLiteral("codelldb");
            m_config.adapterArguments = {QStringLiteral("--port"), QStringLiteral("0")};
            break;

        case project::ProjectKind::Rust:
            m_config.name = QStringLiteral("lldb");
            m_config.adapterProgram = QStringLiteral("codelldb");
            m_config.adapterArguments = {QStringLiteral("--port"), QStringLiteral("0")};
            break;

        case project::ProjectKind::Python:
            m_config.name = QStringLiteral("debugpy");
            m_config.adapterProgram = QStringLiteral("python");
            m_config.adapterArguments = {QStringLiteral("-m"), QStringLiteral("debugpy.adapter")};
            break;

        case project::ProjectKind::Node:
        case project::ProjectKind::Go:
        case project::ProjectKind::Unknown:
            break;
        }
    }

    emit configurationChanged();
}

QString DebugModel::stateLabel() const
{
    return debugger::debugStateLabel(m_session.state());
}

QString DebugModel::stopReason() const
{
    const debugger::StopInfo& info = m_session.stopInfo();
    if (!info.description.isEmpty()) {
        return info.description;
    }
    return info.reason;
}

QVariantList DebugModel::stack() const
{
    QVariantList result;
    const std::vector<StackFrame>& frames = m_session.stack();
    result.reserve(static_cast<qsizetype>(frames.size()));

    for (const StackFrame& frame : frames) {
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), frame.id);
        entry.insert(QStringLiteral("name"), frame.name);
        entry.insert(QStringLiteral("line"), frame.line);
        entry.insert(QStringLiteral("path"), frame.sourcePath);
        entry.insert(QStringLiteral("fileName"), QFileInfo(frame.sourcePath).fileName());
        // A frame with no source cannot be opened, so the view dims it rather
        // than offering a click that does nothing.
        entry.insert(QStringLiteral("hasSource"), frame.hasSource());
        result.append(entry);
    }
    return result;
}

QVariantList DebugModel::variables() const
{
    QVariantList result;
    const std::vector<Variable>& variables = m_session.variables();
    result.reserve(static_cast<qsizetype>(variables.size()));

    for (const Variable& variable : variables) {
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), variable.name);
        entry.insert(QStringLiteral("value"), variable.value);
        entry.insert(QStringLiteral("type"), variable.type);
        entry.insert(QStringLiteral("expandable"), variable.isExpandable());
        result.append(entry);
    }
    return result;
}

QVariantList DebugModel::currentFileBreakpoints() const
{
    const editor::TextDocument* document = m_editors.activeDocument();
    if (!document) {
        return {};
    }

    QVariantList result;
    for (const int line : m_breakpoints.value(document->path())) {
        result.append(line);
    }
    return result;
}

int DebugModel::currentLine() const
{
    if (!m_session.isStopped()) {
        return -1;
    }

    const editor::TextDocument* document = m_editors.activeDocument();
    const std::vector<StackFrame>& stack = m_session.stack();
    if (!document || stack.empty()) {
        return -1;
    }

    // Only when the stopped frame is in the file being shown: highlighting a
    // line number in an unrelated file would be meaningless.
    const StackFrame& frame = stack.front();
    return QFileInfo(frame.sourcePath) == QFileInfo(document->path()) ? frame.line : -1;
}

// ---- Session ---------------------------------------------------------------

void DebugModel::start()
{
    if (!m_config.isValid()) {
        emit notice(tr("Keys has no debug configuration for this kind of project."));
        return;
    }
    if (m_session.isActive()) {
        return;
    }

    if (const core::Status status = m_session.start(m_config, m_project.root()); !status) {
        emit notice(status.error().message());
        return;
    }
    pushAllBreakpoints();
}

void DebugModel::stop()
{
    m_session.stop();
}

void DebugModel::resume() { m_session.resume(); }
void DebugModel::pause() { m_session.pause(); }
void DebugModel::stepOver() { m_session.stepOver(); }
void DebugModel::stepInto() { m_session.stepInto(); }
void DebugModel::stepOut() { m_session.stepOut(); }

void DebugModel::selectFrame(int index)
{
    const std::vector<StackFrame>& stack = m_session.stack();
    if (index < 0 || index >= static_cast<int>(stack.size())) {
        return;
    }

    const StackFrame& frame = stack.at(static_cast<size_t>(index));
    m_session.selectFrame(frame.id);

    if (frame.hasSource()) {
        emit locationRequested(frame.sourcePath, frame.line);
    }
}

// ---- Breakpoints -----------------------------------------------------------

bool DebugModel::hasBreakpoint(const QString& path, int line) const
{
    return m_breakpoints.value(path).contains(line);
}

void DebugModel::toggleBreakpoint(const QString& path, int line)
{
    if (path.isEmpty() || line < 1) {
        return;
    }

    QList<int>& lines = m_breakpoints[path];
    if (const qsizetype index = lines.indexOf(line); index >= 0) {
        lines.removeAt(index);
    } else {
        lines.append(line);
        std::sort(lines.begin(), lines.end());
    }

    if (lines.isEmpty()) {
        m_breakpoints.remove(path);
    }

    // Pushed immediately when a session is running, so a breakpoint set while
    // stopped takes effect on the next step rather than the next launch.
    if (m_session.isActive()) {
        m_session.setBreakpoints(path, std::vector<int>(lines.cbegin(), lines.cend()));
    }

    emit breakpointsChanged();
}

void DebugModel::clearBreakpoints()
{
    const QStringList paths = m_breakpoints.keys();
    m_breakpoints.clear();

    if (m_session.isActive()) {
        for (const QString& path : paths) {
            m_session.setBreakpoints(path, {});
        }
    }
    emit breakpointsChanged();
}

void DebugModel::pushAllBreakpoints()
{
    if (!m_session.isActive()) {
        return;
    }
    for (auto it = m_breakpoints.cbegin(); it != m_breakpoints.cend(); ++it) {
        m_session.setBreakpoints(it.key(),
                                 std::vector<int>(it.value().cbegin(), it.value().cend()));
    }
}

} // namespace keys::ui
