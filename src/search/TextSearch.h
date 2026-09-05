#pragma once

#include "core/Cancellation.h"
#include "core/TaskScheduler.h"
#include "project/Project.h"
#include "search/FileIndex.h"

#include <QObject>
#include <QRegularExpression>
#include <QString>

#include <memory>

namespace keys::search {

/// One matching line.
struct SearchMatch {
    QString relativePath;
    int line = 0;          ///< zero-based
    int column = 0;        ///< zero-based, in UTF-16 code units
    int length = 0;        ///< of the match itself

    /// The whole line, for display. Trimmed of trailing whitespace but not
    /// leading, so indentation still reads correctly in results.
    QString lineText;
};

/// How a search is run.
struct SearchOptions {
    QString query;
    bool caseSensitive = false;
    bool wholeWord = false;
    bool regularExpression = false;

    /// Glob patterns to include or exclude, as the user typed them. Empty
    /// include means every file the index holds.
    QStringList includeGlobs;
    QStringList excludeGlobs;

    /// Results stop at this many. A query like `e` would otherwise match
    /// hundreds of thousands of lines, and no one reads past the first few
    /// hundred — collecting them all would cost time and memory for nothing.
    int maxResults = 2000;
};

/// Project-wide text search.
///
/// **Where the time goes.** The budget is first results within 300 ms. Reading
/// every file dominates, so the work is split across the worker pool by file,
/// and results are reported in batches as they are found rather than after the
/// whole project is scanned. The first batch therefore arrives after a handful
/// of files, not after all of them.
///
/// **Binary files are skipped.** A NUL byte in the first few kilobytes means the
/// file is not text; searching it wastes time and would fill the results with
/// unreadable lines. This is the same heuristic grep uses.
///
/// **Cancellation is mandatory.** The user types, and each keystroke supersedes
/// the last search. A superseded search must stop promptly rather than race its
/// replacement to the results list.
class TextSearch : public QObject {
    Q_OBJECT

public:
    TextSearch(project::Project& project, FileIndex& index,
               core::TaskScheduler& scheduler, QObject* parent = nullptr);
    ~TextSearch() override;

    /// Starts a search, cancelling any that is running.
    void search(const SearchOptions& options);

    /// Stops the running search and clears results.
    void cancel();

    [[nodiscard]] bool isSearching() const { return m_searching; }
    [[nodiscard]] const std::vector<SearchMatch>& results() const { return m_results; }
    [[nodiscard]] int resultCount() const { return static_cast<int>(m_results.size()); }

    /// True when the search stopped because it hit maxResults, so the UI can say
    /// the list is incomplete rather than implying it is exhaustive.
    [[nodiscard]] bool wasTruncated() const { return m_truncated; }

signals:
    void searchingChanged();

    /// More results are available. Emitted per batch, so the list fills in
    /// rather than appearing all at once at the end.
    void resultsChanged();

    /// The search finished, was cancelled, or hit its limit.
    void finished();

private:
    /// Builds the matcher for an options set. Returns an invalid regular
    /// expression if the user's pattern does not compile, which the caller
    /// reports rather than searching for nothing.
    [[nodiscard]] static QRegularExpression buildPattern(const SearchOptions& options);

    /// Searches one file. Static and self-contained so it runs on a worker
    /// without touching anything this object owns.
    [[nodiscard]] static std::vector<SearchMatch> searchFile(
        const QString& absolutePath, const QString& relativePath,
        const QRegularExpression& pattern, const core::CancellationToken& token);

    /// True if the content looks like text rather than a binary file.
    [[nodiscard]] static bool looksLikeText(const QByteArray& sample);

    /// True if `relativePath` passes the include and exclude globs.
    [[nodiscard]] static bool passesFilters(const QString& relativePath,
                                            const SearchOptions& options);

    project::Project& m_project;
    FileIndex& m_index;
    core::TaskScheduler& m_scheduler;

    std::vector<SearchMatch> m_results;
    std::unique_ptr<core::CancellationSource> m_cancellation;

    /// How many per-file tasks are still outstanding, so completion is known
    /// without joining the pool.
    int m_pending = 0;

    bool m_searching = false;
    bool m_truncated = false;

    /// Files handed to one worker task. Batching amortises the scheduling cost,
    /// which would otherwise dominate for small files.
    static constexpr int kFilesPerTask = 64;

    /// How much of a file to sample when deciding whether it is text.
    static constexpr int kBinarySampleBytes = 8192;
};

} // namespace keys::search
