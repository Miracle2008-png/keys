#pragma once

#include <QObject>
#include <QThread>
#include <QThreadPool>

#include <atomic>

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
    ///
    /// `work` itself is not cancelled — it runs to completion on the worker
    /// regardless, so anything it captures by pointer must outlive it. A task
    /// that needs a member of the receiver should capture a shared_ptr to it,
    /// not a raw pointer. Only the delivery of the result is guarded.
    template <typename T>
    void postWithResult(QObject* receiver,
                        std::function<T()> work,
                        std::function<void(T)> onComplete,
                        Priority priority = Priority::Normal)
    {
        // Two pieces, because one is not enough.
        //
        // The obvious implementation - keep a pointer to the receiver and call
        // invokeMethod on it when the work finishes - is a use-after-free. The
        // receiver can be destroyed in the window between the worker deciding to
        // deliver and invokeMethod reading the object to find its thread. A
        // QPointer narrows that window but cannot close it: the check and the
        // call are not atomic across threads.
        //
        // So delivery targets the receiver's *thread*, which outlives the objects
        // affine to it. Whether to actually call `onComplete` is then decided by
        // a flag held in a shared_ptr - cleared on the receiver's thread when it
        // dies, and read on that same thread immediately before the call, so the
        // two cannot interleave.
        auto alive = std::make_shared<std::atomic_bool>(true);

        // Cleared when the receiver dies. Connected on the receiver's thread, so
        // the write and the read below are ordered by that thread's event loop.
        QObject::connect(receiver, &QObject::destroyed, receiver,
                         [alive] { alive->store(false); });

        QThread* thread = receiver->thread();

        post(
            [work = std::move(work), onComplete = std::move(onComplete), alive,
             thread]() mutable {
                T result = work();

                if (!alive->load()) {
                    return;
                }

                // Targeting the thread rather than the receiver is what makes
                // this safe: the QThread is still alive here, so resolving the
                // target cannot touch destroyed memory.
                QMetaObject::invokeMethod(
                    thread,
                    [alive, onComplete = std::move(onComplete),
                     result = std::move(result)]() mutable {
                        if (alive->load()) {
                            onComplete(std::move(result));
                        }
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
