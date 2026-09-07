#include "ui/ProblemsModel.h"

#include "langsvc/LanguageServiceManager.h"
#include "project/Project.h"

#include <QDir>
#include <QFileInfo>
#include <QQmlEngine>

#include <algorithm>

namespace keys::ui {
namespace {

ProblemsModel* g_instance = nullptr;

int severityRank(langsvc::DiagnosticSeverity severity)
{
    return static_cast<int>(severity);
}

} // namespace

ProblemsModel::ProblemsModel(langsvc::LanguageServiceManager& services,
                             const project::Project& project, QObject* parent)
    : QAbstractListModel(parent), m_services(services), m_project(project)
{
    connect(&m_services, &langsvc::LanguageServiceManager::diagnosticsPublished,
            this, &ProblemsModel::onDiagnostics);
}

int ProblemsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant ProblemsModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= static_cast<int>(m_rows.size())) {
        return {};
    }

    const Row& row = m_rows.at(static_cast<size_t>(index.row()));
    const FileGroup& file = m_files.at(static_cast<size_t>(row.fileIndex));

    switch (role) {
    case KindRole:
        return row.kind;
    case PathRole:
        return file.path;
    case FileNameRole:
        return QFileInfo(file.path).fileName();
    case DirectoryRole: {
        // Relative to the project, because an absolute path in a narrow panel
        // is mostly the part every row has in common.
        const QString root = m_project.root();
        const QString directory = QFileInfo(file.path).path();
        if (!root.isEmpty() && directory.startsWith(root)) {
            const QString relative = QDir(root).relativeFilePath(directory);
            return relative == QLatin1String(".") ? QString() : relative;
        }
        return directory;
    }
    case ProblemCountRole:
        return static_cast<int>(file.problems.size());
    case CollapsedRole:
        return file.collapsed;
    case WorstSeverityRole: {
        int worst = HintSeverity;
        for (const langsvc::Diagnostic& problem : file.problems) {
            worst = std::min(worst, severityRank(problem.severity));
        }
        return worst;
    }
    default:
        break;
    }

    if (row.kind != ProblemRow) {
        return {};
    }

    const langsvc::Diagnostic& problem =
        file.problems.at(static_cast<size_t>(row.problemIndex));

    switch (role) {
    case SeverityRole:
        return severityRank(problem.severity);
    case MessageRole:
        // A server may wrap a long message; a row is one line, so the newlines
        // become spaces rather than being drawn as squares.
        return QString(problem.message).replace(QLatin1Char('\n'), QLatin1Char(' '));
    case SourceRole:
        return problem.source;
    case LineRole:
        return problem.range.start.line + 1;      // LSP is zero-based
    case ColumnRole:
        return problem.range.start.character + 1;
    default:
        return {};
    }
}

QHash<int, QByteArray> ProblemsModel::roleNames() const
{
    return {
        {KindRole, "kind"},
        {PathRole, "path"},
        {FileNameRole, "fileName"},
        {DirectoryRole, "directory"},
        {ProblemCountRole, "problemCount"},
        {CollapsedRole, "collapsed"},
        {SeverityRole, "severity"},
        {MessageRole, "message"},
        {SourceRole, "source"},
        {LineRole, "line"},
        {ColumnRole, "column"},
        {WorstSeverityRole, "worstSeverity"},
    };
}

int ProblemsModel::fileCount() const
{
    return static_cast<int>(m_files.size());
}

void ProblemsModel::setErrorsAndWarningsOnly(bool on)
{
    if (m_severeOnly == on) {
        return;
    }
    m_severeOnly = on;
    rebuild();
}

bool ProblemsModel::passesFilter(const langsvc::Diagnostic& diagnostic) const
{
    return !m_severeOnly
           || severityRank(diagnostic.severity) <= WarningSeverity;
}

void ProblemsModel::onDiagnostics(const QString& path,
                                  const std::vector<langsvc::Diagnostic>& diagnostics)
{
    m_hasAnalysed = true;

    // A server clears a file by publishing an empty list for it, so an empty
    // publication is a removal rather than something to ignore.
    if (diagnostics.empty()) {
        m_byPath.erase(path);
    } else {
        m_byPath[path] = diagnostics;
    }

    rebuild();
}

void ProblemsModel::rebuild()
{
    for (const FileGroup& file : m_files) {
        if (file.collapsed) {
            m_collapsed.insert(file.path);
        } else {
            m_collapsed.remove(file.path);
        }
    }

    beginResetModel();

    m_files.clear();
    m_errors = 0;
    m_warnings = 0;
    m_infos = 0;

    // m_byPath is a std::map, so the files come out in a stable order rather
    // than whichever order the servers happened to finish in.
    for (const auto& [path, diagnostics] : m_byPath) {
        FileGroup group;
        group.path = path;
        group.collapsed = m_collapsed.contains(path);

        for (const langsvc::Diagnostic& diagnostic : diagnostics) {
            // Counted before filtering, so the header's totals describe the
            // project rather than the current view - a filter that also changed
            // the counts would make it impossible to tell there were errors
            // being hidden.
            switch (severityRank(diagnostic.severity)) {
            case ErrorSeverity:   ++m_errors;   break;
            case WarningSeverity: ++m_warnings; break;
            default:              ++m_infos;    break;
            }

            if (passesFilter(diagnostic)) {
                group.problems.push_back(diagnostic);
            }
        }

        if (group.problems.empty()) {
            continue;
        }

        // Worst first, then by position: an error twenty lines down matters
        // more than a hint on line one.
        std::sort(group.problems.begin(), group.problems.end(),
                  [](const langsvc::Diagnostic& a, const langsvc::Diagnostic& b) {
                      if (a.severity != b.severity) {
                          return severityRank(a.severity) < severityRank(b.severity);
                      }
                      if (a.range.start.line != b.range.start.line) {
                          return a.range.start.line < b.range.start.line;
                      }
                      return a.range.start.character < b.range.start.character;
                  });

        m_files.push_back(std::move(group));
    }

    buildRows();

    // The remembered row indexes a list that has just been replaced.
    m_lastVisitedRow = -1;

    endResetModel();
    emit changed();
}

void ProblemsModel::buildRows()
{
    m_rows.clear();
    for (size_t f = 0; f < m_files.size(); ++f) {
        m_rows.push_back({FileRow, static_cast<int>(f), 0});
        if (m_files.at(f).collapsed) {
            continue;
        }
        for (size_t p = 0; p < m_files.at(f).problems.size(); ++p) {
            m_rows.push_back({ProblemRow, static_cast<int>(f), static_cast<int>(p)});
        }
    }
}

void ProblemsModel::activate(int row)
{
    if (row < 0 || row >= static_cast<int>(m_rows.size())) {
        return;
    }

    const Row entry = m_rows.at(static_cast<size_t>(row));

    if (entry.kind == FileRow) {
        FileGroup& file = m_files.at(static_cast<size_t>(entry.fileIndex));
        file.collapsed = !file.collapsed;
        if (file.collapsed) {
            m_collapsed.insert(file.path);
        } else {
            m_collapsed.remove(file.path);
        }

        beginResetModel();
        buildRows();
        endResetModel();
        return;
    }

    const FileGroup& file = m_files.at(static_cast<size_t>(entry.fileIndex));
    const langsvc::Diagnostic& problem =
        file.problems.at(static_cast<size_t>(entry.problemIndex));

    emit problemActivated(file.path,
                          problem.range.start.line + 1,
                          problem.range.start.character + 1);
}

bool ProblemsModel::hasProblems() const
{
    for (const Row& row : m_rows) {
        if (row.kind == ProblemRow) {
            return true;
        }
    }
    return false;
}

void ProblemsModel::goToNextProblem()
{
    if (m_rows.empty()) {
        return;
    }

    // Scans forward from the last visit, then wraps. Two passes rather than a
    // modulo walk so a list with no problem rows at all terminates.
    for (int pass = 0; pass < 2; ++pass) {
        const int from = pass == 0 ? m_lastVisitedRow + 1 : 0;
        const int to = pass == 0 ? static_cast<int>(m_rows.size()) : m_lastVisitedRow + 1;

        for (int row = from; row < to; ++row) {
            if (m_rows.at(static_cast<size_t>(row)).kind == ProblemRow) {
                m_lastVisitedRow = row;
                activate(row);
                return;
            }
        }
    }
}

void ProblemsModel::goToPreviousProblem()
{
    if (m_rows.empty()) {
        return;
    }

    for (int pass = 0; pass < 2; ++pass) {
        const int from = pass == 0 ? m_lastVisitedRow - 1
                                   : static_cast<int>(m_rows.size()) - 1;
        const int to = pass == 0 ? -1 : m_lastVisitedRow - 1;

        for (int row = from; row > to; --row) {
            if (row < 0 || row >= static_cast<int>(m_rows.size())) {
                continue;
            }
            if (m_rows.at(static_cast<size_t>(row)).kind == ProblemRow) {
                m_lastVisitedRow = row;
                activate(row);
                return;
            }
        }
    }
}

void ProblemsModel::clear()
{
    m_byPath.clear();
    m_collapsed.clear();
    m_lastVisitedRow = -1;
    m_hasAnalysed = false;
    rebuild();
}

void ProblemsModel::setInstance(ProblemsModel* instance)
{
    g_instance = instance;
}

ProblemsModel* ProblemsModel::create(QQmlEngine*, QJSEngine*)
{
    Q_ASSERT_X(g_instance, "ProblemsModel::create",
               "ProblemsModel::setInstance was not called");

    QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
    return g_instance;
}

} // namespace keys::ui
