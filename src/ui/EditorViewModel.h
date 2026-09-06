#pragma once

#include "editor/SyntaxHighlighter.h"
#include "editor/TextDocument.h"
#include "ui/EditorSettings.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>

#include <vector>

namespace keys::ui {

/// What the editor view binds to.
///
/// The view renders only the lines in its viewport, so this exposes the document
/// as an addressable set of lines plus a caret, rather than as one string. A
/// 200,000-line file therefore costs the same to display as a 20-line one: the
/// view asks for the forty lines it can see.
///
/// Key handling lives here rather than in QML so the editing model stays
/// testable without a scene graph, and so the same logic serves a future command
/// palette or macro system.
class EditorViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("EditorViewModel is provided by the application")

    Q_PROPERTY(int lineCount READ lineCount NOTIFY contentsChanged)
    Q_PROPERTY(int cursorLine READ cursorLine NOTIFY cursorChanged)
    Q_PROPERTY(int cursorColumn READ cursorColumn NOTIFY cursorChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY cursorChanged)
    Q_PROPERTY(bool modified READ isModified NOTIFY modifiedChanged)
    Q_PROPERTY(QString path READ path NOTIFY documentChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY documentChanged)
    Q_PROPERTY(bool hasDocument READ hasDocument NOTIFY documentChanged)
    Q_PROPERTY(bool highlighted READ isHighlighted NOTIFY documentChanged)

public:
    /// Takes the editor's resolved settings so indentation follows the user's
    /// preference. Passed in rather than looked up so the view model stays
    /// testable without a settings file.
    explicit EditorViewModel(EditorSettings& settings, QObject* parent = nullptr);

    /// The document being edited. Null until a file is opened.
    void setDocument(editor::TextDocument* document);
    [[nodiscard]] editor::TextDocument* document() const { return m_document; }
    [[nodiscard]] bool hasDocument() const { return m_document != nullptr; }

    [[nodiscard]] int lineCount() const;
    [[nodiscard]] int cursorLine() const;
    [[nodiscard]] int cursorColumn() const;
    [[nodiscard]] bool hasSelection() const;
    [[nodiscard]] bool isModified() const;
    [[nodiscard]] QString path() const;
    [[nodiscard]] QString fileName() const;

    /// One line's text. Called per visible row, so it must stay cheap.
    Q_INVOKABLE QString lineText(int line) const;

    /// One line as rich text, with syntax colouring applied.
    ///
    /// Rich text rather than a model of spans: QML's Text renders it directly,
    /// where spans would need a Repeater per line and turn a viewport of forty
    /// rows into hundreds of objects. Highlighting is per line and on demand, so
    /// a 200,000-line file costs nothing until those lines are drawn.
    Q_INVOKABLE QString highlightedLine(int line) const;

    /// Whether the open document has syntax rules. False means the view draws
    /// plain text, which is cheaper and avoids escaping cost for no benefit.
    [[nodiscard]] bool isHighlighted() const { return m_highlighted; }

    /// How much of `line` is selected, as [startColumn, endColumn). Returns
    /// an empty range when the line has no selection. The view draws one
    /// highlight rectangle per line from this rather than reasoning about
    /// multi-line ranges itself.
    Q_INVOKABLE int selectionStartOn(int line) const;
    Q_INVOKABLE int selectionEndOn(int line) const;
    Q_INVOKABLE bool lineHasSelection(int line) const;

    // ---- Input ------------------------------------------------------------
    //
    // The view forwards raw input here; all editing decisions are made in C++.

    Q_INVOKABLE void insertText(const QString& text);
    Q_INVOKABLE void insertNewline();
    Q_INVOKABLE void insertTab();
    Q_INVOKABLE void deleteBackward();
    Q_INVOKABLE void deleteForward();

    Q_INVOKABLE void moveCursor(int line, int column, bool extend = false);
    Q_INVOKABLE void moveLeft(bool extend, bool byWord);
    Q_INVOKABLE void moveRight(bool extend, bool byWord);
    Q_INVOKABLE void moveUp(bool extend);
    Q_INVOKABLE void moveDown(bool extend);
    Q_INVOKABLE void moveToLineStart(bool extend);
    Q_INVOKABLE void moveToLineEnd(bool extend);
    Q_INVOKABLE void moveToDocumentStart(bool extend);
    Q_INVOKABLE void moveToDocumentEnd(bool extend);

    /// Page movement needs the viewport height, which only the view knows.
    Q_INVOKABLE void movePage(int lines, bool extend);

    Q_INVOKABLE void selectAll();
    Q_INVOKABLE QString selectedText() const;

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    Q_INVOKABLE void copy();
    Q_INVOKABLE void cut();
    Q_INVOKABLE void paste();

signals:
    void contentsChanged();
    void cursorChanged();
    void modifiedChanged();
    void documentChanged();

    /// Asks the view to scroll the caret into sight after a movement or edit.
    void scrollToCursorRequested();

private:
    EditorSettings& m_settings;
    editor::TextDocument* m_document = nullptr;

    /// The colour for a token kind, taken from the active Theme so highlighting
    /// follows a light/dark switch rather than baking one palette in. Empty for
    /// Plain, which needs no span at all.
    [[nodiscard]] static QString colourFor(editor::TokenKind kind);

    /// Picks the highlighter's language from the document's current path.
    void applyLanguage();

    editor::SyntaxHighlighter m_highlighter;
    bool m_highlighted = false;

    /// The state each line ends in, so a line can be highlighted without
    /// re-scanning the file above it. Grown lazily: entry N is valid only once
    /// every line before it has been seen, which the sequential draw order of a
    /// viewport guarantees in practice.
    mutable std::vector<editor::LineState> m_lineStates;
};

} // namespace keys::ui
