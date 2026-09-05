#pragma once

#include <QObject>
#include <QThreadPool>

#include <functional>
#include <memory>
#include <utility>

namespace keys::core {

/// The application's worker pool.
///
/// Architecture rule: the UI thread does layout, input and paint, nothing else.
/// Every operation with an unbounded or unpredictable cost — search, indexing, git
/// status, file enumeration — runs here instead.
///
/// The pool is deliberately sized to hardware concurrency minus one so a saturated
/// pool cannot starve the UI thread of a core. An IDE that stutters while indexing
/// has failed at its main job.
class TaskScheduler : public QObject {
    Q_OBJECT

public:
    /// Priority is advisory: it orders queued work, it does not preempt running
    /// work. Interactive work (a keystroke-driven completion) must not queue behind
    /// a project-wide index.
    enum class Priority {
        Background = 0,   ///< indexing, warming caches
        Normal = 1,       ///< explicit user actions with visible results
        Interactive = 2,  ///< blocking something the user is waiting on right now
    };

    explicit TaskScheduler(QObject* parent = nullptr);
    ~TaskScheduler() override;

    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;

    /// Run `work` on the pool. Fire-and-forget: use the completion overload or a
    /// queued signal to get results back to the UI thread.
    void post(std::function<void()> work, Priority priority = Priority::Normal);

    /// Run `work` on the pool, then invoke `onComplete` with its result on the
    /// thread that owns `receiver`. This is the safe way to cross back to the UI:
    /// if `receiver` is destroyed before the work finishes, the completion is
    /// dropped rather than called on a dangling object.
    template <typename T>
    void postWithResult(QObject* receiver,
                        std::function<T()> work,
                        std::function<void(T)> onComplete,
                        Priority priority = Priority::Normal)
    {
        // The guard is a QObject living on the receiver's thread. Deleting it with
        // the receiver is what makes the late-completion case safe.
        auto* guard = new QObject(receiver);
        post(
            [work = std::move(work), onComplete = std::move(onComplete), guard]() mutable {
                T result = work();
                QMetaObject::invokeMethod(
                    guard,
                    [onComplete = std::move(onComplete), result = std::move(result)]() mutable {
                        onComplete(std::move(result));
                    },
                    Qt::QueuedConnection);
            },
            priority);
    }

    /// Block until every queued and running task finishes. Shutdown only — calling
    /// this from the UI thread during normal operation is exactly the stall this
    /// class exists to prevent.
    void waitForDone();

    [[nodiscard]] int maxThreadCount() const;
    [[nodiscard]] int activeThreadCount() const;

private:
    std::unique_ptr<QThreadPool> m_pool;
};

} // namespace keys::core
