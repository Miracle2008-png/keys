# Keys — Architecture

Status: living document. Revised when a decision changes, not appended to.

Keys is a native desktop IDE built on C++23, Qt 6.10 and QML. This document defines
module boundaries, dependency rules, threading, and the decisions that shape them.
Read this before adding a module or crossing a layer.

---

## 1. Principles

1. **The editor is the product.** Every other subsystem exists to keep the editor
   fast and the developer oriented. Chrome stays quiet.
2. **Never block the UI thread.** Filesystem, git, indexing, process I/O and search
   all run off-thread. The UI thread does layout, input and paint. Nothing else.
3. **Core does not know about UI.** `core/` .. `extensions/` are Qt Core only — no
   QtGui, no QtQuick. This is enforced by CMake link targets, not by convention.
4. **Interfaces at module seams.** Modules talk through narrow abstract interfaces
   so implementations can be swapped and tested without the rest of the app.
5. **No placeholder disguised as working.** A feature is either implemented and
   tested, or visibly absent. Nothing fakes success.

---

## 2. Layering

Dependencies point downward only. A module may depend on anything strictly below it.
Sideways dependencies within a layer are forbidden except where noted.

```
              ┌─────────────────────────────────────────────┐
   Layer 4    │  app          (composition root, main())    │
              ├─────────────────────────────────────────────┤
   Layer 3    │  ui           (QML, view models, theme)     │
              ├─────────────────────────────────────────────┤
              │  editor    langsvc   terminal   vcs         │
   Layer 2    │  buildrun  debugger  extensions  workspace  │
              ├─────────────────────────────────────────────┤
   Layer 1    │  project   filesystem   process   config    │
              ├─────────────────────────────────────────────┤
   Layer 0    │  core   (types, result, log, events, tasks) │
              └─────────────────────────────────────────────┘
```

**The rule that matters:** `ui` is the only module permitted to link QtQuick/QtGui.
Everything below it links Qt Core alone. If a core module needs to notify the UI it
emits a signal or posts an event; it never reaches upward.

`workspace` sits at layer 2 because it composes project + editor state; it is the
model the UI binds to for "what is open right now".

---

## 3. Modules

| Module | Responsibility | Must not |
|---|---|---|
| `core` | Result/error types, logging, event bus, task scheduler, string & path utils | Depend on anything |
| `filesystem` | Async file read/write, directory enumeration, file watching, atomic saves | Interpret file contents |
| `process` | Child process spawn, lifecycle, stdio pumping, kill trees, PTY host | Know about git or shells specifically |
| `config` | Layered settings (default → user → workspace), schema, change notification | Store UI widget state |
| `project` | Project root detection, metadata, ignore rules, file index | Own open editors |
| `workspace` | Open editors, tab/group layout, session persistence, active state | Do file I/O directly |
| `editor` | Text buffer, cursors, selections, undo, highlighting, folding model | Render pixels |
| `langsvc` | LSP client: transport, lifecycle, capability negotiation, request routing | Assume one language |
| `terminal` | PTY sessions, VT parsing, screen buffer, session management | Draw the terminal |
| `vcs` | Git repository detection, status, staging, commits, branches, diff | Block on subprocess |
| `buildrun` | Run/build configuration abstraction, task execution, output routing | Hard-code a language |
| `debugger` | DAP client: breakpoints, stack, variables, stepping | Fake a debug session |
| `extensions` | Manifest, lifecycle, capability grants, contribution registry | Load untrusted native code in-process |
| `ui` | QML views, view models, theme tokens, command palette, animation policy | Contain business logic |
| `app` | Wire modules together, own the object graph, run the event loop | Contain feature logic |

Each module is a static library `keys_<name>` with a public header directory and a
private implementation. Public headers carry no implementation detail that would
force a rebuild of dependents.

---

## 4. Threading model

Three classes of execution:

- **UI thread.** QML scene graph, input, view models. Any operation with an
  unbounded or unpredictable cost is forbidden here.
- **Worker pool** (`core::TaskScheduler`, sized to hardware concurrency − 1).
  Search, indexing, git status, file enumeration, diff computation.
- **Dedicated threads.** One per long-lived stream: each PTY session, each LSP
  server connection, each debug adapter. These own blocking I/O and post results.

Cross-thread communication is queued signals or `postTask`. No shared mutable state
without an explicit lock documented at the declaration. Results crossing back to the
UI carry immutable value types, never pointers into worker-owned structures.

**Cancellation is mandatory** for anything the user can retrigger — search, indexing,
completion. Every such task takes a `CancellationToken`; a superseded task stops
promptly rather than racing its replacement to the UI.

**Crossing back from a worker.** `TaskScheduler::postWithResult` is the only
sanctioned way to return a result to the UI thread. It guards *delivery* — if the
receiver is destroyed while the work runs, the completion is dropped — but it does
not cancel the work, which runs to completion regardless. A task must therefore
never capture a raw pointer to something the receiver owns; `Repository` holds its
`GitClient` in a `shared_ptr` for exactly this reason.

Delivery targets the receiver's *thread*, not the receiver. Targeting the object
is a use-after-free: it can be destroyed in the window between the worker deciding
to deliver and `invokeMethod` reading it to find its thread, and a `QPointer` only
narrows that window rather than closing it.

---

## 5. Key decisions and tradeoffs

### 5.1 Native editor engine (not Monaco, not Scintilla)

**Decision:** implement the text engine in C++ over a piece table, rendered by a
custom QML item.

*Tradeoff.* Monaco would deliver multi-cursor, folding and LSP semantics immediately,
but requires QtWebEngine — which on Windows is MSVC-only, adds roughly a gigabyte of
Chromium, raises startup cost and memory floor, and makes the centre of a "native,
calm, fast" IDE a browser. Scintilla is native and proven but its own rendering model
resists the design's exact typography and spacing, and its API constrains QML
integration.

Native costs real engineering across milestones 4, 5 and 12. It buys full control of
rendering, exact fidelity to the design, a small binary, fast startup, and no
dependency that could deprecate the product. Given Keys is a long-running project
rather than a demo, control compounds and the one-time cost amortises.

*Mitigation:* the text engine is deliberately narrow — buffer, cursors, selections,
undo, decorations. Highlighting is pluggable so a grammar engine can be added without
touching the buffer. This keeps the reinvention bounded to genuinely core behaviour.

### 5.2 MSVC 2022 primary toolchain

Native Windows ABI, strongest debugger integration, and Qt's default Windows target.
MinGW remains buildable but is not the release target. Code stays free of
compiler-specific extensions so a future Linux/macOS build is a build-system change
rather than a port.

### 5.3 Git via the `git` CLI

**Decision:** drive the real `git` binary through `process`, off the UI thread.

*Tradeoff.* libgit2 avoids subprocess latency and gives structured status, but has a
large API surface, must be vendored and built, and credential/auth handling is
substantially more work to get right. The CLI is correct by construction: it honours
the user's config, hooks, credential helpers and SSH agents exactly as their terminal
does. Latency is real (a few milliseconds per invocation) and is mitigated by
debounced background refresh and caching, not by blocking.

The `vcs` module hides this behind an interface, so libgit2 could be substituted for
hot paths later without disturbing callers.

**What landed in milestone 10.** Status (porcelain v2, `-z`), log, diff, branches,
staging, unstaging, discard and commit. Everything runs on the scheduler; the UI
reads the last known status, which is always immediately available. Refresh is
debounced at 250 ms and driven by the file watcher, so a build touching two
thousand files is one status run rather than two thousand.

Only porcelain formats are parsed. Git prints one thing for people — which
changes between versions and follows the user's config — and another for scripts,
which is documented as stable. Parsing the former would break on a user's
`status.showUntrackedFiles` setting or a git upgrade.

Push, pull, fetch, branch switching and merge resolution are not implemented.
They need credential handling and conflict UI respectively, and shipping a button
that fails on a private remote would be worse than not having one.

### 5.4 Terminal via ConPTY

Windows pseudo-console (`CreatePseudoConsole`) driven from `process`, with a VT
parser and screen buffer in `terminal`. Real shells, real signals, real resizing —
the brief forbids faking this. The PTY abstraction is platform-split so a Unix
`forkpty` backend drops in later without touching the parser or the UI.

### 5.3a Syntax highlighting: lexical, per line, on demand

**Decision:** a hand-written lexer in `editor`, run one line at a time as the
view draws it.

*Tradeoff.* A grammar-driven highlighter (tree-sitter) is more accurate and
handles nesting properly, but it is a dependency per language plus a parse tree
per file. A lexer knows that `class` is a keyword and `"..."` is a string, and
that is most of the visual benefit for none of the cost.

Per line, because the editor renders only its viewport: highlighting a whole file
would be work nobody sees, and on a 200,000-line file it would dominate the cost
of opening it. Lines carry a small state (in-block-comment or not) so a comment
or string spanning lines is still correct - the cached states are truncated from
the edited line down, since an opened block comment recolours everything after
it.

Lexical only. Whether an identifier names a type or a variable needs a compiler,
which is what the language server is for; semantic tokens can layer on top later.
A language with no rules is left plain rather than run through rules that nearly
fit - mis-colouring reads as a bug, no colour reads as an unsupported file type.

### 5.4a Build tasks over pipes, not a PTY

**Decision:** run build and run tasks through `QProcess` pipes, streaming output
line by line, with no pseudo-terminal.

*Tradeoff.* A PTY would let a build tool draw progress bars and colour exactly as
it does in a terminal. But a tool being scripted should do the opposite: emit
plain text a parser can read, and fail rather than stop to ask a question nobody
will answer. `TERM=dumb`, `NO_COLOR` and a closed stdin say so in the ways the
common toolchains listen to. Pipes also work today, where the terminal's ConPTY
attachment does not — so build and run do not wait on that.

Output is emitted as complete lines rather than raw chunks: a read can split a
line anywhere, and every consumer would otherwise reassemble them slightly
differently. Diagnostics are parsed as they stream, so the problem list fills
while the build is still running.

**Tasks are command lines, not language integrations.** Keys does not know how to
build C++; it knows how to run `cmake --build build` and show what comes back.
Defaults are offered per project kind and nothing is inferred from build files —
guessing wrong produces a task that fails confusingly, which is worse than
offering none. Python gets no build or run default for exactly this reason.

**One task at a time.** Two builds writing to one output directory corrupt each
other, and two consoles interleaving is unreadable. A second start is refused
rather than queued, which makes stopping the first an explicit decision.

### 5.5 Language services via LSP

One `langsvc` client speaking standard LSP over stdio. Language support becomes
configuration — a server binary plus a file-type mapping — not new C++ code. This is
what makes "add a language later" cheap, and it is why the editor exposes positions
and ranges in LSP's coordinate model at its boundary.

**What landed in milestone 12.** Framing, lifecycle, capability negotiation,
document sync (incremental where the server supports it), completion, go-to
definition, hover and diagnostics. Servers start on demand — launching every
configured one at project open would cost seconds for languages the user never
touches — and one that is not installed simply never starts.

Framing is the part that repays care: a read can end anywhere, including halfway
through a header, so decoding is stateful and `Content-Length` is authoritative
rather than any delimiter. Source text legitimately contains `



`.

Positions cross the boundary unchanged. The editor already uses zero-based lines
and UTF-16 columns (see TextBuffer), which is LSP's default encoding, so the
conversion is a field copy rather than a re-encoding.

Rename, references, formatting, code actions and semantic tokens are not
implemented. Semantic tokens are the natural driver for syntax highlighting and
are the next thing to add here.

### 5.6 Debugging via DAP

Same reasoning as LSP: the Debug Adapter Protocol makes debugger support a matter of
adapter configuration. The debug UI is built strictly against DAP semantics, so no
control appears that has nothing behind it.

**What landed in milestone 13.** Adapter lifecycle, breakpoints, stepping, call
stack and variables. DAP shares LSP's Content-Length framing but not its
envelope - it uses `seq`/`type`/`command` where JSON-RPC uses `id`/`method` - so
the codec is reused and each protocol reads its own fields from the decoded
object. That is the part worth sharing; the envelopes are not.

Every control is enabled from the session state, so none can send a request the
adapter would reject. Breakpoints live in the UI model rather than the session:
a user sets them before starting and expects them afterwards.

Adapters are separate installs and none is bundled. A project kind with no
conventional adapter gets no debug controls at all rather than ones that cannot
work.

Conditional breakpoints, watch expressions, multi-thread views and expanding
nested variables are not implemented.

### 5.7 Extensions: out-of-process from day one

**Decision:** the extension host is a separate process; extensions never load as
in-process native code.

*Tradeoff.* IPC costs latency versus direct calls. But an in-process model means any
extension can crash or hang the IDE and read all of its memory — and once shipped,
that is unfixable without breaking every extension. Defining the boundary as a
process boundary now, even with a single trivial extension, is what keeps the
architecture honest. V1 defines manifest, lifecycle, capabilities and contribution
points; the marketplace is explicitly out of scope.

---

## 6. Settings

Three layers, resolved in order: **defaults → user → workspace**. Each setting is
declared once in a schema (key, type, default, scope) and read through a typed
accessor. Changes emit targeted notifications; nothing polls.

`animation.level` (`Full` | `Reduced` | `Off`) is a first-class global that maps to
the design's `--anim-t` token: 150 ms / 60 ms / 0 ms. Every animation in the app
reads its duration from this single source. `Off` disables animation entirely rather
than shortening it, and the app also honours the OS reduced-motion preference as the
initial default.

**A setting exists only once something honours it.** A declared key with no consumer
becomes a control in the settings page that changes nothing, and a user cannot tell
that from a broken one. Minimap, format-on-save and the terminal's settings were
removed for this reason and return with the features that read them.

**The settings page is generated from the schema, not hand-written.** The
declaration already carries the type, default, allowed values and bounds; a
hand-built page would duplicate all of it and then drift. The control follows the
type — bool to a toggle, allowed values to a segmented choice, a bounded number to a
slider, anything else to a text field — so adding a setting adds a working row.
Bounds are enforced on write rather than only in the UI, because settings arrive
from a hand-edited JSON file as readily as from a slider.

`accessibility.uiScale` is the one setting that cannot apply live: Qt reads
`QT_SCALE_FACTOR` once, when the GUI application is constructed. It is therefore
read from disk before `QGuiApplication` and takes effect on the next launch, which
the settings page states rather than hides.

---

## 6a. C++ objects exposed to QML

Objects the application owns and QML consumes (theme, controller, settings, models)
are registered as `QML_NAMED_ELEMENT` + `QML_SINGLETON` with a static `create()`
returning the instance `main()` published.

**Such a type must not be default-constructible.** A `QML_SINGLETON` the engine
*can* construct will construct its own instance rather than calling `create()`, and
QML then binds to a second object that `main()` never wired to anything. The failure
is silent: no warning, no error, just a signal that never arrives. This is not
hypothetical — `Theme` shipped this way from milestone 1, which is why the theme
toggled from the rail (mutating QML's private copy) but never persisted and never
responded to settings. Taking a dependency by reference in the constructor is what
makes the registration honest, and `ThemeTests` asserts it with a `static_assert`.

QML compiled ahead of time also cannot see context properties: the AOT compiler
resolves such bindings to `undefined` at compile time, with only a runtime warning.
Anything QML binds to is therefore a declared singleton, not a context property.

---

## 7. Commands

A single registry owns every user-invokable action. A command carries an id, a title,
a category, an enablement predicate and a handler. Modules register their own
commands at startup — the palette, menus and keybindings are all views over this one
registry, so nothing is wired twice and extensions contribute through the same door.

Keybindings map chords to command ids through a separate table, so the same command
is reachable by palette, shortcut, menu or extension without duplication.

---

## 8. Performance budgets

These are targets to measure against, not aspirations. Milestone 15 exists to hold
them; regressions are treated as defects.

| Operation | Budget |
|---|---|
| Cold start to interactive window | < 400 ms |
| Open a 10k-file project (tree usable) | < 500 ms |
| Switch between open tabs | < 16 ms |
| Keystroke to glyph on screen | < 16 ms (one frame) |
| Quick-open first results, 50k files | < 100 ms |
| Project-wide text search, first results | < 300 ms |
| Idle CPU with a project open | ~0% |

Measurement is built in rather than bolted on: `core` carries a lightweight scoped
tracer, and startup and search paths are instrumented from the beginning so
optimisation targets measured bottlenecks rather than guesses.

**Measured at milestone 15** (release build, this machine):

| Operation | Budget | Measured |
|---|---|---|
| Cold start to interactive window | < 400 ms | 89 ms |
| Open a 10k-file project | < 500 ms | 9 ms |
| Switch between open tabs | < 16 ms | < 0.001 ms |
| Keystroke in a 200k-line file | < 16 ms | 0.06 ms |
| Quick open, 50k files | < 100 ms | 89 ms |
| Idle CPU with a project open | ~0% | 0 ms over 10 s |

`tests/BudgetTests.cpp` holds these, so a regression is a failing test rather than
something noticed later. They assert only in a release build - a debug build is
several times slower for reasons unrelated to the algorithm - and report their
numbers either way so a run shows where the headroom is.

One budget was missed and fixed. A keystroke in a 200,000-line file took 22 ms,
past the frame budget, because every edit rebuilt the line index by walking the
whole document. The index is now updated in place: an edit touches only the line
starts it actually moves. That is 0.06 ms, a 370x improvement, and the case that
found it is the one the original code comment said milestone 15 should look at.

---

## 9. Testing strategy

Qt Test, one binary per module, run through CTest.

- **Unit tests** for every layer 0–2 module. Pure logic — buffer edits, undo, path
  handling, settings resolution, VT parsing, diff parsing, command registry.
- **Integration tests** for critical workflows: open project → open file → edit →
  save; stage → commit → verify status; spawn terminal → run command → read output.
- **Filesystem and git tests** run against temporary directories and real
  repositories created in setup, never against the developer's machine state.
- **UI logic** is tested through view models, which hold no QML dependency. QML files
  stay declarative enough that this covers the behaviour worth testing.

Warnings are errors in CI. A milestone is not complete while its module's tests fail.

---

## 10. Milestones

Each leaves the project buildable and tested.

1. **Application shell** — build system, core, config, theme tokens, main window,
   activity rail, sidebar, status bar, command registry, animation policy.
2. Project & workspace management
3. File explorer
4. Editor
5. Tabs & editor splits
6. Terminal
7. Search
8. Command palette
9. Settings & themes
10. Git
11. Build/run
12. Language services
13. Debugger
14. Extensions
15. Performance & polish

---

## 11. Technical risks

| Risk | Impact | Mitigation |
|---|---|---|
| Native editor scope creep | High | Hard boundary on the text engine; highlighting and language features stay pluggable and land in later milestones |
| Editor rendering performance on large files | High | Viewport-only rendering with a line index; measured against budget from milestone 4, not at the end |
| ConPTY behavioural quirks | Medium | Platform-split PTY interface; parser tested against recorded VT streams independent of the OS |
| Git CLI latency on large repos | Medium | Debounced background refresh, cached status, interface allows libgit2 later |
| LSP server variability | Medium | Strict capability negotiation; unsupported features hide rather than fail |
| Qt QML startup cost | Medium | Compile QML to C++ resources, measure cold start every milestone |
| Extension host IPC complexity | Medium | Ship the boundary early with one trivial extension so it is exercised before it matters |
