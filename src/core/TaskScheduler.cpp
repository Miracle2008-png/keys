#include "core/TaskScheduler.h"

#include <QRunnable>

#include <algorithm>
#include <thread>

namespace keys::core {
namespace {

class FunctionTask : public QRunnable {
public:
    explicit FunctionTask(std::function<void()> work) : m_work(std::move(work))
    {
        setAutoDelete(true);
    }

    void run() override
    {
        if (m_work) {
            m_work();
        }
    }

private:
    std::function<void()> m_work;
};

} // namespace

TaskScheduler::TaskScheduler(QObject* parent)
    : QObject(parent), m_pool(std::make_unique<QThreadPool>())
{
    // Leave one core for the UI thread so a saturated pool cannot cause the
    // stutter this class exists to prevent. Single-core machines still get one
    // worker; blocking the UI is worse than contending for the core.
    const auto hardware = static_cast<int>(std::thread::hardware_concurrency());
    m_pool->setMaxThreadCount(std::max(1, hardware - 1));

    // Workers are cheap to keep warm relative to the latency of creating one when
    // the user is waiting.
    m_pool->setExpiryTimeout(30'000);
    m_pool->setObjectName(QStringLiteral("keys-workers"));
}

TaskScheduler::~TaskScheduler()
{
    // Tasks capture application state; letting them run past teardown would use
    // freed objects. Block here so destruction is ordered.
    m_pool->waitForDone();
}

void TaskScheduler::post(std::function<void()> work, Priority priority)
{
    if (!work) {
        return;
    }
    m_pool->start(new FunctionTask(std::move(work)), static_cast<int>(priority));
}

void TaskScheduler::waitForDone()
{
    m_pool->waitForDone();
}

int TaskScheduler::maxThreadCount() const
{
    return m_pool->maxThreadCount();
}

int TaskScheduler::activeThreadCount() const
{
    return m_pool->activeThreadCount();
}

} // namespace keys::core
