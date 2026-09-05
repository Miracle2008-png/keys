#pragma once

#include <QElapsedTimer>
#include <QString>

namespace keys::core {

/// Scoped timer that reports elapsed milliseconds to the "keys.perf" category.
///
/// The architecture sets performance budgets (docs/ARCHITECTURE.md §8) and requires
/// optimisation to target measured bottlenecks rather than guesses. That only works
/// if measurement exists from the start, so startup and other budgeted paths are
/// instrumented as they are written, not retrofitted at milestone 15.
///
/// Cost when the category is disabled is one atomic load, so instrumentation can
/// stay in release builds.
///
///     { KEYS_TRACE("openProject"); ... }   // logs "openProject took 12 ms"
class ScopedTrace {
public:
    explicit ScopedTrace(QString label);
    ~ScopedTrace();

    ScopedTrace(const ScopedTrace&) = delete;
    ScopedTrace& operator=(const ScopedTrace&) = delete;

    /// Elapsed time so far, for callers that want the number rather than a log line.
    [[nodiscard]] qint64 elapsedMs() const { return m_timer.elapsed(); }

private:
    QString m_label;
    QElapsedTimer m_timer;
    bool m_enabled;
};

} // namespace keys::core

#define KEYS_TRACE_CONCAT_(a, b) a##b
#define KEYS_TRACE_CONCAT(a, b) KEYS_TRACE_CONCAT_(a, b)
#define KEYS_TRACE(label) \
    ::keys::core::ScopedTrace KEYS_TRACE_CONCAT(keysTrace_, __LINE__) { QStringLiteral(label) }
