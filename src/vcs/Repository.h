#pragma once

#include "core/Result.h"
#include "core/TaskScheduler.h"
#include "vcs/GitTypes.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <functional>
#include <memory>
#include <vector>

namespace keys::vcs {

class GitClient;

/// The open repository, refreshed in the background.
///
/// **Nothing blocks the UI.** Every git invocation costs a process launch — a
/// few milliseconds at best, much more on a cold cache or a large tree — so all
/// of them run on the scheduler and report back through signals. The UI reads
/// the last known status, which is always immediately available.
///
/// **Refresh is debounced, not polled.** A build touching two thousand files
/// would otherwise queue two thousand status runs. Requests inside the debounce
/// window collapse into one, and a refresh already running is not duplicated;
/// instead the request is remembered and re-run once, so the final state is
/// never stale.
class Repository : public QObject {
    Q_OBJECT

public:
    Repository(core::TaskScheduler& scheduler, QObject* parent = nullptr);
    ~Repository() override;

    /// Points the repository at whatever encloses `projectRoot`, or closes it if
    /// nothing does. A project without git is the normal case, not a failure.
    void openFor(const QString& projectRoot);
    void close();

    [[nodiscard]] bool isOpen() const { return m_client != nullptr; }
    [[nodiscard]] QString root() const;

    /// The last status read. Empty and clean until the first refresh completes,
    /// so the UI has something coherent to draw immediately.
    [[nodiscard]] const RepositoryStatus& status() const { return m_status; }

    [[nodiscard]] bool isRefreshing() const { return m_refreshing; }

    /// Whether git could be found at all. False makes the source-control view
    /// say so once, rather than every action failing separately.
    [[nodiscard]] static bool isGitAvailable();

    /// Asks for a refresh. Cheap to call often — that is the point.
    void refresh();

    // ---- Actions -----------------------------------------------------------
    //
    // Each runs on the scheduler and refreshes on completion. They report
    // failure through actionFailed rather than returning a status, because the
    // caller is a click handler that has nothing useful to do with one.

    void stage(const QStringList& paths);
    void unstage(const QStringList& paths);
    void discard(const QStringList& paths);
    void commit(const QString& message);

    /// Fetches the diff for one path. Asynchronous, so the result arrives on
    /// diffReady rather than as a return value.
    void requestDiff(const QString& path, bool staged);

    /// Recent history, newest first. Populated by refresh().
    [[nodiscard]] const std::vector<Commit>& history() const { return m_history; }

signals:
    /// The repository was opened, closed, or pointed somewhere else.
    void repositoryChanged();

    /// A refresh finished and status() now holds something new.
    void statusChanged();

    void refreshingChanged();
    void historyChanged();

    void diffReady(const QString& path, bool staged, const QString& diff);

    /// An action failed, with git's own message. The UI shows this; nothing
    /// retries automatically, because a failed git command usually needs a
    /// person to decide what to do.
    void actionFailed(const QString& operation, const QString& message);

private:
    /// Runs `work` on the scheduler and refreshes afterwards, reporting any
    /// failure as `operation`. Every action shares this so none of them can
    /// forget to refresh or to report.
    void runAction(const QString& operation,
                   std::function<core::Status(const GitClient&)> work);

    void startRefresh();

    core::TaskScheduler& m_scheduler;
    /// Shared rather than unique: a refresh runs on a worker that holds the
    /// client for the length of the command, and close() can happen while one
    /// is in flight. A raw pointer captured into the task would dangle the
    /// moment the repository was closed - the generation guard discards a stale
    /// *result*, but the work itself is already running and must stay valid.
    std::shared_ptr<GitClient> m_client;

    RepositoryStatus m_status;
    std::vector<Commit> m_history;

    /// Collapses a burst of change notifications into one refresh.
    QTimer m_debounce;

    bool m_refreshing = false;

    /// Set when a refresh is asked for while one is running. The running one
    /// cannot include changes made after it started, so a second is queued
    /// rather than the request being dropped.
    bool m_refreshQueued = false;

    /// Incremented whenever the repository changes. A completion carrying a
    /// stale generation is discarded, so a slow status on a closed project
    /// cannot deliver its results into the next one.
    quint64 m_generation = 0;

    /// How long to wait for changes to settle. Long enough that a save followed
    /// by a formatter is one refresh, short enough to feel immediate.
    static constexpr int kDebounceMs = 250;

    /// The history the source-control view shows; more than fits on screen, far
    /// less than a full log.
    static constexpr int kHistoryLimit = 50;
};

} // namespace keys::vcs
