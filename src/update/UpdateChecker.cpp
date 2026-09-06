#include "update/UpdateChecker.h"

#include "config/Settings.h"
#include "core/Log.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <algorithm>
#include <tuple>

namespace keys::update {
namespace {

/// The releases endpoint for the project. Only the latest is asked for, so the
/// reply is one small object rather than the whole release history.
constexpr auto kReleasesUrl =
    "https://api.github.com/repos/Miracle2008-png/keys/releases/latest";

constexpr auto kEnabledKey = "updates.checkAutomatically";
constexpr auto kLastCheckKey = "updates.lastCheck";

/// Once a day. Often enough that a release is noticed within a working day,
/// rare enough that it is never something the user perceives.
constexpr int kCheckIntervalHours = 24;

/// A tag may be `v0.2.0` or `0.2.0`; both mean the same thing.
QString stripTagPrefix(const QString& tag)
{
    return tag.startsWith(QLatin1Char('v')) ? tag.mid(1) : tag;
}

} // namespace

UpdateChecker::UpdateChecker(config::Settings& settings, QObject* parent)
    : QObject(parent), m_settings(settings)
{
}

UpdateChecker::~UpdateChecker() = default;

QString UpdateChecker::currentVersion() const
{
    return QStringLiteral(KEYS_VERSION);
}

bool UpdateChecker::isEnabled() const
{
    // On by default, per the schema. An editor that never mentions a security
    // fix because the check defaulted to off is not being respectful, it is
    // being useless - and it is one request a day the user can switch off.
    return m_settings.boolValue(QLatin1String(kEnabledKey));
}

void UpdateChecker::setEnabled(bool enabled)
{
    if (const core::Status status =
            m_settings.setValue(QLatin1String(kEnabledKey), enabled);
        !status) {
        qCWarning(lcCore) << "could not save the update setting:"
                          << status.error().toString();
    }
    emit stateChanged();
}

bool UpdateChecker::updateAvailable() const
{
    return !m_availableVersion.isEmpty()
           && isNewer(m_availableVersion, currentVersion());
}

void UpdateChecker::checkIfDue()
{
    if (!isEnabled()) {
        return;   // off means no request, not a request whose answer is ignored
    }

    const QString last = m_settings.stringValue(QLatin1String(kLastCheckKey));
    if (!last.isEmpty()) {
        const QDateTime when = QDateTime::fromString(last, Qt::ISODate);
        if (when.isValid()
            && when.secsTo(QDateTime::currentDateTime()) < kCheckIntervalHours * 3600) {
            return;
        }
    }

    performCheck(false);
}

void UpdateChecker::checkNow()
{
    // Runs even when automatic checking is off: the user asked directly, and
    // refusing because of a background setting would be obtuse.
    performCheck(true);
}

void UpdateChecker::performCheck(bool userInitiated)
{
    if (m_checking) {
        return;
    }

    if (!m_network) {
        m_network = std::make_unique<QNetworkAccessManager>();
    }

    m_checking = true;
    emit stateChanged();

    QNetworkRequest request{QUrl(QLatin1String(kReleasesUrl))};
    request.setRawHeader("Accept", "application/vnd.github+json");

    // GitHub rejects requests with no user agent. Naming the product and its
    // version is what the API asks for and is what any HTTP client should do.
    request.setRawHeader("User-Agent",
                         QStringLiteral("Keys/%1").arg(currentVersion()).toUtf8());

    QNetworkReply* reply = m_network->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, userInitiated] {
        reply->deleteLater();
        m_checking = false;

        // The timestamp is written whether or not the check succeeded, so a
        // machine that is offline does not retry on every launch.
        std::ignore = m_settings.setValue(
            QLatin1String(kLastCheckKey),
            QDateTime::currentDateTime().toString(Qt::ISODate));

        if (reply->error() != QNetworkReply::NoError) {
            qCDebug(lcCore) << "update check failed:" << reply->errorString();
            if (userInitiated) {
                emit checkFailed(reply->errorString());
            }
            emit stateChanged();
            return;
        }

        handleReply(reply->readAll(), userInitiated);
        emit stateChanged();
    });
}

void UpdateChecker::handleReply(const QByteArray& body, bool userInitiated)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        if (userInitiated) {
            emit checkFailed(tr("The release information could not be read."));
        }
        return;
    }

    const QJsonObject release = document.object();

    // A draft or a prerelease is not something to offer: it is published for
    // people who went looking for it, not for everyone on the stable build.
    if (release.value(QStringLiteral("draft")).toBool()
        || release.value(QStringLiteral("prerelease")).toBool()) {
        if (userInitiated) {
            emit upToDate();
        }
        return;
    }

    m_availableVersion =
        stripTagPrefix(release.value(QStringLiteral("tag_name")).toString());
    m_releaseUrl = release.value(QStringLiteral("html_url")).toString();

    if (updateAvailable()) {
        emit updateFound(m_availableVersion, m_releaseUrl);
    } else if (userInitiated) {
        // Only when asked. A daily check that announces "up to date" is an
        // interruption that carries no information.
        emit upToDate();
    }
}

bool UpdateChecker::isNewer(const QString& candidate, const QString& current)
{
    const QStringList a = candidate.split(QLatin1Char('.'));
    const QStringList b = current.split(QLatin1Char('.'));

    // Compared as numbers, not as text: "0.10.0" is newer than "0.9.0", and a
    // string comparison says the opposite.
    for (int i = 0; i < std::max(a.size(), b.size()); ++i) {
        const int left = i < a.size() ? a.at(i).toInt() : 0;
        const int right = i < b.size() ? b.at(i).toInt() : 0;
        if (left != right) {
            return left > right;
        }
    }
    return false;
}

} // namespace keys::update
