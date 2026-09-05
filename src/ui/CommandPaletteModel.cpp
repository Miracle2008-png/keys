#include "ui/CommandPaletteModel.h"

#include "core/Log.h"
#include "search/FuzzyMatch.h"

#include <QVariantList>

#include <algorithm>

using keys::search::FuzzyMatch;
using keys::search::FuzzyResult;

namespace keys::ui {
namespace {

/// The prefix that restricts the palette to commands. `>` is what every editor
/// uses, so muscle memory carries over.
constexpr QChar kCommandPrefix = u'>';

const QString& commandsGroup()
{
    static const QString group = QStringLiteral("Commands");
    return group;
}

const QString& filesGroup()
{
    static const QString group = QStringLiteral("Files");
    return group;
}

/// The instance main() publishes for QML; see Theme.cpp for the reasoning.
CommandPaletteModel* g_instance = nullptr;

} // namespace

void CommandPaletteModel::setInstance(CommandPaletteModel* instance)
{
    g_instance = instance;
}

CommandPaletteModel* CommandPaletteModel::create(QQmlEngine* engine, QJSEngine* scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    Q_ASSERT_X(g_instance, "CommandPaletteModel::create",
               "CommandPaletteModel::setInstance was not called");

    QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
    return g_instance;
}

CommandPaletteModel::CommandPaletteModel(core::CommandRegistry& commands,
                                         search::FileIndex& index,
                                         QObject* parent)
    : QAbstractListModel(parent), m_commands(commands), m_index(index)
{
    // The index finishes building after the palette may already be open, and a
    // command can be registered by a module at any time. Both rebuild the list
    // so it is never stale.
    connect(&m_index, &search::FileIndex::changed, this, [this] { rebuild(); });
    connect(&m_commands, &core::CommandRegistry::commandRegistered,
            this, [this] { rebuild(); });
    connect(&m_commands, &core::CommandRegistry::commandUnregistered,
            this, [this] { rebuild(); });

    rebuild();
}

int CommandPaletteModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant CommandPaletteModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const Entry& entry = m_entries.at(static_cast<size_t>(index.row()));

    switch (role) {
    case TitleRole:
        return entry.title;
    case SubtitleRole:
        return entry.subtitle;
    case GroupRole:
        return entry.group;
    case IsGroupStartRole:
        // The first row overall, or the first after a group change. QML draws a
        // heading on these rather than the model emitting separate header rows,
        // which would complicate selection and keyboard navigation.
        return index.row() == 0
               || m_entries.at(static_cast<size_t>(index.row()) - 1).group != entry.group;
    case EnabledRole:
        return entry.enabled;
    case MatchPositionsRole: {
        QVariantList positions;
        positions.reserve(static_cast<qsizetype>(entry.matchPositions.size()));
        for (const int position : entry.matchPositions) {
            positions.append(position);
        }
        return positions;
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> CommandPaletteModel::roleNames() const
{
    return {
        {TitleRole, "title"},
        {SubtitleRole, "subtitle"},
        {GroupRole, "group"},
        {IsGroupStartRole, "isGroupStart"},
        {EnabledRole, "enabled"},
        {MatchPositionsRole, "matchPositions"},
    };
}

QString CommandPaletteModel::placeholder() const
{
    return m_query.startsWith(kCommandPrefix)
               ? tr("Type a command")
               : tr("Search files, or type > for commands");
}

void CommandPaletteModel::setQuery(const QString& query)
{
    if (query == m_query) {
        return;
    }
    m_query = query;

    emit queryChanged();
    rebuild();
}

void CommandPaletteModel::reset()
{
    m_query.clear();
    emit queryChanged();
    rebuild();
}

void CommandPaletteModel::setSelectedIndex(int index)
{
    const int clamped = m_entries.empty() ? 0 : std::clamp(index, 0, rowCount() - 1);
    if (clamped == m_selectedIndex) {
        return;
    }
    m_selectedIndex = clamped;
    emit selectedIndexChanged();
}

void CommandPaletteModel::selectNext()
{
    if (m_entries.empty()) {
        return;
    }
    // Wrapping: the list is short, and reaching the bottom only to scroll back
    // to the top is a wasted press.
    setSelectedIndex((m_selectedIndex + 1) % rowCount());
}

void CommandPaletteModel::selectPrevious()
{
    if (m_entries.empty()) {
        return;
    }
    setSelectedIndex((m_selectedIndex + rowCount() - 1) % rowCount());
}

bool CommandPaletteModel::acceptSelected()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= rowCount()) {
        return false;
    }

    const Entry entry = m_entries.at(static_cast<size_t>(m_selectedIndex));

    // A disabled command stays visible so the list does not shift under the
    // user, but choosing it does nothing rather than failing loudly.
    if (!entry.enabled) {
        return false;
    }

    if (entry.kind == Kind::Command) {
        const core::Status status = m_commands.invoke(entry.identifier);
        if (!status) {
            qCWarning(lcUi) << "palette command failed:" << status.error().toString();
            return false;
        }
    } else {
        emit fileChosen(entry.identifier);
    }

    emit accepted();
    return true;
}

void CommandPaletteModel::appendCommands(const QString& query,
                                         std::vector<Entry>& into) const
{
    std::vector<Entry> matches;

    for (const core::Command& command : m_commands.all()) {
        // Matched against the title and the category together, so typing
        // "view split" finds "Split Editor" in the View category.
        const QString searchable = command.category.isEmpty()
                                       ? command.title
                                       : command.category + QLatin1Char(' ') + command.title;

        FuzzyResult result = FuzzyMatch::match(query, searchable);
        if (!result.matched()) {
            // Keywords are the fallback: words that help find a command but do
            // not belong in its title.
            bool viaKeyword = false;
            for (const QString& keyword : command.keywords) {
                if (FuzzyMatch::match(query, keyword).matched()) {
                    viaKeyword = true;
                    break;
                }
            }
            if (!viaKeyword) {
                continue;
            }
            result.score = 1;   // found, but ranked below any direct match
            result.positions.clear();
        }

        Entry entry;
        entry.kind = Kind::Command;
        entry.title = command.title;
        entry.subtitle = command.category;
        entry.identifier = command.id;
        entry.group = commandsGroup();
        entry.enabled = command.enabled();
        entry.score = result.score;

        // Positions are against the searchable string, which is prefixed by the
        // category. Shift them onto the title, and drop any that fall inside
        // the prefix - QML highlights the title, not the category.
        const int offset = command.category.isEmpty() ? 0 : command.category.size() + 1;
        for (const int position : result.positions) {
            if (position >= offset) {
                entry.matchPositions.push_back(position - offset);
            }
        }

        matches.push_back(std::move(entry));
    }

    // Highest score first; ties broken by title so the order is stable rather
    // than depending on registration order.
    std::sort(matches.begin(), matches.end(), [](const Entry& a, const Entry& b) {
        return a.score != b.score ? a.score > b.score : a.title < b.title;
    });

    if (matches.size() > static_cast<size_t>(kMaxCommands)) {
        matches.resize(static_cast<size_t>(kMaxCommands));
    }

    into.insert(into.end(), std::make_move_iterator(matches.begin()),
                std::make_move_iterator(matches.end()));
}

void CommandPaletteModel::appendFiles(const QString& query, std::vector<Entry>& into) const
{
    std::vector<Entry> matches;
    matches.reserve(std::min(m_index.files().size(), static_cast<size_t>(kMaxFiles) * 4));

    for (const search::IndexedFile& file : m_index.files()) {
        const FuzzyResult result = FuzzyMatch::matchPath(query, file.relativePath);
        if (!result.matched()) {
            continue;
        }

        Entry entry;
        entry.kind = Kind::File;
        entry.title = file.fileName;
        entry.identifier = file.relativePath;
        entry.group = filesGroup();
        entry.score = result.score;

        // The directory, so two files with the same name are distinguishable.
        const int lastSeparator = file.relativePath.lastIndexOf(QLatin1Char('/'));
        entry.subtitle = lastSeparator > 0 ? file.relativePath.left(lastSeparator) : QString();

        // Positions are against the whole path; the title is only the file
        // name, so shift and drop what falls in the directory part.
        const int nameOffset = lastSeparator >= 0 ? lastSeparator + 1 : 0;
        for (const int position : result.positions) {
            if (position >= nameOffset) {
                entry.matchPositions.push_back(position - nameOffset);
            }
        }

        matches.push_back(std::move(entry));
    }

    // partial_sort rather than a full sort: only the first page is displayed,
    // and on a 50,000-file project sorting everything would be most of the
    // per-keystroke cost for results nobody sees.
    const size_t keep = std::min(matches.size(), static_cast<size_t>(kMaxFiles));
    std::partial_sort(matches.begin(),
                      matches.begin() + static_cast<long>(keep),
                      matches.end(),
                      [](const Entry& a, const Entry& b) {
                          return a.score != b.score ? a.score > b.score : a.title < b.title;
                      });
    matches.resize(keep);

    into.insert(into.end(), std::make_move_iterator(matches.begin()),
                std::make_move_iterator(matches.end()));
}

void CommandPaletteModel::rebuild()
{
    beginResetModel();

    m_entries.clear();

    const bool commandsOnly = m_query.startsWith(kCommandPrefix);
    const QString query = commandsOnly ? m_query.mid(1).trimmed() : m_query.trimmed();

    if (commandsOnly) {
        appendCommands(query, m_entries);
    } else {
        // Files first: with no prefix the user is naming a file, which is the
        // common case. Commands still appear below, so one box finds either
        // without the user having to decide first.
        appendFiles(query, m_entries);
        appendCommands(query, m_entries);
    }

    endResetModel();

    // The first result is selected, so Enter runs the best match without any
    // navigation. Reset rather than preserved: after a keystroke the previous
    // selection refers to a row that may no longer exist.
    m_selectedIndex = 0;

    emit countChanged();
    emit selectedIndexChanged();
}

} // namespace keys::ui
