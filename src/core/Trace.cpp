#include "core/Trace.h"

#include "core/Log.h"

namespace keys::core {

ScopedTrace::ScopedTrace(QString label)
    : m_label(std::move(label)), m_enabled(lcPerf().isDebugEnabled())
{
    // Skip the timer entirely when nobody is listening, so instrumentation can
    // stay compiled into release builds without costing anything.
    if (m_enabled) {
        m_timer.start();
    }
}

ScopedTrace::~ScopedTrace()
{
    if (m_enabled) {
        qCDebug(lcPerf).noquote() << m_label << "took" << m_timer.elapsed() << "ms";
    }
}

} // namespace keys::core
