#pragma once

#include "config/Settings.h"
#include "core/Result.h"

#include <QString>

namespace keys::config {

/// Reads and writes settings layers as JSON on disk.
///
/// Separated from Settings so the in-memory resolution logic stays testable
/// without touching the filesystem, and so the storage format can change without
/// disturbing every consumer of Settings.
class SettingsStore {
public:
    /// Default location of the user settings file:
    /// %APPDATA%/Keys/settings.json (or the platform equivalent).
    [[nodiscard]] static QString userSettingsPath();

    /// Workspace settings live inside the project: <root>/.keys/settings.json
    [[nodiscard]] static QString workspaceSettingsPath(const QString& projectRoot);

    /// Loads `path` into `layer`. A missing file is not an error — it means the
    /// user has not customised anything yet, which is the normal first-run state.
    static core::Status load(Settings& settings, Settings::Layer layer, const QString& path);

    /// Writes `layer` to `path`, creating parent directories as needed.
    /// The write is atomic: a crash mid-save must not leave settings truncated.
    static core::Status save(const Settings& settings, Settings::Layer layer, const QString& path);
};

} // namespace keys::config
