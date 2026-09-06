#include "ui/UpdateModel.h"

#include "update/UpdateChecker.h"

#include <QDesktopServices>
#include <QQmlEngine>
#include <QUrl>

namespace keys::ui {
namespace {

UpdateModel* g_instance = nullptr;

} // namespace

UpdateModel::UpdateModel(update::UpdateChecker& checker, QObject* parent)
    : QObject(parent), m_checker(checker)
{
    connect(&m_checker, &update::UpdateChecker::stateChanged,
            this, &UpdateModel::changed);

    connect(&m_checker, &update::UpdateChecker::updateFound, this,
            [this](const QString& version, const QString&) {
                emit updateFound(version);
            });

    connect(&m_checker, &update::UpdateChecker::upToDate,
            this, &UpdateModel::upToDate);
    connect(&m_checker, &update::UpdateChecker::checkFailed,
            this, &UpdateModel::checkFailed);
}

bool UpdateModel::updateAvailable() const
{
    return m_checker.updateAvailable();
}

bool UpdateModel::isChecking() const
{
    return m_checker.isChecking();
}

QString UpdateModel::currentVersion() const
{
    return m_checker.currentVersion();
}

QString UpdateModel::availableVersion() const
{
    return m_checker.availableVersion();
}

QString UpdateModel::releaseUrl() const
{
    return m_checker.releaseUrl();
}

bool UpdateModel::checkAutomatically() const
{
    return m_checker.isEnabled();
}

void UpdateModel::setCheckAutomatically(bool enabled)
{
    m_checker.setEnabled(enabled);
}

void UpdateModel::checkNow()
{
    m_checker.checkNow();
}

void UpdateModel::openReleasePage()
{
    const QString url = m_checker.releaseUrl();
    if (!url.isEmpty()) {
        QDesktopServices::openUrl(QUrl(url));
    }
}

void UpdateModel::setInstance(UpdateModel* instance)
{
    g_instance = instance;
}

UpdateModel* UpdateModel::create(QQmlEngine*, QJSEngine*)
{
    Q_ASSERT_X(g_instance, "UpdateModel::create",
               "UpdateModel::setInstance was not called");

    QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
    return g_instance;
}

} // namespace keys::ui
