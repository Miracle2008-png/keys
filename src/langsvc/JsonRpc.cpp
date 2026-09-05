#include "langsvc/JsonRpc.h"

#include "core/Log.h"

#include <QJsonDocument>

namespace keys::langsvc {
namespace {

constexpr auto kHeaderTerminator = "\r\n\r\n";
constexpr auto kContentLength = "Content-Length:";

/// A body larger than this is a malformed or hostile stream rather than a real
/// message; a completion response on a huge file is a few megabytes at most.
constexpr int kMaxBodyBytes = 64 * 1024 * 1024;

} // namespace

QByteArray JsonRpcCodec::encode(const QJsonObject& message)
{
    const QByteArray body = QJsonDocument(message).toJson(QJsonDocument::Compact);

    // Content-Length counts bytes, not characters. Using the encoded array's
    // size rather than a string length is what makes that correct for a body
    // containing non-ASCII - which any message carrying source text will.
    return QByteArray("Content-Length: ") + QByteArray::number(body.size())
           + QByteArray(kHeaderTerminator) + body;
}

void JsonRpcCodec::append(const QByteArray& data)
{
    m_buffer.append(data);
}

std::optional<RpcMessage> JsonRpcCodec::next()
{
    while (true) {
        const int headerEnd = m_buffer.indexOf(kHeaderTerminator);
        if (headerEnd < 0) {
            return std::nullopt;   // header not complete yet
        }

        const QByteArray header = m_buffer.left(headerEnd);
        int contentLength = -1;

        for (const QByteArray& line : header.split('\n')) {
            const QByteArray trimmed = line.trimmed();
            if (trimmed.startsWith(kContentLength)) {
                contentLength =
                    trimmed.mid(static_cast<int>(qstrlen(kContentLength))).trimmed().toInt();
            }
        }

        const int bodyStart = headerEnd + static_cast<int>(qstrlen(kHeaderTerminator));

        if (contentLength < 0 || contentLength > kMaxBodyBytes) {
            // Unparseable framing. Dropping the header and resynchronising is
            // the only option: without a length there is no way to know where
            // this message ends, so the stream is treated as lost from here.
            qCWarning(lcCore) << "language server sent an unusable header; resyncing";
            m_buffer.remove(0, bodyStart);
            continue;
        }

        if (m_buffer.size() < bodyStart + contentLength) {
            return std::nullopt;   // body still arriving
        }

        const QByteArray body = m_buffer.mid(bodyStart, contentLength);
        m_buffer.remove(0, bodyStart + contentLength);

        // A malformed body loses one message, not the stream: framing was valid,
        // so the next message is still correctly positioned.
        if (std::optional<RpcMessage> message = parseBody(body)) {
            return message;
        }
        qCWarning(lcCore) << "language server sent a message that is not valid JSON-RPC";
    }
}

std::optional<RpcMessage> JsonRpcCodec::parseBody(const QByteArray& body)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }

    const QJsonObject object = document.object();

    RpcMessage message;
    message.raw = object;
    message.id = object.value(QStringLiteral("id"));
    message.method = object.value(QStringLiteral("method")).toString();
    message.params = object.value(QStringLiteral("params")).toObject();

    // contains() rather than a null check: a successful response to a request
    // with no return value carries `"result": null`, which is a response, not a
    // request.
    if (object.contains(QStringLiteral("result"))) {
        message.result = object.value(QStringLiteral("result"));
        message.hasResult = true;
    }
    if (object.contains(QStringLiteral("error"))) {
        message.error = object.value(QStringLiteral("error")).toObject();
        message.hasError = true;
    }
    return message;
}

} // namespace keys::langsvc
