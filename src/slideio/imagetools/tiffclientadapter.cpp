// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/tiffclientadapter.hpp"
#include "slideio/base/exceptions.hpp"

#include <cstdio>
#include <cstring>

namespace slideio
{

namespace {

struct TiffClientCtx {
    std::shared_ptr<RandomAccessStream> stream;
    uint64_t cursor = 0;
};

libtiff::tmsize_t tiffRead(libtiff::thandle_t h, void* buf, libtiff::tmsize_t n) {
    auto* ctx = static_cast<TiffClientCtx*>(h);
    const size_t got = ctx->stream->read(ctx->cursor, static_cast<size_t>(n), buf);
    ctx->cursor += got;
    return static_cast<libtiff::tmsize_t>(got);
}

libtiff::tmsize_t tiffWrite(libtiff::thandle_t, void*, libtiff::tmsize_t) {
    return -1; // read-only
}

libtiff::toff_t tiffSeek(libtiff::thandle_t h, libtiff::toff_t off, int whence) {
    auto* ctx = static_cast<TiffClientCtx*>(h);
    switch (whence) {
        case SEEK_SET: ctx->cursor = static_cast<uint64_t>(off); break;
        case SEEK_CUR: ctx->cursor += off; break;
        case SEEK_END: ctx->cursor = ctx->stream->size() + off; break;
        default: break;
    }
    return static_cast<libtiff::toff_t>(ctx->cursor);
}

int tiffClose(libtiff::thandle_t h) {
    delete static_cast<TiffClientCtx*>(h);
    return 0;
}

libtiff::toff_t tiffSize(libtiff::thandle_t h) {
    return static_cast<libtiff::toff_t>(static_cast<TiffClientCtx*>(h)->stream->size());
}

// No memory mapping for stream-backed TIFFs.
int tiffMap(libtiff::thandle_t, void**, libtiff::toff_t*) { return 0; }
void tiffUnmap(libtiff::thandle_t, void*, libtiff::toff_t) {}

} // namespace

libtiff::TIFF* openTiffFromStream(std::shared_ptr<RandomAccessStream> stream)
{
    if (!stream) {
        RAISE_RUNTIME_ERROR << "openTiffFromStream: null stream";
    }
    auto* ctx = new TiffClientCtx{ std::move(stream), 0 };
    libtiff::TIFF* t = libtiff::TIFFClientOpen(
        ctx->stream->uri().c_str(), "r",
        static_cast<libtiff::thandle_t>(ctx),
        tiffRead, tiffWrite, tiffSeek, tiffClose, tiffSize, tiffMap, tiffUnmap);
    if (!t) {
        delete ctx;
        RAISE_RUNTIME_ERROR << "openTiffFromStream: TIFFClientOpen failed";
    }
    return t;
}

} // namespace slideio
