#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QtQml/qqmlregistration.h>

namespace keys::update {
class UpdateChecker;
}

namespace keys::ui {

/// The update checker, as QML sees it.
///
/// A thin adapter rather than logic: the checker knows about releases and the
/// network, this knows about properties and signals. Keeping the two apart is
/// what lets the checker be tested without a QML engine.
class UpdateModel : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Updates)
    QML_SINGLETON

    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY changed)
    Q_PROPERTY(bool checking READ isChecking NOTIFY changed)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString availableVersion READ availableVersion NOTIFY changed)
    Q_PROPERTY(QString releaseUrl READ releaseUrl NOTIFY changed)

    /// Whether the daily check runs at all. Off means no request is made.
    Q_PROPERTY(bool checkAutomatically READ checkAutomatically
                   WRITE setCheckAutomatically NOTIFY changed)

public:
    explicit UpdateModel(update::UpdateChecker& checker, QObject* parent = nullptr);

    [[nodiscard]] bool updateAvailable() const;
    [[nodiscard]] bool isChecking() const;
    [[nodiscard]] QString currentVersion() const;
    [[nodiscard]] QString availableVersion() const;
    [[nodiscard]] QString releaseUrl() const;
    [[nodiscard]] bool checkAutomatically() const;

    void setCheckAutomatically(bool enabled);

    /// Checks now and reports either way, because the user asked.
    Q_INVOKABLE void checkNow();

    /// Opens the release page in the browser. Keys does not download or run
    /// anything itself - that needs signature checking and a recovery path,
    /// and neither is worth building before anyone has asked for it.
    Q_INVOKABLE void openReleasePage();

    static void setInstance(UpdateModel* instance);
    static UpdateModel* create(QQmlEngine*, QJSEngine*);

signals:
    void changed();

    /// A newer release exists. The window shows a notice; nothing is forced.
    void updateFound(const QString& version);

    /// A user-initiated check found nothing, or could not complete.
    void upToDate();
    void checkFailed(const QString& reason);

private:
    update::UpdateChecker& m_checker;
};

} // namespace keys::ui
