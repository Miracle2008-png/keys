"""A minimal Keys extension.

Speaks JSON-RPC over stdio with Content-Length framing - the same wire format
the language client uses, so an extension needs no Keys-specific library.
"""
import json
import sys


def read_message():
    """Reads one framed message, or None at end of stream."""
    length = None
    while True:
        line = sys.stdin.buffer.readline()
        if not line:
            return None
        line = line.strip()
        if not line:
            break                      # end of headers
        if line.lower().startswith(b"content-length:"):
            length = int(line.split(b":", 1)[1])
    if length is None:
        return None
    return json.loads(sys.stdin.buffer.read(length))


def send(message):
    body = json.dumps(message).encode("utf-8")
    sys.stdout.buffer.write(b"Content-Length: %d\r\n\r\n" % len(body))
    sys.stdout.buffer.write(body)
    sys.stdout.buffer.flush()


def main():
    next_id = 1
    while True:
        message = read_message()
        if message is None:
            return

        method = message.get("method", "")

        if method == "keys/shutdown":
            return

        if method == "keys/executeCommand":
            # showMessage needs the showUi capability, which this extension
            # declared. Asking for something it did not declare would come back
            # refused rather than silently ignored.
            send({
                "jsonrpc": "2.0",
                "id": next_id,
                "method": "window/showMessage",
                "params": {"message": "Hello from an extension, running in its own process."},
            })
            next_id += 1


if __name__ == "__main__":
    main()
