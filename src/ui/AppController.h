#pragma once

#include "config/AnimationPolicy.h"
#include "config/Settings.h"
#include "core/CommandRegistry.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>

namespace keys::ui {

/// The façade QML binds to.
///
/// QML talks to this one object rather than to core modules directly. That keeps
/// the boundary narrow — view logic cannot reach into the filesystem or spawn a
/// process on a whim — and it means the whole UI surface is reviewable in one
/// header. It also keeps QML testable: everything here is plain C++ that a test
/// can drive without a scene graph.
///
/// This is a façade, not a god object: it holds no state of its own beyond what
/// the view needs and delegates every decision to the module that owns it.
class AppController : public QObject {
    Q_OBJECT
    // Exposed to QML as "App": the QML side reads as App.sidebarVisible rather
    // than AppController.sidebarVisible, which is what the views actually mean.
    QML_NAMED_ELEMENT(App)
    QML_SINGLETON

    Q_PROPERTY(int animationDuration READ animationDuration NOTIFY animationChanged)
    Q_PROPERTY(int fastAnimationDuration READ fastAnimationDuration NOTIFY animationChanged)

    Q_PROPERTY(bool sidebarVisible READ sidebarVisible NOTIFY workbenchChanged)
    Q_PROPERTY(QString activeView READ activeView NOTIFY workbenchChanged)
    Q_PROPERTY(int sidebarWidth READ sidebarWidth NOTIFY workbenchChanged)

    /// Empty until a project is opened. The UI shows the project name in the top
    /// bar and falls back to a neutral state when there is none, rather than
    /// displaying a placeholder that implies something is open.
    Q_PROPERTY(QString projectName READ projectName NOTIFY projectChanged)

public:
    AppController(core::CommandRegistry& commands,
                  config::Settings& settings,
                  config::AnimationPolicy& animation,
                  QObject* parent = nullptr);

    /// Publishes the application's controller to QML. AppController takes its
    /// dependencies by reference, so the engine cannot construct one itself;
    /// main() builds the object graph and publishes the result here.
    static void setInstance(AppController* instance);

    /// QML singleton factory, returning the published instance. Ownership stays
    /// in C++.
    static AppController* create(QQmlEngine* engine, QJSEngine* scriptEngine);

    [[nodiscard]] int animationDuration() const { return m_animation.duration(); }
    [[nodiscard]] int fastAnimationDuration() const { return m_animation.fastDuration(); }

    [[nodiscard]] bool sidebarVisible() const;
    [[nodiscard]] QString activeView() const;
    [[nodiscard]] int sidebarWidth() const;
    [[nodiscard]] QString projectName() const { return m_projectName; }

    /// Runs a command by id. Returns false and logs if it is unknown or disabled,
    /// so a mis-wired shortcut is visible rather than silently dead.
    Q_INVOKABLE bool invokeCommand(const QString& id);

    /// Selects a sidebar view, or collapses the sidebar if that view is already
    /// showing — the behaviour the design specifies for the activity rail.
    Q_INVOKABLE void selectView(const QString& view);

    Q_INVOKABLE void setSidebarWidth(int width);

signals:
    void animationChanged();
    void workbenchChanged();
    void projectChanged();

private:
    /// Registers the commands the workbench itself owns. Other modules register
    /// their own; nothing here knows about them.
    void registerWorkbenchCommands();

    core::CommandRegistry& m_commands;
    config::Settings& m_settings;
    config::AnimationPolicy& m_animation;
    QString m_projectName;
};

} // namespace keys::ui
