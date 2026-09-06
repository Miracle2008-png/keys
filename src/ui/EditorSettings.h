#pragma once

#include "config/Settings.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>

namespace keys::ui {

/// Editor appearance and indentation, resolved from settings.
///
/// The same reasoning as AnimationPolicy: the views read one object rather than
/// each reaching into Settings and parsing keys themselves. That keeps the
/// mapping from key to behaviour in a single place, and means a setting is
/// honoured everywhere it applies instead of in whichever view someone
/// remembered to wire.
///
/// Exposed to QML as `EditorConfig` rather than `Editor`, which would collide
/// with the per-pane EditorViewModel instances the panes already bind to.
class EditorSettings : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(EditorConfig)
    QML_SINGLETON

    Q_PROPERTY(qreal fontSize READ fontSize NOTIFY changed)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY changed)

    /// The height of one line in pixels, already multiplied out. QML would
    /// otherwise repeat `fontMetrics.height * multiplier` in every view and the
    /// two could disagree.
    Q_PROPERTY(qreal lineHeightFactor READ lineHeightFactor NOTIFY changed)

    Q_PROPERTY(int tabSize READ tabSize NOTIFY changed)
    Q_PROPERTY(bool insertSpaces READ insertSpaces NOTIFY changed)

    /// What the editor draws. Each of these is read by CodeEditor; a setting
    /// with no consumer would appear in the settings page as a control that
    /// changes nothing, which is worse than the setting not existing.
    Q_PROPERTY(bool showLineNumbers READ showLineNumbers NOTIFY changed)
    Q_PROPERTY(bool highlightCurrentLine READ highlightCurrentLine NOTIFY changed)
    Q_PROPERTY(bool showIndentGuides READ showIndentGuides NOTIFY changed)
    Q_PROPERTY(bool showWhitespace READ showWhitespace NOTIFY changed)
    Q_PROPERTY(bool caretBlink READ caretBlink NOTIFY changed)
    Q_PROPERTY(bool scrollPastEnd READ scrollPastEnd NOTIFY changed)

public:
    explicit EditorSettings(config::Settings& settings, QObject* parent = nullptr);

    static void setInstance(EditorSettings* instance);
    static EditorSettings* create(QQmlEngine* engine, QJSEngine* scriptEngine);

    /// In points, converted from the design's CSS pixels the same way Metrics
    /// does it — so a size set here renders at the size the user asked for on a
    /// high-DPI display rather than being pinned to device pixels.
    [[nodiscard]] qreal fontSize() const;

    /// The configured family, or the platform default when unset. Empty is the
    /// schema's way of saying "whatever the platform uses", which the view
    /// cannot resolve for itself.
    [[nodiscard]] QString fontFamily() const;

    [[nodiscard]] qreal lineHeightFactor() const { return m_lineHeight; }
    [[nodiscard]] int tabSize() const { return m_tabSize; }
    [[nodiscard]] bool insertSpaces() const { return m_insertSpaces; }

    [[nodiscard]] bool showLineNumbers() const { return m_showLineNumbers; }
    [[nodiscard]] bool highlightCurrentLine() const { return m_highlightCurrentLine; }
    [[nodiscard]] bool showIndentGuides() const { return m_showIndentGuides; }
    [[nodiscard]] bool showWhitespace() const { return m_showWhitespace; }
    [[nodiscard]] bool caretBlink() const { return m_caretBlink; }
    [[nodiscard]] bool scrollPastEnd() const { return m_scrollPastEnd; }

    /// What a save rewrites, if anything. Read by the workspace when it writes
    /// a file, not by the view.
    [[nodiscard]] bool trimTrailingWhitespaceOnSave() const {
        return m_trimTrailingWhitespace;
    }
    [[nodiscard]] bool ensureNewlineAtEndOnSave() const {
        return m_ensureNewlineAtEnd;
    }

    /// What pressing Tab inserts: `tabSize` spaces, or one tab character.
    /// Lives here rather than in the view model so the two cannot disagree.
    [[nodiscard]] QString indentString() const;

signals:
    void changed();

private:
    void applyFromSettings();

    config::Settings& m_settings;

    qreal m_fontSize = 13.0;
    QString m_fontFamily;

    bool m_showLineNumbers = true;
    bool m_highlightCurrentLine = true;
    bool m_showIndentGuides = true;
    bool m_showWhitespace = false;
    bool m_caretBlink = true;
    bool m_scrollPastEnd = true;
    bool m_trimTrailingWhitespace = false;
    bool m_ensureNewlineAtEnd = false;
    qreal m_lineHeight = 1.6;
    int m_tabSize = 4;
    bool m_insertSpaces = true;
};

} // namespace keys::ui
