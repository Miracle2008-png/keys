#pragma once

#include "project/Project.h"

#include <QString>
#include <QStringList>

#include <vector>

namespace keys::buildrun {

/// What a task is for. Decides which button runs it and what a failure means.
enum class TaskKind {
    Build,   ///< produces artifacts; failures are the point of the problem list
    Run,     ///< starts the thing that was built
    Test,
    Clean,
    Custom,
};

/// One runnable command.
///
/// Deliberately a command line and not a language integration. Keys does not
/// know how to build C++ or Node; it knows how to run `cmake --build build` and
/// show what comes back. That is what keeps `buildrun` free of per-language
/// special cases, and what lets a project with an unusual build work by editing
/// one line rather than waiting for Keys to support it.
struct Task {
    QString name;          ///< shown in the picker: "Build", "Run tests"
    QString program;
    QStringList arguments;

    /// Relative to the project root, or empty for the root itself. A CMake build
    /// runs from the project root while a Node script runs from wherever its
    /// package.json is, so this cannot be assumed.
    QString workingDirectory;

    TaskKind kind = TaskKind::Custom;

    /// Extra environment entries as `KEY=value`, applied over the inherited
    /// environment.
    QStringList environment;

    [[nodiscard]] bool isValid() const { return !name.isEmpty() && !program.isEmpty(); }

    /// The command as a person would type it, for the console's first line. Not
    /// used to execute anything — arguments are passed as a list, so nothing
    /// here has to survive a shell.
    [[nodiscard]] QString commandLine() const;
};

/// The default tasks for a project kind.
///
/// **Defaults, not detection.** These are the commands that work for a
/// conventional project of that kind. A project that does something else needs
/// its own tasks, and Keys does not try to infer them by reading build files —
/// guessing wrong produces a task that fails confusingly, which is worse than
/// offering nothing.
///
/// Returns an empty list for a kind Keys has no sensible default for, and the
/// UI then says there are no tasks rather than showing one that cannot work.
[[nodiscard]] std::vector<Task> defaultTasksFor(project::ProjectKind kind);

/// Human label for a kind, for the UI.
[[nodiscard]] QString taskKindLabel(TaskKind kind);

} // namespace keys::buildrun
