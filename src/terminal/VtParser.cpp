#include "terminal/VtParser.h"

#include <algorithm>

namespace keys::terminal {
namespace {

constexpr char kEscape = 0x1B;
constexpr char kBell = 0x07;
constexpr char kBackspace = 0x08;
constexpr char kTab = 0x09;
constexpr char kLineFeed = 0x0A;
constexpr char kCarriageReturn = 0x0D;

} // namespace

VtParser::VtParser(TerminalScreen& screen)
    : m_screen(screen), m_decoder(QStringDecoder::Utf8)
{
}

void VtParser::reset()
{
    m_state = State::Ground;
    m_pending.clear();
    m_parameters.clear();
    m_parameterStarted = false;
    m_privateMarker = 0;
    m_oscString.clear();
    m_style = CellStyle{};
    m_decoder = QStringDecoder(QStringDecoder::Utf8);
}

int VtParser::parameter(size_t index, int defaultTo) const
{
    if (index >= m_parameters.size()) {
        return defaultTo;
    }
    // An explicitly empty parameter also means the default: `ESC [ ;5 H` sets
    // the row to its default and the column to 5.
    return m_parameters.at(index) < 0 ? defaultTo : m_parameters.at(index);
}

void VtParser::flushText()
{
    if (m_pending.isEmpty()) {
        return;
    }
    m_screen.writeText(m_pending, m_style);
    m_pending.clear();
}

void VtParser::parse(const QByteArray& data)
{
    // Decode incrementally: a multi-byte character split across two reads is
    // held by the decoder rather than turned into a replacement character.
    const QString text = m_decoder.decode(data);

    for (const QChar character : text) {
        const char16_t unit = character.unicode();

        switch (m_state) {
        case State::Ground:
            switch (unit) {
            case kEscape:
                flushText();
                m_state = State::Escape;
                break;
            case kLineFeed:
                flushText();
                m_screen.lineFeed();
                break;
            case kCarriageReturn:
                flushText();
                m_screen.carriageReturn();
                break;
            case kBackspace:
                flushText();
                m_screen.backspace();
                break;
            case kTab:
                flushText();
                m_screen.tab();
                break;
            case kBell:
                // Deliberately silent. An audible bell from a build script is
                // an interruption nobody wants from an editor.
                break;
            default:
                if (unit >= 0x20) {
                    m_pending += character;
                }
                // Other control characters are dropped rather than printed as
                // boxes.
                break;
            }
            break;

        case State::Escape:
            m_parameters.clear();
            m_parameterStarted = false;
            m_privateMarker = 0;

            switch (unit) {
            case u'[':
                m_state = State::CsiEntry;
                break;
            case u']':
                m_oscString.clear();
                m_state = State::OscString;
                break;
            case u'c':
                // RIS: full reset.
                m_screen.reset();
                m_style = CellStyle{};
                m_state = State::Ground;
                break;
            default:
                // Two-character sequences that are not implemented are simply
                // consumed, so their final byte is not printed as text.
                m_state = State::Ground;
                break;
            }
            break;

        case State::CsiEntry:
            if (unit >= u'0' && unit <= u'9') {
                if (!m_parameterStarted) {
                    m_parameters.push_back(0);
                    m_parameterStarted = true;
                }
                m_parameters.back() = m_parameters.back() * 10 + (unit - u'0');
            } else if (unit == u';') {
                // A separator with no digits before it means an omitted
                // parameter, recorded as -1 so it can take its default.
                if (!m_parameterStarted) {
                    m_parameters.push_back(-1);
                }
                m_parameterStarted = false;
            } else if (unit == u'?' || unit == u'>' || unit == u'<' || unit == u'=') {
                m_privateMarker = static_cast<char>(unit);
            } else if (unit >= 0x40 && unit <= 0x7E) {
                // A byte in this range ends the sequence. With no parameters
                // collected, every handler falls back to its own default.
                dispatchCsi(static_cast<char>(unit));
                m_state = State::Ground;
            }
            // Intermediate bytes (0x20-0x2F) are collected by neither branch and
            // simply ignored; no sequence Keys implements uses them.
            break;

        case State::OscString:
            if (unit == kBell) {
                dispatchOsc();
                m_state = State::Ground;
            } else if (unit == kEscape) {
                // Possibly the start of ST (ESC \).
                m_state = State::OscEscape;
            } else {
                m_oscString += character;
            }
            break;

        case State::OscEscape:
            // ST terminates the string; anything else was not a terminator, so
            // the escape is discarded and collection resumes.
            dispatchOsc();
            m_state = State::Ground;
            break;
        }
    }

    flushText();
}

void VtParser::dispatchCsi(char finalByte)
{
    switch (finalByte) {
    case 'A':   // CUU - cursor up
        m_screen.moveCursor(-parameter(0, 1), 0);
        break;
    case 'B':   // CUD - cursor down
        m_screen.moveCursor(parameter(0, 1), 0);
        break;
    case 'C':   // CUF - cursor forward
        m_screen.moveCursor(0, parameter(0, 1));
        break;
    case 'D':   // CUB - cursor back
        m_screen.moveCursor(0, -parameter(0, 1));
        break;

    case 'H':   // CUP - cursor position
    case 'f':
        // Terminal coordinates are 1-based; the screen's are 0-based.
        m_screen.setCursor(parameter(0, 1) - 1, parameter(1, 1) - 1);
        break;

    case 'G':   // CHA - cursor to column, row unchanged
        m_screen.setCursor(m_screen.cursorRow(), parameter(0, 1) - 1);
        break;

    case 'd':   // VPA - cursor to row, column unchanged
        m_screen.setCursor(parameter(0, 1) - 1, m_screen.cursorColumn());
        break;

    case 'J':   // ED - erase in display
        m_screen.eraseInDisplay(parameter(0, 0));
        break;
    case 'K':   // EL - erase in line
        m_screen.eraseInLine(parameter(0, 0));
        break;

    case 'm':   // SGR - colours and attributes
        applyGraphicRendition();
        break;

    case 'h':   // SM - set mode
        if (m_privateMarker == '?' && parameter(0, 0) == 25) {
            m_screen.setCursorVisible(true);
        }
        break;
    case 'l':   // RM - reset mode
        if (m_privateMarker == '?' && parameter(0, 0) == 25) {
            m_screen.setCursorVisible(false);
        }
        break;

    default:
        // Consumed and ignored. Discarding an unimplemented sequence leaves a
        // blank area; printing its bytes would corrupt the screen.
        break;
    }
}

void VtParser::applyGraphicRendition()
{
    // No parameters means reset, which is how `ESC [ m` is defined.
    if (m_parameters.empty()) {
        m_style = CellStyle{};
        return;
    }

    for (size_t i = 0; i < m_parameters.size(); ++i) {
        const int code = m_parameters.at(i) < 0 ? 0 : m_parameters.at(i);

        switch (code) {
        case 0:
            m_style = CellStyle{};
            break;
        case 1:
            m_style.bold = true;
            break;
        case 3:
            m_style.italic = true;
            break;
        case 4:
            m_style.underline = true;
            break;
        case 7:
            m_style.inverse = true;
            break;
        case 22:
            m_style.bold = false;
            break;
        case 23:
            m_style.italic = false;
            break;
        case 24:
            m_style.underline = false;
            break;
        case 27:
            m_style.inverse = false;
            break;

        case 39:
            m_style.foreground = -1;
            break;
        case 49:
            m_style.background = -1;
            break;

        case 38:
        case 48: {
            // Extended colour: `38;5;n` for the 256-colour palette, `38;2;r;g;b`
            // for direct colour. Both consume their parameters so the following
            // codes are not misread as attributes.
            const bool isForeground = code == 38;
            const int kind = parameter(i + 1, 0);

            if (kind == 5) {
                const int index = parameter(i + 2, 0);
                (isForeground ? m_style.foreground : m_style.background) = index;
                i += 2;
            } else if (kind == 2) {
                // Direct colour is mapped into the palette's 16-231 cube rather
                // than stored exactly: the screen model holds indices so the
                // theme can resolve them, and carrying two colour
                // representations through it would complicate every consumer for
                // output that is rare in a shell.
                const int r = std::clamp(parameter(i + 2, 0), 0, 255) * 5 / 255;
                const int g = std::clamp(parameter(i + 3, 0), 0, 255) * 5 / 255;
                const int b = std::clamp(parameter(i + 4, 0), 0, 255) * 5 / 255;
                (isForeground ? m_style.foreground : m_style.background) =
                    16 + 36 * r + 6 * g + b;
                i += 4;
            }
            break;
        }

        default:
            if (code >= 30 && code <= 37) {
                m_style.foreground = code - 30;
            } else if (code >= 40 && code <= 47) {
                m_style.background = code - 40;
            } else if (code >= 90 && code <= 97) {
                // Bright foreground: palette entries 8-15.
                m_style.foreground = code - 90 + 8;
            } else if (code >= 100 && code <= 107) {
                m_style.background = code - 100 + 8;
            }
            break;
        }
    }
}

void VtParser::dispatchOsc()
{
    // OSC strings are `number;text`. Only the title sequences are acted on.
    const int separator = m_oscString.indexOf(QLatin1Char(';'));
    if (separator < 0) {
        m_oscString.clear();
        return;
    }

    const int command = m_oscString.left(separator).toInt();
    if (command == 0 || command == 2) {
        m_title = m_oscString.mid(separator + 1);
    }

    m_oscString.clear();
}

} // namespace keys::terminal
