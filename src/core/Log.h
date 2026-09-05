#pragma once

#include <QLoggingCategory>

/// Logging categories, one per module.
///
/// Categories rather than a single log let a developer enable exactly the subsystem
/// they are debugging without drowning in the rest — which matters once filesystem
/// watching and indexing are running continuously. Everything below Warning is off
/// by default so a release build stays quiet and cheap.
///
/// Enable at runtime, e.g.:  QT_LOGGING_RULES="keys.fs.debug=true"

Q_DECLARE_LOGGING_CATEGORY(lcCore)
Q_DECLARE_LOGGING_CATEGORY(lcFs)
Q_DECLARE_LOGGING_CATEGORY(lcProcess)
Q_DECLARE_LOGGING_CATEGORY(lcConfig)
Q_DECLARE_LOGGING_CATEGORY(lcUi)
Q_DECLARE_LOGGING_CATEGORY(lcPerf)

namespace keys::core {

/// Installs the message handler and the default category rules. Called once from
/// main() before anything else logs.
void initializeLogging();

} // namespace keys::core
