// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "ndpifile.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/ndpi/ndpilibtiff.hpp"
#include "slideio/core/log.hpp"
#include "slideio/core/exceptions.hpp"


slideio::NDPIFile::~NDPIFile()
{
    // The pool's destructor blocks until every outstanding borrow is returned and then
    // destroys every NDPIReadContext, which closes its keeper -- so there is no handle
    // left to close here explicitly, unlike the old single-handle m_tiff.
    SLIDEIO_LOG(INFO) << "Closing file " << m_filePath;
}

void slideio::NDPIFile::init(const std::string& filePath)
{
    SLIDEIO_LOG(INFO) << "Initialization of NDPI TIFF file : " << filePath;

    Tools::throwIfPathNotExist(filePath, "NDPIFile::init");
    SLIDEIO_LOG(INFO) << "Opening of NDPI TIFF file " << filePath;
    m_filePath = filePath;
    m_contextPool = std::make_unique<ContextPool>([filePath]() {
        return std::make_unique<NDPIReadContext>(filePath);
    });
    {
        // Force one context open right away, matching the old eager
        // m_tiff.openTiffFile() + isValid() check: a bad file is rejected here,
        // before scanFile(), rather than surfacing later at the first concurrent
        // read. NDPIReadContext's constructor throws on failure (see ndpifile.hpp).
        auto borrow = acquireContext();
    }
    SLIDEIO_LOG(INFO) << "File " << filePath << " is successfully opened";
    scanFile();
    for(auto& dir : m_directories) {
        NDPITiffTools::readDirectoryJpegHeaders(this, dir);
    }
    SLIDEIO_LOG(INFO) << "File " << filePath << " initialization is complete";
}

void slideio::NDPIFile::scanFile()
{
    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanFile-begin";
    // Runs during init(), single-threaded, so a local borrow is enough -- no member
    // handle needs to survive past this function.
    auto borrow = acquireContext();
    libtiff::TIFF* tiff = borrow.as<NDPIReadContext>().keeper.getHandle();
    int dirs = libtiff::TIFFNumberOfDirectories(tiff);
    SLIDEIO_LOG(INFO) << "Total number of directories: " << dirs;
    m_directories.resize(dirs);
    for (int dir = 0; dir < dirs; dir++) {
        SLIDEIO_LOG(INFO) << "NDPITiffTools::scanFile processing directory " << dir;
        m_directories[dir].dirIndex = dir;
        NDPITiffTools::scanTiffDir(tiff, dir, 0, m_directories[dir]);
    }
    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanFile-end";
}

namespace
{
    // Above this, the raster is handed back and not kept. A SingleStripe level is
    // usually the coarse end of a pyramid and runs to tens of megabytes -- this
    // file's two are 25MB and 100MB -- but nothing in the format bounds it, and a
    // library holding an unbounded buffer per open file on the strength of one
    // read is worse than the slowness this avoids. Past the cap the behaviour is
    // exactly what it was before: correct, and a decode per read.
    constexpr size_t kMaxCachedStripedRasterBytes = 256ull * 1024ull * 1024ull;
}

cv::Mat slideio::NDPIFile::readStripedDirectory(libtiff::TIFF* tiff, const slideio::NDPITiffDirectory& dir)
{
    // The decode happens under the lock, so two threads reading one directory do not
    // each decode it: the second waits and then finds it cached. That serializes
    // decodes of *different* directories too, which is accepted -- the decode being
    // expensive is the whole reason this function exists, and the alternative is
    // every thread paying for its own copy of the same image.
    std::lock_guard<std::mutex> lock(m_stripedCacheMutex);
    if (m_stripedCacheDirIndex == dir.dirIndex && !m_stripedCacheRaster.empty()) {
        return m_stripedCacheRaster;
    }

    cv::Mat raster;
    m_stripedDecodeCount.fetch_add(1);
    NDPITiffTools::readStripedDir(tiff, dir, raster);

    const size_t bytes = raster.total() * raster.elemSize();
    if (!raster.empty() && bytes <= kMaxCachedStripedRasterBytes) {
        m_stripedCacheRaster = raster;
        m_stripedCacheDirIndex = dir.dirIndex;
    }
    else {
        // Drop whatever was held rather than leaving a stale entry behind a
        // directory index that is no longer the one last read.
        m_stripedCacheRaster = cv::Mat();
        m_stripedCacheDirIndex = -1;
    }
    return raster;
}

const slideio::NDPITiffDirectory& slideio::NDPIFile::findZoomDirectory(double zoom, int sceneWidth, int dirBegin, int dirEnd)
{
    const auto& directories = m_directories;
    const int dirCount = dirEnd - dirBegin;
    const int index = Tools::findZoomLevel(zoom, dirCount, [&directories, sceneWidth, dirBegin](int ind){
        return static_cast<double>(directories[ind+dirBegin].width)/static_cast<double>(sceneWidth);
    });
    return m_directories[index + dirBegin];
}
