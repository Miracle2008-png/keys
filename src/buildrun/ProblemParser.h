#pragma once

#include <QRegularExpression>
#include <QString>

#include <optional>
#include <vector>

namespace keys::buildrun {

/// How bad a reported problem is.
enum class ProblemSeverity {
    Error,
    Warning,
    Note,   ///< a compiler's "note:" lines, which explain the error above them
};

/// One diagnostic pulled out of build output.
struct Problem {
    /// Absolute where the compiler gave one, otherwise relative to the task's
    /// working directory. Resolved by the caller, which knows that directory.
    QString file;

    /// One-based, matching what every compiler prints. Zero means the problem
    /// names a file but no position.
    int line = 0;
    int column = 0;

    ProblemSeverity severity = ProblemSeverity::Error;
    QString message;

    /// The output line this came from, so the console can highlight it and the
    /// list can fall back to it if the parsed message is unhelpful.
    QString rawLine;

    [[nodiscard]] bool hasPosition() const { return line > 0; }
};

/// Turns compiler output into problems.
///
/// **Regex over a handful of known formats.** Compilers do not agree on a
/// diagnostic format and none of them offers a machine-readable one that is
/// universally available, so this matches the shapes the common toolchains
/// actually emit: MSVC, GCC/Clang, and the two that wrap them (MSBuild, Ninja
/// pass them through unchanged).
///
/// **Unmatched lines are not problems.** A line that does not look like a
/// diagnostic is left alone rather than guessed at. A wrong entry in the problem
/// list sends the user to the wrong place, which costs more than a missing entry
/// they can still read in the console.
///
/// **Stateless per line.** Each line is parsed independently, so output can be
/// parsed as it streams rather than after the build finishes.
class ProblemParser {
public:
    /// Parses one line, returning nothing when it is not a diagnostic.
    [[nodiscard]] static std::optional<Problem> parseLine(const QString& line);

    /// Parses a whole block, for tests and for pasted output.
    [[nodiscard]] static std::vector<Problem> parse(const QStringList& lines);

    [[nodiscard]] static QString severityLabel(ProblemSeverity severity);

private:
    /// MSVC: `path\file.cpp(120,7): error C2065: 'x': undeclared identifier`
    /// The column is optional; older MSVC and some tools emit only the line.
    [[nodiscard]] static const QRegularExpression& msvcPattern();

    /// GCC and Clang: `path/file.cpp:120:7: error: expected ';'`
    [[nodiscard]] static const QRegularExpression& gccPattern();

    [[nodiscard]] static ProblemSeverity severityFromText(const QString& text);
};

} // namespace keys::buildrun
