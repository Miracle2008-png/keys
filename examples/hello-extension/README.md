# Hello — a sample Keys extension

A minimal extension that proves the host boundary works end to end.

It runs in its own process, speaks JSON-RPC over stdio, contributes a command to
the palette, and asks Keys to show a message — which succeeds only because it
declared the `showUi` capability and the user granted it.

## Installing

Copy this directory to:

- Windows: `%LOCALAPPDATA%\Keys\extensions\hello`
- macOS: `~/Library/Application Support/Keys/extensions/hello`
- Linux: `~/.local/share/Keys/extensions/hello`

Then open the Extensions view, read what it says the extension can do, and
choose **Allow and run**. Nothing starts before that.

## The protocol

Content-Length framing with a JSON-RPC body — the same wire format the language
client uses, so an extension needs no Keys-specific library. `hello.py` is 60
lines and depends on nothing.

Keys sends:

- `keys/initialize` — the workspace root and the capabilities actually granted,
  so an extension can decide what to offer rather than discovering its limits by
  being refused.
- `keys/executeCommand` — a contributed command was invoked.
- `keys/shutdown` — exit cleanly.

The extension may request:

- `window/showMessage` — needs `showUi`.

Anything outside the grant comes back as an error, not silence.
