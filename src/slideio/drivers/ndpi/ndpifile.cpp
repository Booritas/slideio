// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "ndpifile.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/ndpi/ndpilibtiff.hpp"
#include "slideio/core/log.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/drivers/ndpi/ndpistripedcache.hpp"


slideio::NDPIFile::~NDPIFile()
{
    // The pool's destructor blocks until every outstanding borrow is returned and then
    // destroys every NDPIReadContext, which closes its keeper -- so there is no handle
    // left to close here explicitly, unlike the old single-handle m_tiff.
    //
    // Destroyed here rather than left to member destruction so that it happens before
    // releaseFile below: every striped decode runs inside a read holding a borrow, so
    // once the pool is gone no decode of this file can still add a cache entry.
    m_contextPool.reset();
    NDPIStripedRasterCache::instance().releaseFile(m_stripedCacheId);
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

cv::Mat slideio::NDPIFile::readStripedDirectory(libtiff::TIFF* tiff, const slideio::NDPITiffDirectory& dir)
{
    // Keyed by (dirIndex, offset): a SubIFD shares its parent's dirIndex and differs
    // from it only by offset.
    return NDPIStripedRasterCache::instance().get(m_stripedCacheId, dir.dirIndex,
        static_cast<int64_t>(dir.offset), [this, tiff, &dir]() {
            cv::Mat raster;
            NDPITiffTools::readStripedDir(tiff, dir, raster);
            m_stripedDecodeCount.fetch_add(1);
            return raster;
        });
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
