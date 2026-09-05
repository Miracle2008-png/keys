#include "langsvc/LspTypes.h"

#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QUrl>

namespace keys::langsvc {

QJsonObject LspPosition::toJson() const
{
    return {{QStringLiteral("line"), line},
            {QStringLiteral("character"), character}};
}

LspPosition LspPosition::fromJson(const QJsonObject& object)
{
    LspPosition position;
    position.line = object.value(QStringLiteral("line")).toInt();
    position.character = object.value(QStringLiteral("character")).toInt();
    return position;
}

QJsonObject LspRange::toJson() const
{
    return {{QStringLiteral("start"), start.toJson()},
            {QStringLiteral("end"), end.toJson()}};
}

LspRange LspRange::fromJson(const QJsonObject& object)
{
    LspRange range;
    range.start = LspPosition::fromJson(object.value(QStringLiteral("start")).toObject());
    range.end = LspPosition::fromJson(object.value(QStringLiteral("end")).toObject());
    return range;
}

Diagnostic Diagnostic::fromJson(const QJsonObject& object)
{
    Diagnostic diagnostic;
    diagnostic.range = LspRange::fromJson(object.value(QStringLiteral("range")).toObject());
    diagnostic.message = object.value(QStringLiteral("message")).toString();
    diagnostic.source = object.value(QStringLiteral("source")).toString();

    // Severity is optional; LSP says an absent one means the client decides.
    // Error is the safe default: under-reporting a real error is worse than
    // over-reporting a hint.
    const int severity = object.value(QStringLiteral("severity")).toInt(1);
    diagnostic.severity = severity >= 1 && severity <= 4
                              ? static_cast<DiagnosticSeverity>(severity)
                              : DiagnosticSeverity::Error;

    // The code is a string or a number depending on the server.
    const QJsonValue code = object.value(QStringLiteral("code"));
    if (code.isString()) {
        diagnostic.code = code.toString();
    } else if (code.isDouble()) {
        diagnostic.code = QString::number(code.toInt());
    }
    return diagnostic;
}

CompletionItem CompletionItem::fromJson(const QJsonObject& object)
{
    CompletionItem item;
    item.label = object.value(QStringLiteral("label")).toString();
    item.detail = object.value(QStringLiteral("detail")).toString();
    item.insertText = object.value(QStringLiteral("insertText")).toString();
    item.sortText = object.value(QStringLiteral("sortText")).toString();

    const int kind = object.value(QStringLiteral("kind")).toInt(1);
    item.kind = kind >= 1 && kind <= 25 ? static_cast<CompletionKind>(kind)
                                        : CompletionKind::Text;

    // Documentation is a plain string or a MarkupContent object.
    const QJsonValue documentation = object.value(QStringLiteral("documentation"));
    if (documentation.isString()) {
        item.documentation = documentation.toString();
    } else if (documentation.isObject()) {
        item.documentation =
            documentation.toObject().value(QStringLiteral("value")).toString();
    }
    return item;
}

Location Location::fromJson(const QJsonObject& object)
{
    Location location;
    location.uri = object.value(QStringLiteral("uri")).toString();
    location.range = LspRange::fromJson(object.value(QStringLiteral("range")).toObject());
    return location;
}

QString pathToUri(const QString& path)
{
    if (path.isEmpty()) {
        return QString();
    }
    // QUrl::fromLocalFile handles the drive-letter and separator rules, which
    // are the parts a hand-rolled version gets wrong.
    return QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()).toString();
}

QString uriToPath(const QString& uri)
{
    if (uri.isEmpty()) {
        return QString();
    }
    return QUrl(uri).toLocalFile();
}

QString languageIdForPath(const QString& path)
{
    // LSP's own identifiers. Only extensions Keys can name are mapped: an
    // unknown one returns empty rather than a guess, which stops a document
    // being opened against a server that cannot read it.
    static const QHash<QString, QString> byExtension = {
        {QStringLiteral("c"), QStringLiteral("c")},
        {QStringLiteral("h"), QStringLiteral("cpp")},
        {QStringLiteral("cc"), QStringLiteral("cpp")},
        {QStringLiteral("cpp"), QStringLiteral("cpp")},
        {QStringLiteral("cxx"), QStringLiteral("cpp")},
        {QStringLiteral("hpp"), QStringLiteral("cpp")},
        {QStringLiteral("hxx"), QStringLiteral("cpp")},
        {QStringLiteral("cs"), QStringLiteral("csharp")},
        {QStringLiteral("go"), QStringLiteral("go")},
        {QStringLiteral("java"), QStringLiteral("java")},
        {QStringLiteral("js"), QStringLiteral("javascript")},
        {QStringLiteral("jsx"), QStringLiteral("javascriptreact")},
        {QStringLiteral("json"), QStringLiteral("json")},
        {QStringLiteral("md"), QStringLiteral("markdown")},
        {QStringLiteral("py"), QStringLiteral("python")},
        {QStringLiteral("rs"), QStringLiteral("rust")},
        {QStringLiteral("ts"), QStringLiteral("typescript")},
        {QStringLiteral("tsx"), QStringLiteral("typescriptreact")},
        {QStringLiteral("qml"), QStringLiteral("qml")},
        {QStringLiteral("sh"), QStringLiteral("shellscript")},
        {QStringLiteral("toml"), QStringLiteral("toml")},
        {QStringLiteral("yaml"), QStringLiteral("yaml")},
        {QStringLiteral("yml"), QStringLiteral("yaml")},
    };

    return byExtension.value(QFileInfo(path).suffix().toLower());
}

} // namespace keys::langsvc
