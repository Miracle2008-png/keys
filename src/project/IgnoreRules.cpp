#include "project/IgnoreRules.h"

namespace keys::project {
namespace {

/// Directories that are never worth indexing, for projects with no .gitignore.
/// Kept short on purpose: the project's own .gitignore is the authority, and a
/// long built-in list would start hiding files the user expects to find.
const QStringList& builtinPatterns()
{
    static const QStringList patterns = {
        QStringLiteral(".git/"),
        QStringLiteral(".hg/"),
        QStringLiteral(".svn/"),
        QStringLiteral("node_modules/"),
        QStringLiteral("__pycache__/"),
        QStringLiteral(".venv/"),
        QStringLiteral("target/"),
        QStringLiteral("build/"),
        QStringLiteral("dist/"),
        QStringLiteral(".DS_Store"),
    };
    return patterns;
}

} // namespace

IgnoreRules::IgnoreRules()
{
    addDefaults();
}

void IgnoreRules::addDefaults()
{
    for (const QString& pattern : builtinPatterns()) {
        m_patterns.append(compile(pattern, QString()));
    }
}

void IgnoreRules::clear()
{
    m_patterns.clear();
}

void IgnoreRules::addPatterns(const QString& gitignoreText, const QString& baseDirectory)
{
    const QStringList lines = gitignoreText.split(QLatin1Char('\n'));
    for (const QString& rawLine : lines) {
        QString line = rawLine.trimmed();

        // Blank lines and comments carry no rule.
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        // A backslash escapes a leading '#' or '!' that is meant literally.
        if (line.startsWith(QLatin1String("\\#")) || line.startsWith(QLatin1String("\\!"))) {
            line = line.mid(1);
        }

        m_patterns.append(compile(line, baseDirectory));
    }
}

IgnoreRules::Pattern IgnoreRules::compile(const QString& rawPattern,
                                          const QString& baseDirectory)
{
    Pattern result;
    QString pattern = rawPattern;

    if (pattern.startsWith(QLatin1Char('!'))) {
        result.negated = true;
        pattern = pattern.mid(1);
    }

    if (pattern.endsWith(QLatin1Char('/'))) {
        result.directoryOnly = true;
        pattern.chop(1);
    }

    // A pattern containing a slash anywhere but the end is anchored to the
    // directory holding the .gitignore. One without is matched against any path
    // segment, at any depth - that is the rule that makes "*.log" work everywhere.
    const bool anchored = pattern.contains(QLatin1Char('/'));
    if (pattern.startsWith(QLatin1Char('/'))) {
        pattern = pattern.mid(1);
    }

    // Escape everything, then re-enable the two wildcards gitignore defines.
    // Building the regex this way means no other regex metacharacter in a
    // filename can be misread as syntax.
    QString expression = QRegularExpression::escape(pattern);
    expression.replace(QStringLiteral("\\*"), QStringLiteral("[^/]*"));
    expression.replace(QStringLiteral("\\?"), QStringLiteral("[^/]"));

    QString prefix;
    if (!baseDirectory.isEmpty()) {
        prefix = QRegularExpression::escape(baseDirectory) + QStringLiteral("/");
    }

    // Anchored patterns must match from the base directory. Unanchored ones may
    // begin at any segment boundary, so they match the same name at any depth.
    //
    // Built as statements rather than a ternary: QT_USE_QSTRINGBUILDER makes each
    // branch a different expression template type, which a ternary cannot unify.
    QString body = prefix;
    if (!anchored) {
        body += QStringLiteral("(?:.*/)?");
    }
    body += expression;

    // A directory pattern also covers everything beneath it: ignoring
    // "node_modules/" must ignore its contents, not merely the folder entry.
    result.regex = QRegularExpression(QStringLiteral("^%1(?:/.*)?$").arg(body));
    return result;
}

bool IgnoreRules::isIgnored(const QString& relativePath, bool isDirectory) const
{
    // A directory-only rule ("build/") covers the directory and everything under
    // it, but must not match a plain *file* named "build". Testing the path
    // itself only when it is a directory, and always testing its ancestors,
    // expresses that directly - each ancestor is a directory by definition.
    QStringList candidates;
    if (isDirectory) {
        candidates.append(relativePath);
    }
    for (int slash = relativePath.indexOf(QLatin1Char('/')); slash >= 0;
         slash = relativePath.indexOf(QLatin1Char('/'), slash + 1)) {
        candidates.append(relativePath.left(slash));
    }

    bool ignored = false;

    // Later rules override earlier ones, which is what makes a "!" re-include
    // work. Every pattern is evaluated rather than stopping at the first match.
    for (const Pattern& pattern : m_patterns) {
        bool matched = false;

        if (pattern.directoryOnly) {
            for (const QString& candidate : candidates) {
                if (pattern.regex.match(candidate).hasMatch()) {
                    matched = true;
                    break;
                }
            }
        } else {
            matched = pattern.regex.match(relativePath).hasMatch();
        }

        if (matched) {
            ignored = !pattern.negated;
        }
    }

    return ignored;
}

} // namespace keys::project
