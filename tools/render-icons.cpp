// Renders the Keys mark from its SVG source into every raster size the
// platforms need, plus a Windows .ico.
//
// Built as part of the project rather than run through an external tool so the
// icons can be regenerated from source on any machine that can build Keys, with
// no extra dependency to install. Qt's SVG renderer is also the one the running
// application uses, so what ships matches what the app draws.
//
//     render-icons <source.svg> <small-source.svg> <output-directory>
//
// Two sources because the mark is optically sized: below kSmallMarkThreshold the
// keycap outline and caret cannot be resolved and merge into noise, so those
// sizes render from a simplified variant instead. See resources/icons/*.svg.

#include <QBuffer>
#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QTextStream>

#include <algorithm>
#include <array>

namespace {

/// Sizes every desktop platform asks for, plus the ones Windows packs into an
/// .ico. 16 through 512 covers taskbar, Explorer, Alt-Tab, the Start menu, and
/// macOS/Linux hidpi variants.
constexpr std::array kSizes = {16, 20, 24, 32, 40, 48, 64, 96, 128, 256, 512};

/// The sizes Windows actually reads out of an .ico. Including 16 and 32 matters
/// most: those are the ones a user sees all day, and a bad downscale there is
/// what makes an application look cheap.
constexpr std::array kIcoSizes = {16, 20, 24, 32, 48, 64, 128, 256};

/// Sizes below this render from the simplified mark.
///
/// Chosen by rendering both variants and comparing: at 32px the keycap stroke
/// and the gap inside it each land on their own pixels and the full mark is
/// crisp. At 24px and below they start sharing pixels and the outline closes up
/// around the caret, so those sizes get the simplified variant instead.
constexpr int kSmallMarkThreshold = 32;

/// Rasterises the SVG at one size.
///
/// Rendering each size from the vector rather than downscaling one large bitmap
/// is the whole point: a 16px icon resampled from 512px turns the mark's
/// rounded corners to mush, while a direct render keeps the geometry crisp.
QImage renderAt(QSvgRenderer& renderer, int size)
{
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer.render(&painter);
    painter.end();

    return image;
}

/// Writes a multi-frame Windows .ico.
///
/// The format is a 6-byte header, then one 16-byte directory entry per frame,
/// then the frame payloads. Each payload here is a complete PNG file: the ICO
/// format has allowed PNG frames since Vista, and it avoids hand-rolling the
/// legacy DIB layout with its bottom-up rows and separate AND mask.
///
/// A frame's stored dimension byte is 0 for 256 pixels, which is how the format
/// encodes that size in a single byte.
bool writeIco(const QString& path, const QList<QImage>& frames)
{
    if (frames.isEmpty()) {
        return false;
    }

    // Encode every frame first: the directory needs each payload's exact size
    // and offset before any of it can be written.
    QList<QByteArray> payloads;
    payloads.reserve(frames.size());
    for (const QImage& frame : frames) {
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        if (!frame.save(&buffer, "PNG")) {
            return false;
        }
        payloads.append(png);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);

    constexpr quint16 kIconType = 1;
    stream << quint16(0) << kIconType << quint16(frames.size());

    constexpr int kHeaderSize = 6;
    constexpr int kDirectoryEntrySize = 16;
    quint32 offset = kHeaderSize + kDirectoryEntrySize * quint32(frames.size());

    for (int i = 0; i < frames.size(); ++i) {
        const QImage& frame = frames.at(i);
        const int side = frame.width();

        stream << quint8(side >= 256 ? 0 : side)   // width, 0 meaning 256
               << quint8(side >= 256 ? 0 : side)   // height
               << quint8(0)                        // palette size; 0 for truecolour
               << quint8(0)                        // reserved
               << quint16(1)                       // colour planes
               << quint16(32)                      // bits per pixel
               << quint32(payloads.at(i).size())
               << offset;

        offset += quint32(payloads.at(i).size());
    }

    for (const QByteArray& payload : payloads) {
        if (file.write(payload) != payload.size()) {
            return false;
        }
    }

    return file.error() == QFileDevice::NoError;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream err(stderr);
    QTextStream out(stdout);

    const QStringList arguments = QCoreApplication::arguments();
    if (arguments.size() != 4) {
        err << "usage: render-icons <source.svg> <small-source.svg> "
               "<output-directory>\n";
        return 2;
    }

    const QString source = arguments.at(1);
    const QString smallSource = arguments.at(2);
    const QString outputDirectory = arguments.at(3);

    QSvgRenderer renderer(source);
    if (!renderer.isValid()) {
        err << "error: could not parse " << source << "\n";
        return 1;
    }

    QSvgRenderer smallRenderer(smallSource);
    if (!smallRenderer.isValid()) {
        err << "error: could not parse " << smallSource << "\n";
        return 1;
    }

    QDir directory(outputDirectory);
    if (!directory.exists() && !QDir().mkpath(outputDirectory)) {
        err << "error: could not create " << outputDirectory << "\n";
        return 1;
    }

    QList<QImage> icoFrames;

    for (const int size : kSizes) {
        const bool small = size < kSmallMarkThreshold;
        const QImage image = renderAt(small ? smallRenderer : renderer, size);

        const QString path =
            directory.filePath(QStringLiteral("keys-%1.png").arg(size, 3, 10, QLatin1Char('0')));
        if (!image.save(path, "PNG")) {
            err << "error: could not write " << path << "\n";
            return 1;
        }
        out << "wrote " << QFileInfo(path).fileName()
            << (small ? "  (simplified mark)" : "") << "\n";

        if (kIcoSizes.cend() != std::find(kIcoSizes.cbegin(), kIcoSizes.cend(), size)) {
            icoFrames.append(image);
        }
    }

    // The .ico container is written directly. Qt's ICO plugin writes a single
    // image, but Windows picks the frame closest to the size it needs - so a
    // one-frame icon means the 16px taskbar entry is a downscale of a large
    // bitmap, which is exactly the mush this tool exists to avoid.
    //
    // Each frame is stored as a complete PNG, which the format has permitted
    // since Vista and which every supported Windows version reads.
    const QString icoPath = directory.filePath(QStringLiteral("keys.ico"));
    if (!writeIco(icoPath, icoFrames)) {
        err << "error: could not write " << icoPath << "\n";
        return 1;
    }
    out << "wrote keys.ico (" << icoFrames.size() << " sizes)\n";

    return 0;
}
