#include "search/TextSearch.h"

#include "core/Log.h"
#include "core/Trace.h"
#include "filesystem/FileSystem.h"

#include <QDir>
#include <QFile>

using keys::core::CancellationSource;
using keys::core::CancellationToken;
using keys::fs::FileSystem;

namespace keys::search {

TextSearch::TextSearch(project::Project& project, FileIndex& index,
                       core::TaskScheduler& scheduler, QObject* parent)
    : QObject(parent),
      m_project(project),
      m_index(index),
      m_scheduler(scheduler),
      m_cancellation(std::make_unique<CancellationSource>())
{
}

TextSearch::~TextSearch()
{
    m_cancellation->cancel();
}

QRegularExpression TextSearch::buildPattern(const SearchOptions& options)
{
    QString pattern = options.regularExpression
                          ? options.query
                          // Escaped when not a regex, so a query containing
                          // `(` or `.` searches for those characters rather
                          // than being read as syntax.
                          : QRegularExpression::escape(options.query);

    if (options.wholeWord) {
        pattern = QStringLiteral("\\b(?:%1)\\b").arg(pattern);
    }

    QRegularExpression::PatternOptions patternOptions =
        QRegularExpression::UseUnicodePropertiesOption;
    if (!options.caseSensitive) {
        patternOptions |= QRegularExpression::CaseInsensitiveOption;
    }

    return QRegularExpression(pattern, patternOptions);
}

bool TextSearch::looksLikeText(const QByteArray& sample)
{
    // A NUL byte in the first few kilobytes means binary. The same heuristic
    // grep uses: cheap, and wrong only for files that are not worth searching
    // anyway.
    return !sample.contains('\0');
}

bool TextSearch::passesFilters(const QString& relativePath, const SearchOptions& options)
{
    // Excludes win over includes, which is the behaviour every search tool has:
    // "these files, but not those" is the common intent.
    for (const QString& glob : options.excludeGlobs) {
        if (QDir::match(glob, relativePath)) {
            return false;
        }
    }

    if (options.includeGlobs.isEmpty()) {
        return true;
    }

    for (const QString& glob : options.includeGlobs) {
        if (QDir::match(glob, relativePath)) {
            return true;
        }
    }
    return false;
}

std::vector<SearchMatch> TextSearch::searchFile(const QString& absolutePath,
                                                const QString& relativePath,
                                                const QRegularExpression& pattern,
                                                const CancellationToken& token)
{
    std::vector<SearchMatch> matches;

    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return matches;
    }

    // A binary check on the first chunk, before committing to reading the whole
    // file. A repository full of images and build artefacts would otherwise cost
    // as much to search as one full of source.
    const QByteArray sample = file.peek(kBinarySampleBytes);
    if (!looksLikeText(sample)) {
        return matches;
    }

    // Read whole rather than streaming: source files are small, and one read is
    // faster than many. The index already excludes what .gitignore excludes, so
    // this does not encounter multi-gigabyte artefacts.
    const QString contents = QString::fromUtf8(file.readAll());
    file.close();

    int lineNumber = 0;
    int lineStart = 0;

    while (lineStart <= contents.size()) {
        if (token.isCancelled()) {
            return {};
        }

        int lineEnd = contents.indexOf(QLatin1Char('\n'), lineStart);
        if (lineEnd < 0) {
            lineEnd = contents.size();
        }

        // Materialised once per line: globalMatch takes a QString, and building
        // it here rather than per match avoids a copy for every hit on a line
        // that has several.
        const QString line = contents.mid(lineStart, lineEnd - lineStart);

        QRegularExpressionMatchIterator iterator = pattern.globalMatch(line);
        if (iterator.hasNext()) {
            // Trailing whitespace trimmed, leading kept: indentation is part of
            // how code reads, trailing spaces are noise. Computed once for the
            // line rather than per match.
            QString displayText = line;
            while (!displayText.isEmpty() && displayText.back().isSpace()) {
                displayText.chop(1);
            }

            // Every match on the line, not just the first: a line can contain
            // the query several times and each is a distinct place to jump to.
            while (iterator.hasNext()) {
                const QRegularExpressionMatch match = iterator.next();

                SearchMatch result;
                result.relativePath = relativePath;
                result.line = lineNumber;
                result.column = static_cast<int>(match.capturedStart());
                result.length = static_cast<int>(match.capturedLength());
                result.lineText = displayText;

                matches.push_back(std::move(result));
            }
        }

        lineStart = lineEnd + 1;
        ++lineNumber;
    }

    return matches;
}

void TextSearch::search(const SearchOptions& options)
{
    cancel();

    if (options.query.isEmpty() || !m_project.isOpen()) {
        return;
    }

    const QRegularExpression pattern = buildPattern(options);
    if (!pattern.isValid()) {
        // An incomplete regular expression is what the user types while still
        // typing it; reporting nothing is correct and quiet.
        qCDebug(lcCore) << "invalid search pattern:" << pattern.errorString();
        emit finished();
        return;
    }

    // Snapshot the file list: the index can change under a long search, and
    // iterating it directly from workers would be a data race.
    std::vector<QString> paths;
    paths.reserve(m_index.files().size());
    for (const IndexedFile& file : m_index.files()) {
        if (passesFilters(file.relativePath, options)) {
            paths.push_back(file.relativePath);
        }
    }

    if (paths.empty()) {
        emit finished();
        return;
    }

    m_searching = true;
    m_truncated = false;
    emit searchingChanged();

    const QString root = m_project.root();
    const CancellationToken token = m_cancellation->token();
    const int maxResults = options.maxResults;

    // Split across the pool by batches of files. Batching amortises the
    // per-task scheduling cost, which would otherwise dominate for small files.
    for (size_t start = 0; start < paths.size(); start += kFilesPerTask) {
        const size_t end = std::min(start + kFilesPerTask, paths.size());
        std::vector<QString> batch(paths.begin() + static_cast<long>(start),
                                   paths.begin() + static_cast<long>(end));

        ++m_pending;

        m_scheduler.postWithResult<std::vector<SearchMatch>>(
            this,
            [root, batch, pattern, token] {
                std::vector<SearchMatch> found;
                for (const QString& relative : batch) {
                    if (token.isCancelled()) {
                        return std::vector<SearchMatch>{};
                    }
                    const QString absolute = QDir(root).filePath(relative);
                    std::vector<SearchMatch> inFile =
                        searchFile(absolute, relative, pattern, token);
                    found.insert(found.end(),
                                 std::make_move_iterator(inFile.begin()),
                                 std::make_move_iterator(inFile.end()));
                }
                return found;
            },
            [this, token, maxResults](std::vector<SearchMatch> found) {
                --m_pending;

                if (token.isCancelled()) {
                    return;
                }

                if (!found.empty() && !m_truncated) {
                    const int room = maxResults - static_cast<int>(m_results.size());
                    if (room <= 0) {
                        m_truncated = true;
                    } else {
                        if (static_cast<int>(found.size()) > room) {
                            found.resize(static_cast<size_t>(room));
                            m_truncated = true;
                        }
                        m_results.insert(m_results.end(),
                                         std::make_move_iterator(found.begin()),
                                         std::make_move_iterator(found.end()));

                        // Emitted per batch, so results fill in progressively
                        // rather than appearing all at once at the end.
                        emit resultsChanged();
                    }
                }

                if (m_pending == 0) {
                    m_searching = false;
                    emit searchingChanged();
                    emit finished();
                }
            },
            core::TaskScheduler::Priority::Normal);
    }
}

void TextSearch::cancel()
{
    m_cancellation->cancel();
    m_cancellation = std::make_unique<CancellationSource>();

    m_pending = 0;
    m_truncated = false;

    if (!m_results.empty()) {
        m_results.clear();
        emit resultsChanged();
    }

    if (m_searching) {
        m_searching = false;
        emit searchingChanged();
    }
}

} // namespace keys::search
