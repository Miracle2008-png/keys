#pragma once

#include "debugger/DebugSession.h"
#include "project/Project.h"
#include "workspace/EditorLayout.h"

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

namespace keys::ui {

/// The debug UI's state.
///
/// **Breakpoints outlive the session.** A user sets them before starting and
/// expects them still there after it ends, so they are kept here and pushed to
/// the adapter when a session starts — not held by the session.
///
/// **Every control reads the state.** Continue, Step and Stop are enabled from
/// the session's state rather than each deciding for itself, so a button never
/// offers something the adapter would refuse.
class DebugModel : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Debugger)
    QML_SINGLETON

    Q_PROPERTY(bool active READ isActive NOTIFY stateChanged)
    Q_PROPERTY(bool stopped READ isStopped NOTIFY stateChanged)
    Q_PROPERTY(QString stateLabel READ stateLabel NOTIFY stateChanged)
    Q_PROPERTY(QString stopReason READ stopReason NOTIFY stateChanged)

    /// Whether a debug configuration exists for this project. False hides the
    /// controls rather than showing them disabled forever.
    Q_PROPERTY(bool configured READ isConfigured NOTIFY configurationChanged)
    Q_PROPERTY(QString configurationName READ configurationName NOTIFY configurationChanged)

    Q_PROPERTY(QVariantList stack READ stack NOTIFY stackChanged)
    Q_PROPERTY(QVariantList variables READ variables NOTIFY variablesChanged)
    Q_PROPERTY(int selectedFrameId READ selectedFrameId NOTIFY stackChanged)

    /// Breakpoints in the active document, as line numbers for the gutter.
    Q_PROPERTY(QVariantList currentFileBreakpoints READ currentFileBreakpoints
                   NOTIFY breakpointsChanged)

    /// Where execution is stopped in the active document, or -1. The editor
    /// highlights this line.
    Q_PROPERTY(int currentLine READ currentLine NOTIFY stackChanged)

public:
    DebugModel(debugger::DebugSession& session, project::Project& project,
               workspace::EditorLayout& editors, QObject* parent = nullptr);

    static void setInstance(DebugModel* instance);
    static DebugModel* create(QQmlEngine* engine, QJSEngine* scriptEngine);

    [[nodiscard]] bool isActive() const { return m_session.isActive(); }
    [[nodiscard]] bool isStopped() const { return m_session.isStopped(); }
    [[nodiscard]] QString stateLabel() const;
    [[nodiscard]] QString stopReason() const;

    [[nodiscard]] bool isConfigured() const { return m_config.isValid(); }
    [[nodiscard]] QString configurationName() const { return m_config.name; }

    [[nodiscard]] QVariantList stack() const;
    [[nodiscard]] QVariantList variables() const;
    [[nodiscard]] int selectedFrameId() const { return m_session.selectedFrameId(); }

    [[nodiscard]] QVariantList currentFileBreakpoints() const;
    [[nodiscard]] int currentLine() const;

    // ---- Session -----------------------------------------------------------

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

    Q_INVOKABLE void resume();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stepOver();
    Q_INVOKABLE void stepInto();
    Q_INVOKABLE void stepOut();

    /// Shows a frame's variables and opens its source.
    Q_INVOKABLE void selectFrame(int index);

    // ---- Breakpoints -------------------------------------------------------

    /// Adds or removes a breakpoint. Line is one-based, matching the gutter.
    Q_INVOKABLE void toggleBreakpoint(const QString& path, int line);
    Q_INVOKABLE bool hasBreakpoint(const QString& path, int line) const;
    Q_INVOKABLE void clearBreakpoints();

signals:
    void stateChanged();
    void configurationChanged();
    void stackChanged();
    void variablesChanged();
    void breakpointsChanged();

    /// A frame was selected, or execution stopped somewhere. The workbench opens
    /// the file; the model does not.
    void locationRequested(const QString& path, int line);

    void notice(const QString& message);

    /// Output from the debuggee, for the run console.
    void output(const QString& text);

private:
    /// Builds the debug configuration for the open project, or an invalid one
    /// when Keys has no sensible default for its kind.
    void reloadConfiguration();

    /// Pushes every breakpoint to the adapter, per file. DAP sets a source's
    /// breakpoints as a set, so this sends whole files rather than one line.
    void pushAllBreakpoints();

    debugger::DebugSession& m_session;
    project::Project& m_project;
    workspace::EditorLayout& m_editors;

    debugger::DebugConfig m_config;

    /// Breakpoints by absolute path. Kept here rather than in the session so
    /// they survive it: a user sets them before starting and expects them after.
    QHash<QString, QList<int>> m_breakpoints;
};

} // namespace keys::ui
