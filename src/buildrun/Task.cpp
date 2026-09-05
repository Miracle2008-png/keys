#include "buildrun/Task.h"

namespace keys::buildrun {
namespace {

/// Quotes an argument only when it needs it. This is for display, so the rule is
/// "would a person have to quote this", not any particular shell's grammar.
QString displayArgument(const QString& argument)
{
    if (argument.isEmpty()) {
        return QStringLiteral("\"\"");
    }
    if (!argument.contains(QLatin1Char(' '))) {
        return argument;
    }
    return QLatin1Char('"') + argument + QLatin1Char('"');
}

} // namespace

QString Task::commandLine() const
{
    QStringList parts;
    parts.reserve(arguments.size() + 1);
    parts << displayArgument(program);
    for (const QString& argument : arguments) {
        parts << displayArgument(argument);
    }
    return parts.join(QLatin1Char(' '));
}

QString taskKindLabel(TaskKind kind)
{
    switch (kind) {
    case TaskKind::Build:
        return QStringLiteral("Build");
    case TaskKind::Run:
        return QStringLiteral("Run");
    case TaskKind::Test:
        return QStringLiteral("Test");
    case TaskKind::Clean:
        return QStringLiteral("Clean");
    case TaskKind::Custom:
        break;
    }
    return QStringLiteral("Task");
}

std::vector<Task> defaultTasksFor(project::ProjectKind kind)
{
    const auto task = [](const char* name, TaskKind taskKind, const char* program,
                         const QStringList& arguments) {
        Task result;
        result.name = QString::fromLatin1(name);
        result.kind = taskKind;
        result.program = QString::fromLatin1(program);
        result.arguments = arguments;
        return result;
    };

    switch (kind) {
    case project::ProjectKind::Rust:
        // Cargo is the whole toolchain for a Rust project, so these are safe:
        // there is one conventional way to build, run and test.
        return {
            task("Build", TaskKind::Build, "cargo", {QStringLiteral("build")}),
            task("Run", TaskKind::Run, "cargo", {QStringLiteral("run")}),
            task("Test", TaskKind::Test, "cargo", {QStringLiteral("test")}),
            task("Clean", TaskKind::Clean, "cargo", {QStringLiteral("clean")}),
        };

    case project::ProjectKind::Go:
        return {
            task("Build", TaskKind::Build, "go", {QStringLiteral("build"), QStringLiteral("./...")}),
            task("Run", TaskKind::Run, "go", {QStringLiteral("run"), QStringLiteral(".")}),
            task("Test", TaskKind::Test, "go", {QStringLiteral("test"), QStringLiteral("./...")}),
        };

    case project::ProjectKind::Node:
        // npm scripts are conventional but not guaranteed: a package.json need
        // not define "build" or "test". These are offered because running them
        // fails loudly and legibly ("missing script: build") rather than doing
        // something unexpected.
        return {
            task("Install", TaskKind::Custom, "npm", {QStringLiteral("install")}),
            task("Build", TaskKind::Build, "npm", {QStringLiteral("run"), QStringLiteral("build")}),
            task("Start", TaskKind::Run, "npm", {QStringLiteral("start")}),
            task("Test", TaskKind::Test, "npm", {QStringLiteral("test")}),
        };

    case project::ProjectKind::Cpp:
        // Configure and build separately: a build against a missing build
        // directory fails with a clear message, and re-configuring on every
        // build would be slow and occasionally destructive.
        return {
            task("Configure", TaskKind::Custom, "cmake",
                 {QStringLiteral("-S"), QStringLiteral("."), QStringLiteral("-B"),
                  QStringLiteral("build")}),
            task("Build", TaskKind::Build, "cmake",
                 {QStringLiteral("--build"), QStringLiteral("build")}),
            task("Test", TaskKind::Test, "ctest",
                 {QStringLiteral("--test-dir"), QStringLiteral("build"),
                  QStringLiteral("--output-on-failure")}),
        };

    case project::ProjectKind::Python:
        // No build or run default. There is no conventional entry point for a
        // Python project - it might be a module, a script, a package or an
        // application - and a default that guessed would fail confusingly.
        // Testing is the one thing that is conventional.
        return {
            task("Test", TaskKind::Test, "python",
                 {QStringLiteral("-m"), QStringLiteral("pytest")}),
        };

    case project::ProjectKind::Unknown:
        break;
    }

    // Nothing recognised: the UI says so rather than offering a command that
    // cannot work.
    return {};
}

} // namespace keys::buildrun
