#pragma once

#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace keys::project {

/// Decides which paths a project's index and search should skip.
///
/// Two sources, in order of authority:
///
/// 1. `.gitignore` files in the project. Honouring them is what makes search
///    results useful — a developer almost never wants `node_modules` in their
///    results, and they have already said so in a file the project owns.
/// 2. A small built-in list of directories that are never worth indexing
///    (`.git`, `node_modules`, `target`, build outputs). This covers projects
///    with no `.gitignore` at all.
///
/// This is a pattern matcher, not a full git implementation: it supports the
/// gitignore syntax that actually appears in practice — literal names, `*` and
/// `?` globs, directory-only patterns with a trailing slash, anchoring with a
/// leading slash, and negation with `!`. Deliberately excluded is `**` spanning
/// arbitrary depth in the middle of a pattern, which is rare and expensive to
/// match correctly; such a pattern is treated as a single-segment glob rather
/// than silently ignored.
class IgnoreRules {
public:
    IgnoreRules();

    /// Parses gitignore-format text. `baseDirectory` is the directory the file
    /// lives in, relative to the project root, so anchored patterns resolve
    /// against the right place. Additive: call once per .gitignore found.
    void addPatterns(const QString& gitignoreText, const QString& baseDirectory = {});

    /// True if `relativePath` (relative to the project root, forward slashes)
    /// should be skipped. `isDirectory` matters because a pattern ending in "/"
    /// matches only directories.
    [[nodiscard]] bool isIgnored(const QString& relativePath, bool isDirectory) const;

    /// Clears everything, including the built-in defaults. Used by tests that
    /// need to check a single rule in isolation.
    void clear();

    /// Restores the built-in defaults after clear().
    void addDefaults();

    [[nodiscard]] int patternCount() const { return static_cast<int>(m_patterns.size()); }

private:
    struct Pattern {
        QRegularExpression regex;
        bool directoryOnly = false;
        bool negated = false;   ///< a "!" rule, which re-includes a match
    };

    /// Translates one gitignore pattern into an anchored regular expression.
    static Pattern compile(const QString& pattern, const QString& baseDirectory);

    QList<Pattern> m_patterns;
};

} // namespace keys::project
