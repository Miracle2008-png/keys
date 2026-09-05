#include "buildrun/ProblemParser.h"

namespace keys::buildrun {
namespace {

/// A path that is followed by a position. Deliberately permissive about what a
/// path may contain — spaces are ordinary on Windows — and bounded instead by
/// what must follow it, which is what stops it swallowing the message.
constexpr auto kMsvcPattern =
    R"(^\s*(.+?)\((\d+)(?:,(\d+))?\)\s*:\s*(fatal error|error|warning|note)\s+?([^:]*):\s*(.*)$)";

constexpr auto kGccPattern =
    R"(^\s*(.+?):(\d+):(?:(\d+):)?\s*(fatal error|error|warning|note):\s*(.*)$)";

} // namespace

const QRegularExpression& ProblemParser::msvcPattern()
{
    static const QRegularExpression pattern(QString::fromLatin1(kMsvcPattern));
    return pattern;
}

const QRegularExpression& ProblemParser::gccPattern()
{
    static const QRegularExpression pattern(QString::fromLatin1(kGccPattern));
    return pattern;
}

ProblemSeverity ProblemParser::severityFromText(const QString& text)
{
    if (text.startsWith(QLatin1String("warning"), Qt::CaseInsensitive)) {
        return ProblemSeverity::Warning;
    }
    if (text.startsWith(QLatin1String("note"), Qt::CaseInsensitive)) {
        return ProblemSeverity::Note;
    }
    // "error" and "fatal error" both. A fatal error is an error that also
    // stopped the compiler, which changes nothing about how it is displayed.
    return ProblemSeverity::Error;
}

QString ProblemParser::severityLabel(ProblemSeverity severity)
{
    switch (severity) {
    case ProblemSeverity::Error:
        return QStringLiteral("Error");
    case ProblemSeverity::Warning:
        return QStringLiteral("Warning");
    case ProblemSeverity::Note:
        return QStringLiteral("Note");
    }
    return QString();
}

std::optional<Problem> ProblemParser::parseLine(const QString& line)
{
    if (line.trimmed().isEmpty()) {
        return std::nullopt;
    }

    // GCC/Clang first. The MSVC pattern requires parentheses around the
    // position, so the two cannot both match, but trying the more specific
    // shape first keeps that independent of the patterns' details.
    if (const QRegularExpressionMatch match = gccPattern().match(line); match.hasMatch()) {
        Problem problem;
        problem.file = match.captured(1).trimmed();
        problem.line = match.captured(2).toInt();
        problem.column = match.captured(3).toInt();   // 0 when the group is empty
        problem.severity = severityFromText(match.captured(4));
        problem.message = match.captured(5).trimmed();
        problem.rawLine = line;

        // A bare drive letter would otherwise be read as a path with a line
        // number: "C:\src\a.cpp:12" splits at the wrong colon on Windows. A
        // one-character "file" is never real.
        if (problem.file.size() <= 1) {
            return std::nullopt;
        }
        return problem;
    }

    if (const QRegularExpressionMatch match = msvcPattern().match(line); match.hasMatch()) {
        Problem problem;
        problem.file = match.captured(1).trimmed();
        problem.line = match.captured(2).toInt();
        problem.column = match.captured(3).toInt();
        problem.severity = severityFromText(match.captured(4));

        // MSVC puts a diagnostic code between the severity and the message
        // ("error C2065: ..."). It is useful in the message, so it is kept
        // rather than dropped.
        const QString code = match.captured(5).trimmed();
        const QString text = match.captured(6).trimmed();
        problem.message = code.isEmpty() ? text
                                         : QStringLiteral("%1: %2").arg(code, text);
        problem.rawLine = line;

        if (problem.file.size() <= 1) {
            return std::nullopt;
        }
        return problem;
    }

    return std::nullopt;
}

std::vector<Problem> ProblemParser::parse(const QStringList& lines)
{
    std::vector<Problem> problems;
    for (const QString& line : lines) {
        if (std::optional<Problem> problem = parseLine(line)) {
            problems.push_back(std::move(*problem));
        }
    }
    return problems;
}

} // namespace keys::buildrun
