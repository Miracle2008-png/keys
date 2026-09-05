#include "extensions/Manifest.h"

#include "filesystem/FileSystem.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>

namespace keys::extensions {
namespace {

struct CapabilityInfo {
    Capability capability;
    const char* id;
    const char* description;
    bool sensitive;
};

/// The table that defines every capability. One place, so an added capability
/// cannot be missing a description or a sensitivity - which is how a permission
/// ends up granted silently.
const std::vector<CapabilityInfo>& capabilityTable()
{
    static const std::vector<CapabilityInfo> table = {
        {Capability::ReadWorkspace, "readWorkspace",
         "Read the files in your project", false},
        {Capability::WriteWorkspace, "writeWorkspace",
         "Create, change and delete files in your project", true},
        {Capability::ReadEditor, "readEditor",
         "See the text and cursor position in your editor", false},
        {Capability::ModifyEditor, "modifyEditor",
         "Change the text in your editor", true},
        {Capability::RegisterCommands, "registerCommands",
         "Add commands to the command palette", false},
        {Capability::ShowUi, "showUi",
         "Show notifications and panels", false},
        {Capability::Network, "network",
         "Connect to the internet", true},
        {Capability::RunProcesses, "runProcesses",
         "Run programs on your computer", true},
    };
    return table;
}

const CapabilityInfo* findCapability(Capability capability)
{
    const auto& table = capabilityTable();
    const auto it = std::find_if(table.cbegin(), table.cend(),
                                 [capability](const CapabilityInfo& info) {
                                     return info.capability == capability;
                                 });
    return it == table.cend() ? nullptr : &*it;
}

constexpr auto kManifestFileName = "keys-extension.json";

} // namespace

QString capabilityId(Capability capability)
{
    const CapabilityInfo* info = findCapability(capability);
    return info ? QString::fromLatin1(info->id) : QString();
}

std::optional<Capability> capabilityFromId(const QString& id)
{
    for (const CapabilityInfo& info : capabilityTable()) {
        if (id == QLatin1String(info.id)) {
            return info.capability;
        }
    }
    return std::nullopt;
}

QString capabilityDescription(Capability capability)
{
    const CapabilityInfo* info = findCapability(capability);
    return info ? QString::fromLatin1(info->description) : QString();
}

bool capabilityIsSensitive(Capability capability)
{
    const CapabilityInfo* info = findCapability(capability);
    return info && info->sensitive;
}

bool Manifest::declares(Capability capability) const
{
    return std::find(capabilities.cbegin(), capabilities.cend(), capability)
           != capabilities.cend();
}

core::Result<Manifest> Manifest::parse(const QJsonObject& object)
{
    Manifest manifest;
    manifest.id = object.value(QStringLiteral("id")).toString().trimmed();
    manifest.name = object.value(QStringLiteral("name")).toString().trimmed();
    manifest.version = object.value(QStringLiteral("version")).toString();
    manifest.description = object.value(QStringLiteral("description")).toString();
    manifest.author = object.value(QStringLiteral("author")).toString();

    const QJsonObject entry = object.value(QStringLiteral("entry")).toObject();
    manifest.entryPoint = entry.value(QStringLiteral("program")).toString();

    for (const QJsonValue& argument : entry.value(QStringLiteral("arguments")).toArray()) {
        manifest.entryArguments << argument.toString();
    }

    // The id is what a grant is recorded against, so it has to be usable as a
    // key: an empty or path-like id would collide or escape its own directory.
    if (manifest.id.isEmpty()) {
        return core::Err(core::ErrorCode::ParseError,
                         QStringLiteral("The manifest has no id"));
    }
    if (manifest.id.contains(QLatin1Char('/')) || manifest.id.contains(QLatin1Char('\\'))
        || manifest.id.contains(QLatin1String(".."))) {
        return core::Err(core::ErrorCode::ParseError,
                         QStringLiteral("The extension id must not contain path separators"),
                         manifest.id);
    }
    if (manifest.name.isEmpty()) {
        return core::Err(core::ErrorCode::ParseError,
                         QStringLiteral("The manifest has no name"), manifest.id);
    }
    if (manifest.entryPoint.isEmpty()) {
        return core::Err(core::ErrorCode::ParseError,
                         QStringLiteral("The manifest has no entry program"), manifest.id);
    }

    // An unknown capability fails the whole manifest rather than being dropped.
    // Silently ignoring one would run the extension with less than it asked for
    // and no way for either side to notice.
    for (const QJsonValue& value : object.value(QStringLiteral("capabilities")).toArray()) {
        const QString id = value.toString();
        const std::optional<Capability> capability = capabilityFromId(id);
        if (!capability) {
            return core::Err(core::ErrorCode::ParseError,
                             QStringLiteral("Unknown capability '%1'").arg(id),
                             manifest.id);
        }
        manifest.capabilities.push_back(*capability);
    }

    const QJsonObject contributes = object.value(QStringLiteral("contributes")).toObject();

    for (const QJsonValue& value : contributes.value(QStringLiteral("commands")).toArray()) {
        const QJsonObject command = value.toObject();

        CommandContribution contribution;
        contribution.id = command.value(QStringLiteral("id")).toString();
        contribution.title = command.value(QStringLiteral("title")).toString();
        contribution.category = command.value(QStringLiteral("category")).toString();

        if (contribution.id.isEmpty() || contribution.title.isEmpty()) {
            return core::Err(core::ErrorCode::ParseError,
                             QStringLiteral("A contributed command needs an id and a title"),
                             manifest.id);
        }

        // Namespaced by the extension so two extensions cannot claim the same
        // command id, and so the palette can say where a command came from.
        if (!contribution.id.startsWith(manifest.id + QLatin1Char('.'))) {
            return core::Err(
                core::ErrorCode::ParseError,
                QStringLiteral("Command '%1' must be namespaced as '%2.<name>'")
                    .arg(contribution.id, manifest.id),
                manifest.id);
        }

        manifest.commands.push_back(std::move(contribution));
    }

    // Contributing commands without asking for the capability is a manifest
    // that contradicts itself, and would leave commands in the palette that
    // cannot run.
    if (!manifest.commands.empty() && !manifest.declares(Capability::RegisterCommands)) {
        return core::Err(
            core::ErrorCode::ParseError,
            QStringLiteral("Contributing commands requires the 'registerCommands' capability"),
            manifest.id);
    }

    for (const QJsonValue& value :
         object.value(QStringLiteral("activationLanguages")).toArray()) {
        manifest.activationLanguages << value.toString();
    }

    return manifest;
}

core::Result<Manifest> Manifest::load(const QString& directory)
{
    const QString path = QDir(directory).filePath(QLatin1String(kManifestFileName));

    const core::Result<QString> text = fs::FileSystem::readTextFile(path);
    if (!text) {
        return text.error();
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(text.value().toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return core::Err(core::ErrorCode::ParseError,
                         QStringLiteral("The manifest is not valid JSON: %1")
                             .arg(error.errorString()),
                         path);
    }

    return parse(document.object());
}

} // namespace keys::extensions
