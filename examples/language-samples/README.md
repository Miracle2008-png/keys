# Language samples

One file per language Keys highlights - 26 of them - holding idiomatic code
rather than a snippet: decorators and f-strings in Python, lifetimes and traits
in Rust, goroutines and channels in Go, LINQ and pattern matching in C#,
protocols and async/await in Swift, metatables and coroutines in Lua, CTEs and
window functions in SQL.

Those 26 sets of rules cover about 80 file extensions, because several
languages genuinely share a lexical shape: C and C++, JavaScript and JSON, HTML
and XML, TOML and INI. An extension is mapped only where the shape actually
matches - not to inflate the count.

`tools/language-check.cpp` runs the real highlighter over all of them and
reports, per file, how many lines produced tokens and which kinds appeared. It
fails a file whose extension is not recognised, one where most lines produce
nothing, and one that only ever emits a single kind of token.

That last pair of checks is the point. Unit tests assert that a given snippet
produces a given token, which cannot catch the failures that matter here: a
string state that never closes and swallows the rest of the file, a comment
state that leaks across lines, or rules registered for a language that nothing
ever reaches. Markdown passed every unit test it had while marking only
headings and bullets - 47% of its lines came out plain, and this is what
showed it.

    cmake --build build --target keys_language_check
    build/bin/keys_language_check examples/language-samples
