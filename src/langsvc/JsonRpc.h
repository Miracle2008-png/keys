#pragma once

#include "core/Result.h"

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>

#include <optional>

namespace keys::langsvc {

/// One decoded message from a language server.
struct RpcMessage {
    /// Present on requests and responses, absent on notifications. LSP allows a
    /// string or a number; Keys only ever sends numbers, and a server echoes
    /// back what it was given.
    QJsonValue id;

    /// Present on requests and notifications.
    QString method;

    QJsonObject params;

    /// Present on a successful response. `result` can legitimately be null, so
    /// its presence is what distinguishes a response from a request, not its
    /// value.
    QJsonValue result;
    bool hasResult = false;

    QJsonObject error;
    bool hasError = false;

    /// The whole decoded object.
    ///
    /// DAP shares LSP's framing but not its envelope - it uses `seq`, `type` and
    /// `command` where JSON-RPC uses `id` and `method`. Keeping the raw object
    /// lets the debugger read its own fields from the same codec rather than
    /// duplicating the framing, which is the part worth sharing.
    QJsonObject raw;

    [[nodiscard]] bool isResponse() const { return hasResult || hasError; }

    /// A notification is a method call with no id. An *absent* id decodes to
    /// Undefined, not Null - checking isNull() alone reports every notification
    /// as a request, and the client then tries to reply to messages that expect
    /// no reply.
    [[nodiscard]] bool hasId() const { return !id.isUndefined() && !id.isNull(); }

    [[nodiscard]] bool isNotification() const { return !method.isEmpty() && !hasId(); }
    [[nodiscard]] bool isRequest() const { return !method.isEmpty() && hasId(); }
};

/// Frames and parses LSP's wire format.
///
/// **Why a class and not a function.** The transport is a byte stream: a read
/// can end anywhere, including halfway through a header or a body. So decoding
/// is stateful — bytes are appended and whole messages are taken out as they
/// complete. Every framing bug in an LSP client comes from assuming a read
/// boundary is a message boundary.
///
/// **Content-Length is authoritative.** The header names the body's length in
/// bytes, and the body is JSON that can contain anything including newlines. So
/// the parser counts bytes rather than looking for a delimiter.
class JsonRpcCodec {
public:
    /// Wraps `message` in the header LSP requires.
    [[nodiscard]] static QByteArray encode(const QJsonObject& message);

    /// Appends received bytes to the buffer.
    void append(const QByteArray& data);

    /// Takes the next complete message, or nothing when one has not fully
    /// arrived yet. Call repeatedly until it returns nothing: a single read can
    /// deliver several messages.
    [[nodiscard]] std::optional<RpcMessage> next();

    /// Discards buffered input, for a server that is being restarted.
    void reset() { m_buffer.clear(); }

    [[nodiscard]] int bufferedBytes() const { return static_cast<int>(m_buffer.size()); }

private:
    /// Parses a decoded body into a message. Separated so a malformed body can
    /// be dropped without losing framing on the rest of the stream.
    [[nodiscard]] static std::optional<RpcMessage> parseBody(const QByteArray& body);

    QByteArray m_buffer;
};

} // namespace keys::langsvc
