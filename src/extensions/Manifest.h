#pragma once

#include "core/Result.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <vector>

namespace keys::extensions {

/// What an extension is allowed to do.
///
/// **Granted, never assumed.** An extension declares the capabilities it needs
/// in its manifest and gets exactly those. The host refuses any request outside
/// the grant, so a capability the user did not approve cannot be reached by
/// asking for it at runtime.
///
/// **Deliberately coarse.** Fine-grained permissions read well and are impossible
/// to reason about: a user cannot meaningfully consent to nineteen checkboxes.
/// These are the distinctions that actually matter — can it see my code, can it
/// change my code, can it reach the network.
enum class Capability {
    ReadWorkspace,    ///< read files in the open project
    WriteWorkspace,   ///< create, modify and delete them
    ReadEditor,       ///< the open document's text and caret
    ModifyEditor,     ///< insert and replace text
    RegisterCommands, ///< contribute to the palette
    ShowUi,           ///< show notifications and panels
    Network,          ///< reach the internet
    RunProcesses,     ///< spawn child processes
};

/// The identifier used in a manifest: "readWorkspace", "network".
[[nodiscard]] QString capabilityId(Capability capability);

/// Parses one, or nothing when the identifier is unknown. An unknown capability
/// is refused rather than ignored: silently dropping it would give the extension
/// less than it asked for and no way to tell.
[[nodiscard]] std::optional<Capability> capabilityFromId(const QString& id);

/// A one-line explanation for the grant prompt, phrased as what the user is
/// agreeing to rather than as an API name.
[[nodiscard]] QString capabilityDescription(Capability capability);

/// Whether granting this needs an explicit decision. Reading the workspace is
/// what every extension does and prompting for it trains people to click yes;
/// writing files or reaching the network genuinely deserves a pause.
[[nodiscard]] bool capabilityIsSensitive(Capability capability);

/// A command an extension contributes to the palette.
struct CommandContribution {
    QString id;      ///< namespaced by the extension: "fmt.formatDocument"
    QString title;
    QString category;
};

/// One extension's manifest.
struct Manifest {
    QString id;          ///< reverse-DNS or a simple slug, unique per install
    QString name;        ///< shown to the user
    QString version;
    QString description;
    QString author;

    /// The program the host launches, relative to the extension's directory.
    QString entryPoint;
    QStringList entryArguments;

    std::vector<Capability> capabilities;
    std::vector<CommandContribution> commands;

    /// File types the extension wants to be activated for. Empty means it
    /// activates when the workspace opens.
    QStringList activationLanguages;

    [[nodiscard]] bool isValid() const
    {
        return !id.isEmpty() && !name.isEmpty() && !entryPoint.isEmpty();
    }

    [[nodiscard]] bool declares(Capability capability) const;

    /// Parses a manifest, failing with a reason rather than a partial result.
    /// A malformed manifest must not produce an extension that half-works.
    [[nodiscard]] static core::Result<Manifest> parse(const QJsonObject& object);

    /// Reads and parses `keys-extension.json` from a directory.
    [[nodiscard]] static core::Result<Manifest> load(const QString& directory);
};

} // namespace keys::extensions
