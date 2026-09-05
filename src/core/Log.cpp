#include "core/Log.h"

// Categories default to Warning and above. Debug output is opt-in via
// QT_LOGGING_RULES so neither release builds nor ordinary development runs pay for
// logging nobody is reading.
Q_LOGGING_CATEGORY(lcCore, "keys.core", QtWarningMsg)
Q_LOGGING_CATEGORY(lcFs, "keys.fs", QtWarningMsg)
Q_LOGGING_CATEGORY(lcProcess, "keys.process", QtWarningMsg)
Q_LOGGING_CATEGORY(lcConfig, "keys.config", QtWarningMsg)
Q_LOGGING_CATEGORY(lcUi, "keys.ui", QtWarningMsg)
Q_LOGGING_CATEGORY(lcPerf, "keys.perf", QtWarningMsg)

namespace keys::core {

void initializeLogging()
{
    // Category, level and message. No timestamp: the console adds one, and log
    // files are not a milestone-1 concern.
    qSetMessagePattern(QStringLiteral(
        "[%{category}] "
        "%{if-debug}D%{endif}%{if-info}I%{endif}%{if-warning}W%{endif}"
        "%{if-critical}E%{endif}%{if-fatal}F%{endif} "
        "%{message}"));
}

} // namespace keys::core
