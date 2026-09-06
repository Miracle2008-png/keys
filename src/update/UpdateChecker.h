#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>

#include <memory>

class QNetworkAccessManager;

namespace keys::config {
class Settings;
}

namespace keys::update {

/// Asks GitHub whether a newer release exists.
///
/// **Quiet by default.** The check runs once a day, in the background, and says
/// nothing when there is nothing to say. An editor that interrupts to announce
/// it is up to date has misunderstood what the user is doing. This is the model
/// VS Code and every well-behaved desktop application uses.
///
/// **It reports, it does not act.** Keys tells the user a release exists and
/// links to it. Downloading and running an executable fetched over the network
/// is a different order of risk - it needs signature checking, a trusted
/// channel, and a recovery path when the replacement fails - and none of that
/// is worth building before anyone has asked for it.
///
/// **Opt-out, and honest about the network.** The check is the only outbound
/// request Keys makes. It can be turned off in settings, and when it is, no
/// request is made at all rather than being made and discarded.
class UpdateChecker : public QObject {
    Q_OBJECT

public:
    explicit UpdateChecker(config::Settings& settings, QObject* parent = nullptr);
    ~UpdateChecker() override;

    /// The version this build reports, from KEYS_VERSION.
    [[nodiscard]] QString currentVersion() const;

    /// The newest release seen, or empty if none has been found.
    [[nodiscard]] QString availableVersion() const { return m_availableVersion; }
    [[nodiscard]] QString releaseUrl() const { return m_releaseUrl; }

    /// Whether the newest release is newer than this build.
    [[nodiscard]] bool updateAvailable() const;

    /// Whether a check is in flight, so the UI can say "checking…" rather than
    /// appearing to have ignored the request.
    [[nodiscard]] bool isChecking() const { return m_checking; }

    /// Whether checking is enabled at all. Off means no request is made.
    [[nodiscard]] bool isEnabled() const;
    void setEnabled(bool enabled);

    /// Checks if it has not been checked today. Called at startup.
    void checkIfDue();

    /// Checks now, regardless of when the last one was. What Help → Check for
    /// Updates does, and it reports "you are up to date" because the user
    /// asked - an explicit request deserves an explicit answer.
    void checkNow();

signals:
    void stateChanged();

    /// A newer release exists. Emitted once per check, never repeatedly.
    void updateFound(const QString& version, const QString& url);

    /// A user-initiated check found nothing. Not emitted for the daily one,
    /// which stays silent.
    void upToDate();

    /// The check could not complete. Reported only for a user-initiated check:
    /// a background check that fails because the laptop is on a train is not
    /// something to interrupt anyone about.
    void checkFailed(const QString& reason);

private:
    void performCheck(bool userInitiated);
    void handleReply(const QByteArray& body, bool userInitiated);

    /// Compares two `x.y.z` strings. Returns true when `candidate` is newer.
    [[nodiscard]] static bool isNewer(const QString& candidate, const QString& current);

    config::Settings& m_settings;
    std::unique_ptr<QNetworkAccessManager> m_network;

    QString m_availableVersion;
    QString m_releaseUrl;
    bool m_checking = false;
};

} // namespace keys::update
