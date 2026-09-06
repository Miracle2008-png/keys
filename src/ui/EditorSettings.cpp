#include "ui/EditorSettings.h"

#include "ui/Metrics.h"


namespace keys::ui {
namespace {

constexpr auto kFontSizeKey = "editor.fontSize";
constexpr auto kFontFamilyKey = "editor.fontFamily";
constexpr auto kLineHeightKey = "editor.lineHeight";
constexpr auto kTabSizeKey = "editor.tabSize";
constexpr auto kInsertSpacesKey = "editor.insertSpaces";

/// The instance main() publishes for QML; see Theme.cpp for the reasoning.
EditorSettings* g_instance = nullptr;

} // namespace

void EditorSettings::setInstance(EditorSettings* instance)
{
    g_instance = instance;
}

EditorSettings* EditorSettings::create(QQmlEngine* engine, QJSEngine* scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    Q_ASSERT_X(g_instance, "EditorSettings::create",
               "EditorSettings::setInstance was not called");

    QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
    return g_instance;
}

EditorSettings::EditorSettings(config::Settings& settings, QObject* parent)
    : QObject(parent), m_settings(settings)
{
    applyFromSettings();

    connect(&m_settings, &config::Settings::changed, this,
            [this](const QString& key, const QVariant&) {
                if (key == QLatin1String(kFontSizeKey)
                    || key == QLatin1String(kFontFamilyKey)
                    || key == QLatin1String(kLineHeightKey)
                    || key == QLatin1String(kTabSizeKey)
                    || key == QLatin1String(kInsertSpacesKey)) {
                    applyFromSettings();
                }
            });
}

qreal EditorSettings::fontSize() const
{
    // The setting is in CSS pixels, matching the design and what the user sees
    // in the settings UI; QML assigns it to font.pointSize. See Metrics.
    return m_fontSize * Metrics::kPxToPt;
}

QString EditorSettings::fontFamily() const
{
    if (!m_fontFamily.isEmpty()) {
        return m_fontFamily;
    }

    // Empty means "the face Keys ships". JetBrains Mono is bundled and
    // registered at startup, so it resolves on every platform - the editor no
    // longer inherits whatever monospaced font a machine happens to have, and
    // code looks the same everywhere. Resolved here rather than in QML so the
    // default is one decision instead of one per view.
    return QStringLiteral("JetBrains Mono");
}

QString EditorSettings::indentString() const
{
    return m_insertSpaces ? QString(m_tabSize, QLatin1Char(' '))
                          : QStringLiteral("\t");
}

void EditorSettings::applyFromSettings()
{
    // Everything is read, then compared as a block. The alternative - a
    // condition naming every field - grew a term per setting and would
    // eventually miss one, which shows as a preference that saves and does not
    // take effect until the next launch.
    const qreal fontSize = m_settings.doubleValue(QLatin1String(kFontSizeKey));
    const QString fontFamily = m_settings.stringValue(QLatin1String(kFontFamilyKey));
    const qreal lineHeight = m_settings.doubleValue(QLatin1String(kLineHeightKey));
    const int tabSize = m_settings.intValue(QLatin1String(kTabSizeKey));
    const bool insertSpaces = m_settings.boolValue(QLatin1String(kInsertSpacesKey));

    const bool showLineNumbers =
        m_settings.boolValue(QStringLiteral("editor.showLineNumbers"));
    const bool highlightCurrentLine =
        m_settings.boolValue(QStringLiteral("editor.highlightCurrentLine"));
    const bool showIndentGuides =
        m_settings.boolValue(QStringLiteral("editor.showIndentGuides"));
    const bool showWhitespace =
        m_settings.boolValue(QStringLiteral("editor.showWhitespace"));
    const bool caretBlink =
        m_settings.boolValue(QStringLiteral("editor.caretBlink"));
    const bool scrollPastEnd =
        m_settings.boolValue(QStringLiteral("editor.scrollPastEnd"));
    const bool trimTrailingWhitespace =
        m_settings.boolValue(QStringLiteral("editor.trimTrailingWhitespaceOnSave"));
    const bool ensureNewlineAtEnd =
        m_settings.boolValue(QStringLiteral("editor.ensureNewlineAtEndOnSave"));

    const bool unchanged =
        qFuzzyCompare(fontSize, m_fontSize) && fontFamily == m_fontFamily
        && qFuzzyCompare(lineHeight, m_lineHeight) && tabSize == m_tabSize
        && insertSpaces == m_insertSpaces
        && showLineNumbers == m_showLineNumbers
        && highlightCurrentLine == m_highlightCurrentLine
        && showIndentGuides == m_showIndentGuides
        && showWhitespace == m_showWhitespace
        && caretBlink == m_caretBlink
        && scrollPastEnd == m_scrollPastEnd
        && trimTrailingWhitespace == m_trimTrailingWhitespace
        && ensureNewlineAtEnd == m_ensureNewlineAtEnd;

    if (unchanged) {
        return;
    }

    m_fontSize = fontSize;
    m_fontFamily = fontFamily;
    m_lineHeight = lineHeight;
    m_tabSize = tabSize;
    m_insertSpaces = insertSpaces;

    m_showLineNumbers = showLineNumbers;
    m_highlightCurrentLine = highlightCurrentLine;
    m_showIndentGuides = showIndentGuides;
    m_showWhitespace = showWhitespace;
    m_caretBlink = caretBlink;
    m_scrollPastEnd = scrollPastEnd;
    m_trimTrailingWhitespace = trimTrailingWhitespace;
    m_ensureNewlineAtEnd = ensureNewlineAtEnd;

    emit changed();
}

} // namespace keys::ui
