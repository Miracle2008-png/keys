# Keys — gap analysis and plan

Measured against VS Code, CLion and RustRover, from an audit of the source on
2026-09-06. Everything marked *missing* was verified absent by inspection, not
assumed. Everything marked *built* was verified present.

The order below is the order of work. It runs feel first, then reach: an editor
that feels wrong is not rescued by having more features, and every item added
before the feel is right has to be re-tuned afterwards.

---

## Phase 1 — How it feels

The three things that make Keys read as unfinished next to CLion, none of which
are about features.

### 1.1 Fonts

`resources/fonts/` exists and is empty, so Keys falls back to Segoe UI and
Cascadia Mono. CLion ships Inter for its interface and JetBrains Mono for code,
and that pairing is most of what "feels like CLion" means.

- Bundle **Inter** (UI) and **JetBrains Mono** (code). Both are OFL, so they can
  ship with the application.
- Load them in `main.cpp` via `QFontDatabase::addApplicationFont` before the QML
  engine starts, so the first frame is already correct.
- `Fonts.qml` keeps its per-platform fallback stack for the case where a bundled
  face fails to load.
- Tune weights: Inter needs slightly tighter letter-spacing at small sizes than
  Segoe UI does, and the existing `-0.3`/`-0.5` values were chosen against Segoe.

**Why first:** it is the largest change to how the product reads, and the
smallest to make.

### 1.2 Motion

18 of 42 QML files animate. The policy layer (`AnimationPolicy`, with Full /
Reduced / Off) already exists and is respected where it is used — the gap is
coverage, not machinery.

Missing transitions, in the order they are noticed:

| Where | What |
|---|---|
| Opening a file | Tab appears and content swaps with no transition |
| Closing a tab | Vanishes; remaining tabs jump to fill the space |
| Switching views | Explorer → Search → Git snaps |
| Dialogs | New File, Save As, About all appear instantly |
| Panels | Run and Problems panels have no reveal |
| Welcome → project | The whole window changes in one frame |
| Settings | Opens as an instant replacement |
| Explorer | Expanding a folder is instantaneous |

Every one respects `App.animationDuration` / `fastAnimationDuration`, so Off
stays genuinely off.

### 1.3 Density and rhythm

Once the font lands, the spacing scale needs a pass against it. CLion's menus are
tighter than Keys' current 30px rows; its tool windows carry more per vertical
inch without feeling cramped.

---

## Phase 2 — The menus

Keys has File, Edit, View, Build, Help. CLion has File, Edit, View, Navigate,
Code, Refactor, Build, Run, Tools, VCS, Window, Help.

The rule from the original brief holds: **an item exists only if it works.** The
list below is drawn from what is already implemented in C++ and not yet exposed.

### 2.1 Deepen File

Built and unexposed: none. These need small additions.

- **New ▸** submenu (File, Folder, and later From Template)
- **Recent Projects ▸** — the model exists (`RecentProjects`, with pinning)
- **Save All** — `EditorLayout` knows every open document
- **Reload from Disk** — `FileWatcher` already tracks external changes
- **File Properties ▸** — path, size, encoding, line ending
- **Close Project** *(exists)*, **Settings** *(exists)*, **Exit** *(exists)*

### 2.2 Deepen Edit

Needs new editor commands, all local to `TextDocument`:

- Copy Path / Reference
- Duplicate Line, Join Lines, Sort Lines, Reverse Lines
- Toggle Case, Transpose
- Indent / Unindent Selection
- Column Selection Mode *(depends on multi-cursor, Phase 3)*
- **Find and Replace in file** — see 3.1; the single largest daily gap

### 2.3 Navigate *(new menu)*

Built and unexposed:

- Go to Definition — `LanguageModel::goToDefinition()`
- Go to File — the palette's quick-open
- Go to Line — needs a small dialog
- Back / Forward — needs a navigation history stack
- Next / Previous Error — `LanguageModel` holds diagnostics
- Switch Header/Source — a filename rule for C and C++

### 2.4 Code *(new menu)*

- Completion — `requestCompletion()`, bound to Ctrl+Space, unexposed in a menu
- Quick Documentation — `requestHover()` **is implemented in C++ and never
  called from QML**; hover tooltips do not appear anywhere today
- Reformat — needs `textDocument/formatting` in the LSP client
- Comment / Uncomment Lines — local to the editor

### 2.5 Run *(new menu)*

Everything here is built and only reachable through the Build menu or a panel:

- Run, Build, Stop — `RunPanelModel`
- Start / Stop Debugging, **Step Over, Step Into, Step Out, Resume, Pause** —
  `DebugModel` has all of them; only start and stop are exposed
- Toggle Breakpoint — `DebugModel::toggleBreakpoint()`

### 2.6 VCS *(new menu)*

`SourceControlModel` has all of this, reachable only from the side panel:

- Stage / Unstage / Discard, Stage All, Unstage All
- Commit
- Refresh

### 2.7 Window *(new menu)*

- Split Editor *(exists in View)*, Close Tab, Close Others, Close All
- Next / Previous Tab — `TabBarModel` supports it

---

## Phase 3 — Editor features

Verified absent. These are what separate a text editor from an IDE editor, and
each is a real piece of work rather than a menu entry.

### 3.1 Find and Replace in a file *(highest value)*

Project-wide search exists (`TextSearch`, the Search panel). Ctrl+F **inside a
document does not exist at all**. Needs: an inline find bar, match highlighting,
next/previous, replace and replace-all, and case/word/regex toggles.

### 3.2 Multiple cursors

No `std::vector<Cursor>` anywhere — `TextDocument` holds exactly one caret.
Adding this touches the buffer, the undo stack and every movement command, so it
is a structural change and should land as one deliberate piece.

### 3.3 Code folding

No folding model. Needs fold regions (from the language server where it offers
them, from indentation where it does not), gutter markers, and a line index that
understands hidden ranges.

### 3.4 Bracket matching and auto-indent

Neither exists. Both are small next to the two above, and both are noticed
constantly while typing.

### 3.5 Rename symbol

`textDocument/rename` is not implemented in the LSP client.

### 3.6 Smaller, still missing

Bookmarks · Minimap · Word wrap toggle · Zoom in and out · Column selection ·
Multi-file diff

---

## Phase 4 — Missing surfaces

### 4.1 Terminal panel

`terminal/` holds a complete VT parser, screen model and session — and **there is
no terminal anywhere in the UI**. The ConPTY attachment is blocked on this
machine (reproduced with a bare Microsoft-sample probe and with `pywinpty`, so it
is the environment rather than Keys), but the panel, its rendering and its
scrollback can all be built and tested against a pipe-backed session.

### 4.2 Problems panel

Diagnostics arrive from the language server and are drawn in the gutter. There is
no list of them to work through.

### 4.3 Keybinding customisation

Shortcuts are hardcoded in QML. No settings page, no conflict detection, no
per-command binding. Every serious IDE has this.

### 4.4 Local History

CLion keeps its own history independent of git. Nothing equivalent here.

---

## Phase 5 — Further out

- Refactor menu — extract function, extract variable, change signature
- Task templates and run configurations beyond build/run
- Plugin marketplace UI — the extension host exists and is out-of-process
- Remote development
- Structure view — the symbol outline of the current file
- Call hierarchy, type hierarchy
- Multi-root workspaces

---

## What is already solid

Not everything is a gap. Built and verified working:

Piece-table buffer with an incremental line index (0.06 ms per keystroke in a
200k-line file) · syntax highlighting for 17 languages · LSP client with
completion and diagnostics · DAP debugger with breakpoints and stepping · git
integration · fuzzy file and command search · out-of-process extension host with
capability grants · editor splits and tabs · themes with a full light and dark
palette · settings that persist · an NSIS installer.

The foundation is sound. What is missing is reach and polish, in that order.
