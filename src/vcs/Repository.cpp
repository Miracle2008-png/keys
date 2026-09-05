#include "vcs/Repository.h"

#include "core/Log.h"
#include "vcs/GitClient.h"

namespace keys::vcs {
namespace {

/// The result of one background refresh. Carried as a unit so the completion
/// applies status and history together — applying them separately would let the
/// UI draw a commit list from before a commit alongside a status from after it.
struct RefreshResult {
    bool ok = false;
    QString error;
    RepositoryStatus status;
    std::vector<Commit> history;
};

} // namespace

Repository::Repository(core::TaskScheduler& scheduler, QObject* parent)
    : QObject(parent), m_scheduler(scheduler)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(kDebounceMs);
    connect(&m_debounce, &QTimer::timeout, this, &Repository::startRefresh);
}

Repository::~Repository() = default;

bool Repository::isGitAvailable()
{
    // Cached: this launches a process, and the answer cannot change while Keys
    // is running in any way that matters.
    static const bool available = GitClient::isGitAvailable();
    return available;
}

QString Repository::root() const
{
    return m_client ? m_client->root() : QString();
}

void Repository::openFor(const QString& projectRoot)
{
    close();

    if (projectRoot.isEmpty() || !isGitAvailable()) {
        return;
    }

    const core::Result<QString> found = GitClient::findRepositoryRoot(projectRoot);
    if (!found) {
        // A project that is not a repository is ordinary, so this is logged at
        // debug level and the view simply says there is no source control.
        qCDebug(lcCore) << "no git repository for" << projectRoot;
        emit repositoryChanged();
        return;
    }

    m_client = std::make_shared<GitClient>(found.value());
    emit repositoryChanged();

    refresh();
}

void Repository::close()
{
    m_debounce.stop();

    // Bumping the generation is what makes an in-flight refresh harmless: its
    // completion will see a stale generation and drop its results.
    ++m_generation;
    m_refreshQueued = false;

    const bool wasOpen = m_client != nullptr;
    m_client.reset();

    m_status = RepositoryStatus{};
    m_history.clear();

    if (m_refreshing) {
        m_refreshing = false;
        emit refreshingChanged();
    }

    if (wasOpen) {
        emit repositoryChanged();
        emit statusChanged();
        emit historyChanged();
    }
}

void Repository::refresh()
{
    if (!m_client) {
        return;
    }
    m_debounce.start();
}

void Repository::startRefresh()
{
    if (!m_client) {
        return;
    }

    if (m_refreshing) {
        // A refresh in flight cannot see changes made after it started, so the
        // request is remembered rather than dropped.
        m_refreshQueued = true;
        return;
    }

    m_refreshing = true;
    emit refreshingChanged();

    const quint64 generation = m_generation;
    const std::shared_ptr<GitClient> client = m_client;

    m_scheduler.postWithResult<RefreshResult>(
        this,
        [client] {
            RefreshResult result;

            const core::Result<RepositoryStatus> status = client->status();
            if (!status) {
                result.error = status.error().message();
                return result;
            }
            result.status = status.value();

            // A history failure is not a refresh failure: a repository with no
            // commits yet has a perfectly good status and no log at all.
            if (const core::Result<std::vector<Commit>> history =
                    client->log(kHistoryLimit)) {
                result.history = history.value();
            }

            result.ok = true;
            return result;
        },
        [this, generation](RefreshResult result) {
            if (generation != m_generation) {
                return;   // the repository changed while this was running
            }

            m_refreshing = false;
            emit refreshingChanged();

            if (!result.ok) {
                qCWarning(lcCore) << "git status failed:" << result.error;
                emit actionFailed(QStringLiteral("status"), result.error);
            } else {
                m_status = std::move(result.status);
                m_history = std::move(result.history);
                emit statusChanged();
                emit historyChanged();
            }

            if (m_refreshQueued) {
                m_refreshQueued = false;
                startRefresh();
            }
        },
        core::TaskScheduler::Priority::Background);
}

void Repository::runAction(const QString& operation,
                           std::function<core::Status(const GitClient&)> work)
{
    if (!m_client) {
        return;
    }

    const quint64 generation = m_generation;
    const std::shared_ptr<GitClient> client = m_client;

    m_scheduler.postWithResult<QString>(
        this,
        [client, work = std::move(work)]() -> QString {
            const core::Status status = work(*client);
            // An empty string means success; carrying the message rather than a
            // Status keeps the completion trivially copyable.
            return status ? QString() : status.error().message();
        },
        [this, generation, operation](QString error) {
            if (generation != m_generation) {
                return;
            }
            if (!error.isEmpty()) {
                qCWarning(lcCore) << operation << "failed:" << error;
                emit actionFailed(operation, error);
            }
            // Refreshed either way: a partly-applied failure still changed the
            // repository, and showing stale state after a failure is worse than
            // the failure.
            startRefresh();
        },
        core::TaskScheduler::Priority::Normal);
}

void Repository::stage(const QStringList& paths)
{
    runAction(QStringLiteral("stage"),
              [paths](const GitClient& client) { return client.stage(paths); });
}

void Repository::unstage(const QStringList& paths)
{
    runAction(QStringLiteral("unstage"),
              [paths](const GitClient& client) { return client.unstage(paths); });
}

void Repository::discard(const QStringList& paths)
{
    runAction(QStringLiteral("discard"),
              [paths](const GitClient& client) { return client.discard(paths); });
}

void Repository::commit(const QString& message)
{
    runAction(QStringLiteral("commit"),
              [message](const GitClient& client) { return client.commit(message); });
}

void Repository::requestDiff(const QString& path, bool staged)
{
    if (!m_client) {
        return;
    }

    const quint64 generation = m_generation;
    const std::shared_ptr<GitClient> client = m_client;

    m_scheduler.postWithResult<QString>(
        this,
        [client, path, staged] {
            const core::Result<QString> diff = client->diff(path, staged);
            return diff ? diff.value() : QString();
        },
        [this, generation, path, staged](QString diff) {
            if (generation != m_generation) {
                return;
            }
            emit diffReady(path, staged, diff);
        },
        core::TaskScheduler::Priority::Interactive);
}

} // namespace keys::vcs
