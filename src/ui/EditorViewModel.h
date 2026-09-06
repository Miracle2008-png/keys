#pragma once

#include "editor/DocumentSearch.h"
#include "editor/FoldModel.h"
#include "editor/SyntaxHighlighter.h"
#include "editor/TextDocument.h"
#include "ui/EditorSettings.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantList>

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

    /// Bumped on every content change.
    ///
    /// `lineText` and `highlightedLine` are functions, and QML cannot know that
    /// a function's result has changed - a binding that calls one is evaluated
    /// once and never again. Naming this property inside such a binding gives
    /// the engine a dependency it *can* track, so an edit repaints the line
    /// instead of leaving a stale one on screen while the buffer moves on.
    Q_PROPERTY(int revision READ revision NOTIFY contentsChanged)
    Q_PROPERTY(int cursorLine READ cursorLine NOTIFY cursorChanged)
    Q_PROPERTY(int cursorColumn READ cursorColumn NOTIFY cursorChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY cursorChanged)
    Q_PROPERTY(bool modified READ isModified NOTIFY modifiedChanged)
    Q_PROPERTY(QString path READ path NOTIFY documentChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY documentChanged)
    Q_PROPERTY(bool hasDocument READ hasDocument NOTIFY documentChanged)
    Q_PROPERTY(bool highlighted READ isHighlighted NOTIFY documentChanged)

    /// The language's display name, for the status bar. Empty when Keys has no
    /// rules for the file, which the bar reports as plain text rather than
    /// guessing at a name.
    Q_PROPERTY(QString languageName READ languageName NOTIFY documentChanged)

    // ---- Find and replace --------------------------------------------------

    Q_PROPERTY(bool findOpen READ isFindOpen NOTIFY findChanged)
    Q_PROPERTY(bool replaceOpen READ isReplaceOpen NOTIFY findChanged)
    Q_PROPERTY(QString findQuery READ findQuery WRITE setFindQuery NOTIFY findChanged)
    Q_PROPERTY(bool findCaseSensitive READ findCaseSensitive
                   WRITE setFindCaseSensitive NOTIFY findChanged)
    Q_PROPERTY(bool findWholeWord READ findWholeWord
                   WRITE setFindWholeWord NOTIFY findChanged)
    Q_PROPERTY(bool findRegex READ findRegex WRITE setFindRegex NOTIFY findChanged)

    /// How many matches, and which one is current (one-based, 0 when none).
    /// The bar shows "3 of 47", which is most of what makes a find useful.
    Q_PROPERTY(int findCount READ findCount NOTIFY findChanged)
    Q_PROPERTY(int findCurrent READ findCurrent NOTIFY findChanged)

    /// False while the query is a malformed regex, so the field can say so
    /// rather than appearing to match nothing.
    Q_PROPERTY(bool findQueryValid READ isFindQueryValid NOTIFY findChanged)

    // ---- Bracket matching --------------------------------------------------

    /// Where the bracket under or before the caret is, and where its partner
    /// is, or -1 when the caret is not on one. Two positions rather than a
    /// range: they can be thousands of lines apart.
    Q_PROPERTY(int bracketLine READ bracketLine NOTIFY cursorChanged)
    Q_PROPERTY(int bracketColumn READ bracketColumn NOTIFY cursorChanged)
    Q_PROPERTY(int matchLine READ matchLine NOTIFY cursorChanged)
    Q_PROPERTY(int matchColumn READ matchColumn NOTIFY cursorChanged)

    /// False when the caret is on a bracket whose partner is missing, so the
    /// view can mark it as unbalanced rather than simply not highlighting.
    Q_PROPERTY(bool bracketMatched READ isBracketMatched NOTIFY cursorChanged)

    /// How many carets there are. The view draws extra ones only when this is
    /// above 1, which is almost never - the cost of multi-cursor should be
    /// nothing at all when it is not in use.
    Q_PROPERTY(int cursorCount READ cursorCount NOTIFY cursorChanged)

    // ---- Folding -----------------------------------------------------------

    /// How many rows the view draws. Equal to lineCount with nothing folded,
    /// which is the case the editor is in almost all of the time.
    Q_PROPERTY(int visibleLineCount READ visibleLineCount NOTIFY contentsChanged)

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

    /// Comments or uncomments the selected lines, or the caret's line.
    ///
    /// Toggling on the whole block rather than per line: if every line is
    /// already commented the block is uncommented, otherwise all of it is
    /// commented. Deciding line by line would leave a mixed selection half
    /// commented, which is never what was wanted.
    Q_INVOKABLE void toggleLineComment();
    Q_INVOKABLE QString selectedText() const;

    [[nodiscard]] int revision() const { return m_revision; }

    [[nodiscard]] QString languageName() const;

    [[nodiscard]] int bracketLine() const { return m_bracket.line; }
    [[nodiscard]] int bracketColumn() const { return m_bracket.column; }
    [[nodiscard]] int matchLine() const { return m_bracketMatch.line; }
    [[nodiscard]] int matchColumn() const { return m_bracketMatch.column; }
    [[nodiscard]] bool isBracketMatched() const { return m_bracketMatched; }

    [[nodiscard]] int cursorCount() const;

    [[nodiscard]] int visibleLineCount() const;

    /// The document line a visible row shows. The view counts rows; everything
    /// else in the editor counts lines, and this is the only bridge.
    Q_INVOKABLE int documentLineFor(int visibleRow) const;

    /// Whether a line starts a foldable region, and whether it is folded - what
    /// the gutter needs to decide between a marker, an arrow, and nothing.
    Q_INVOKABLE bool isFoldable(int line) const;
    Q_INVOKABLE bool isFolded(int line) const;

    /// How many lines a folded region hides, so the row can say so rather than
    /// silently swallowing them.
    Q_INVOKABLE int foldedLineCount(int line) const;

    Q_INVOKABLE void toggleFold(int line);
    Q_INVOKABLE void foldAll();
    Q_INVOKABLE void unfoldAll();

    /// The columns of every caret on one line, for the view to draw. Empty on
    /// a line with none, which is most of them.
    Q_INVOKABLE QVariantList cursorsOnLine(int line) const;

    /// Adds a caret above or below the outermost one, and drops all but the
    /// primary. Escape clears, which is what every editor binds.
    Q_INVOKABLE void addCursorAbove();
    Q_INVOKABLE void addCursorBelow();
    Q_INVOKABLE void clearExtraCursors();

    /// Adds a caret at a point the user clicked, for Alt+Click.
    Q_INVOKABLE void addCursorAt(int line, int column);

    // ---- Find and replace --------------------------------------------------

    [[nodiscard]] bool isFindOpen() const { return m_findOpen; }
    [[nodiscard]] bool isReplaceOpen() const { return m_replaceOpen; }
    [[nodiscard]] QString findQuery() const { return m_findOptions.query; }
    [[nodiscard]] bool findCaseSensitive() const { return m_findOptions.caseSensitive; }
    [[nodiscard]] bool findWholeWord() const { return m_findOptions.wholeWord; }
    [[nodiscard]] bool findRegex() const { return m_findOptions.regularExpression; }
    [[nodiscard]] int findCount() const { return m_find.count(); }
    [[nodiscard]] int findCurrent() const { return m_find.currentIndex() + 1; }
    [[nodiscard]] bool isFindQueryValid() const { return m_find.isQueryValid(); }

    void setFindQuery(const QString& query);
    void setFindCaseSensitive(bool enabled);
    void setFindWholeWord(bool enabled);
    void setFindRegex(bool enabled);

    /// Opens the bar. `withReplace` opens it with the replace field showing;
    /// the two are one bar because they are one task.
    Q_INVOKABLE void openFind(bool withReplace = false);
    Q_INVOKABLE void closeFind();

    /// Steps through matches, wrapping at either end.
    Q_INVOKABLE void findNext();
    Q_INVOKABLE void findPrevious();

    /// Replaces the current match, or every match, with `replacement`.
    /// Replacing all is one undo step: it is one action as far as the user is
    /// concerned, and forty presses of Ctrl+Z to undo it would be absurd.
    Q_INVOKABLE void replaceCurrent(const QString& replacement);
    Q_INVOKABLE void replaceAll(const QString& replacement);

    /// Whether a match covers this line, and where - so the view can paint
    /// every hit rather than only the current one.
    Q_INVOKABLE QVariantList matchesOnLine(int line) const;

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    Q_INVOKABLE void copy();
    Q_INVOKABLE void cut();
    Q_INVOKABLE void paste();

signals:
    void contentsChanged();
    void findChanged();
    void cursorChanged();
    void modifiedChanged();
    void documentChanged();

    /// Asks the view to scroll the caret into sight after a movement or edit.
    void scrollToCursorRequested();

private:
    /// Counts content changes; only its identity matters, never its value.
    int m_revision = 0;

    /// Recomputes the bracket under the caret and its partner. Called on every
    /// caret move, so it stops at a bound rather than scanning a whole file:
    /// an unmatched brace in a large document must not cost a frame.
    void updateBracketMatch();

    /// Re-runs the search and moves the caret to the current match.
    void refreshFind(bool keepPosition = false);

    /// Puts the caret on the current match and selects it, so Enter in the
    /// find field leaves the editor ready to type over the hit.
    void revealCurrentMatch();

    editor::FoldModel m_folds;

    editor::Position m_bracket{-1, -1};
    editor::Position m_bracketMatch{-1, -1};
    bool m_bracketMatched = false;

    editor::DocumentSearch m_find;
    editor::FindOptions m_findOptions;
    bool m_findOpen = false;
    bool m_replaceOpen = false;

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
