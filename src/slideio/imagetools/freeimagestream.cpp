// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// In-memory decoding of JPEG, PNG and BMP streams through FreeImage. FIWrapper reads
// files by path; these read a buffer of known length -- an image embedded in a larger
// container file -- and never look past it.
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/core/exceptions.hpp"

#include <FreeImage.h>
#include <opencv2/imgproc.hpp>
#include <cstring>
#include <limits>

using namespace slideio;

namespace
{
    // Owns a FIMEMORY over the caller's bytes; FreeImage only reads them.
    class MemoryStream
    {
    public:
        MemoryStream(const uint8_t* data, size_t size)
            : m_memory(FreeImage_OpenMemory(const_cast<BYTE*>(data), static_cast<DWORD>(size))) {}
        ~MemoryStream() { if (m_memory) FreeImage_CloseMemory(m_memory); }
        MemoryStream(const MemoryStream&) = delete;
        MemoryStream& operator=(const MemoryStream&) = delete;
        FIMEMORY* get() const { return m_memory; }
    private:
        FIMEMORY* m_memory;
    };

    class Bitmap
    {
    public:
        explicit Bitmap(FIBITMAP* bitmap) : m_bitmap(bitmap) {}
        ~Bitmap() { if (m_bitmap) FreeImage_Unload(m_bitmap); }
        Bitmap(const Bitmap&) = delete;
        Bitmap& operator=(const Bitmap&) = delete;
        FIBITMAP* get() const { return m_bitmap; }
    private:
        FIBITMAP* m_bitmap;
    };

    // FreeImage_OpenMemory takes a DWORD length.
    bool usableBuffer(const uint8_t* data, size_t size)
    {
        return data != nullptr && size > 0 && size <= std::numeric_limits<DWORD>::max();
    }

    Compression toCompression(FREE_IMAGE_FORMAT fif)
    {
        switch (fif) {
        case FIF_JPEG: return Compression::Jpeg;
        case FIF_PNG:  return Compression::Png;
        case FIF_BMP:  return Compression::BMP;
        default:       return Compression::Unknown;
        }
    }

    // The codec the stream's signature names, with the stream rewound for loading.
    FREE_IMAGE_FORMAT detectFormat(FIMEMORY* memory)
    {
        const FREE_IMAGE_FORMAT fif = FreeImage_GetFileTypeFromMemory(memory, 0);
        FreeImage_SeekMemory(memory, 0, SEEK_SET);
        return toCompression(fif) == Compression::Unknown ? FIF_UNKNOWN : fif;
    }
}

ImageTools::EncodedImageInfo ImageTools::probeFreeImageStream(const uint8_t* data, size_t size)
{
    EncodedImageInfo info;
    if (!usableBuffer(data, size)) {
        return info;
    }
    MemoryStream memory(data, size);
    if (!memory.get()) {
        return info;
    }
    const FREE_IMAGE_FORMAT fif = detectFormat(memory.get());
    if (fif == FIF_UNKNOWN || !FreeImage_FIFSupportsNoPixels(fif)) {
        // Without header-only loading a probe would decode the whole image.
        return info;
    }
    Bitmap header(FreeImage_LoadFromMemory(fif, memory.get(), FIF_LOAD_NOPIXELS));
    if (!header.get()) {
        return info;
    }
    info.codec = toCompression(fif);
    info.size = cv::Size(static_cast<int>(FreeImage_GetWidth(header.get())),
                         static_cast<int>(FreeImage_GetHeight(header.get())));
    info.bitsPerPixel = static_cast<int>(FreeImage_GetBPP(header.get()));
    info.hasAlpha = FreeImage_GetColorType(header.get()) == FIC_RGBALPHA
        || FreeImage_IsTransparent(header.get()) != FALSE;
    return info;
}

void ImageTools::decodeFreeImageStream(const uint8_t* data, size_t size, cv::OutputArray output)
{
    if (!usableBuffer(data, size)) {
        RAISE_RUNTIME_ERROR << "decodeFreeImageStream: invalid buffer of " << size << " bytes";
    }
    MemoryStream memory(data, size);
    if (!memory.get()) {
        RAISE_RUNTIME_ERROR << "decodeFreeImageStream: cannot open a memory stream of " << size << " bytes";
    }
    const FREE_IMAGE_FORMAT fif = detectFormat(memory.get());
    if (fif == FIF_UNKNOWN) {
        RAISE_RUNTIME_ERROR << "decodeFreeImageStream: the stream is not a JPEG, PNG or BMP image";
    }
    Bitmap bitmap(FreeImage_LoadFromMemory(fif, memory.get(), fif == FIF_JPEG ? JPEG_ACCURATE : 0));
    if (!bitmap.get() || !FreeImage_HasPixels(bitmap.get())) {
        RAISE_RUNTIME_ERROR << "decodeFreeImageStream: cannot decode the " << toCompression(fif) << " stream";
    }
    FIBITMAP* const bits = bitmap.get();
    if (FreeImage_GetImageType(bits) != FIT_BITMAP) {
        RAISE_RUNTIME_ERROR << "decodeFreeImageStream: unsupported sample type " << FreeImage_GetImageType(bits);
    }
    const unsigned bpp = FreeImage_GetBPP(bits);
    const FREE_IMAGE_COLOR_TYPE colorType = FreeImage_GetColorType(bits);
    int channels = 0;
    if (bpp == 8 && colorType == FIC_MINISBLACK) {
        channels = 1;
    }
    else if (bpp == 24 && colorType == FIC_RGB) {
        channels = 3;
    }
    else if (bpp == 32 && (colorType == FIC_RGBALPHA || colorType == FIC_RGB)) {
        channels = 4;
    }
    else {
        RAISE_RUNTIME_ERROR << "decodeFreeImageStream: unsupported pixel layout: "
            << bpp << " bits per pixel, colour type " << colorType;
    }

    const int width = static_cast<int>(FreeImage_GetWidth(bits));
    const int height = static_cast<int>(FreeImage_GetHeight(bits));
    output.create(height, width, CV_MAKETYPE(CV_8U, channels));
    cv::Mat raster = output.getMat();
    const size_t rowBytes = static_cast<size_t>(width) * channels;
    for (int y = 0; y < height; ++y) {
        // FreeImage keeps rows bottom-up.
        std::memcpy(raster.ptr<uint8_t>(y), FreeImage_GetScanLine(bits, height - 1 - y), rowBytes);
    }
#if FREEIMAGE_COLORORDER == FREEIMAGE_COLORORDER_BGR
    if (channels == 3) {
        cv::cvtColor(raster, raster, cv::COLOR_BGR2RGB);
    }
    else if (channels == 4) {
        cv::cvtColor(raster, raster, cv::COLOR_BGRA2RGBA);
    }
#endif
}
