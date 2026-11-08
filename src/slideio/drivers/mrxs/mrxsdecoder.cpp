// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/mrxs/mrxsdecoder.hpp"
#include "slideio/drivers/mrxs/mrxserrors.hpp"
#include "slideio/imagetools/imagetools.hpp"

#include <opencv2/imgproc.hpp>
#include <cstring>

using namespace slideio::mrxs;

namespace
{
    const char* nameOf(ImageFormat format)
    {
        switch (format) {
        case ImageFormat::Jpeg: return "JPEG";
        case ImageFormat::Png: return "PNG";
        case ImageFormat::Bmp24: return "BMP24";
        }
        return "?";
    }

    // What the stream's own signature says it is.
    const char* signatureOf(const std::vector<uint8_t>& b)
    {
        static const uint8_t png[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
        if (b.size() >= 3 && b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF) return "JPEG";
        if (b.size() >= 8 && std::memcmp(b.data(), png, 8) == 0) return "PNG";
        if (b.size() >= 2 && b[0] == 'B' && b[1] == 'M') return "BMP";
        return nullptr;
    }

    uint32_t u32(const std::vector<uint8_t>& b, size_t at)
    {
        return uint32_t(b[at]) | (uint32_t(b[at + 1]) << 8) | (uint32_t(b[at + 2]) << 16) | (uint32_t(b[at + 3]) << 24);
    }

    uint16_t u16(const std::vector<uint8_t>& b, size_t at) { return uint16_t(b[at] | (b[at + 1] << 8)); }

    // The documented BMP24 profile (format description 12.1), checked on the header bytes
    // themselves because FreeImage would decode far more than this profile allows.
    void checkBmp24(const std::vector<uint8_t>& b, int width, int height, const std::string& what)
    {
        if (b.size() < 54) {
            MRXS_CORRUPT << what << ": a BMP needs a 54-byte header, the payload has " << b.size() << " bytes";
        }
        const uint32_t pixelOffset = u32(b, 10);
        const uint32_t dibSize = u32(b, 14);
        int32_t w, h;
        uint32_t uw = u32(b, 18), uh = u32(b, 22);
        std::memcpy(&w, &uw, 4);
        std::memcpy(&h, &uh, 4);
        const uint16_t planes = u16(b, 26);
        const uint16_t bpp = u16(b, 28);
        const uint32_t compression = u32(b, 30);
        const uint32_t imageSize = u32(b, 34);
        const uint32_t colours = u32(b, 46);
        if (dibSize != 40 || planes != 1 || bpp != 24 || compression != 0 || colours != 0) {
            MRXS_CORRUPT << what << ": not a BMP24 profile image (DIB header " << dibSize << " bytes, "
                << planes << " planes, " << bpp << " bits per pixel, compression " << compression
                << ", " << colours << " palette colours)";
        }
        if (w != width || h != height) {
            MRXS_CORRUPT << what << ": the BMP is " << w << " x " << h << " (negative height: top-down) where the level says "
                << width << " x " << height;
        }
        const uint64_t stride = 4 * ((uint64_t(width) * 3 + 3) / 4);
        const uint64_t pixels = stride * uint64_t(height);
        if (pixelOffset < 54 || pixelOffset > b.size() || pixels > b.size() - pixelOffset
            || (imageSize != 0 && imageSize != pixels)) {
            MRXS_CORRUPT << what << ": the BMP's pixel data (" << pixels << " bytes at offset " << pixelOffset
                << ") does not fit its " << b.size() << "-byte stream";
        }
    }
}

cv::Mat slideio::mrxs::decodeImage(const std::vector<uint8_t>& bytes, ImageFormat format, int width, int height,
                                   const std::string& what)
{
    const char* actual = signatureOf(bytes);
    const char* declared = format == ImageFormat::Bmp24 ? "BMP" : nameOf(format);
    if (!actual || std::strcmp(actual, declared) != 0) {
        MRXS_CORRUPT << what << ": IMAGE_FORMAT is " << nameOf(format) << " but the payload is "
            << (actual ? actual : "not a recognised image");
    }
    if (int64_t(width) * height * 3 > kMaxDecodedBytes) {
        MRXS_LIMIT << what << ": a " << width << " x " << height << " image exceeds the decoded-image budget of "
            << kMaxDecodedBytes << " bytes";
    }
    if (format == ImageFormat::Bmp24) {
        checkBmp24(bytes, width, height, what);
    }
    // The header alone, before any pixel is allocated.
    const ImageTools::EncodedImageInfo info = ImageTools::probeFreeImageStream(bytes.data(), bytes.size());
    if (info.codec == Compression::Unknown) {
        MRXS_CORRUPT << what << ": the " << actual << " header cannot be read";
    }
    if (info.size != cv::Size(width, height)) {
        MRXS_CORRUPT << what << ": the image is " << info.size.width << " x " << info.size.height
            << " where the level says " << width << " x " << height;
    }
    if (format == ImageFormat::Png && info.hasAlpha) {
        MRXS_UNSUPPORTED << what << ": PNG with transparency (the pyramid is opaque RGB)";
    }

    cv::Mat decoded;
    try {
        if (format == ImageFormat::Jpeg) {
            ImageTools::decodeJpegStream(bytes.data(), bytes.size(), decoded);
        }
        else {
            ImageTools::decodeFreeImageStream(bytes.data(), bytes.size(), decoded);
        }
    }
    catch (const slideio::RuntimeError& e) {
        MRXS_CORRUPT << what << ": " << e.what();
    }
    catch (const cv::Exception& e) {
        MRXS_CORRUPT << what << ": " << e.what();
    }
    if (decoded.depth() != CV_8U || decoded.size() != cv::Size(width, height)) {
        MRXS_CORRUPT << what << ": decoded to " << decoded.cols << " x " << decoded.rows
            << " where the level says " << width << " x " << height;
    }
    if (decoded.channels() == 1) {
        cv::cvtColor(decoded, decoded, cv::COLOR_GRAY2RGB);
    }
    else if (decoded.channels() != 3) {
        MRXS_UNSUPPORTED << what << ": decoded to " << decoded.channels() << " channels; the pyramid is RGB";
    }
    return decoded;
}
