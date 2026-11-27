// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/gdal/gdal_api_def.hpp"
#include "slideio/core/colorprofile.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/slideio_enums.hpp"
#include "slideio/core/tools/tilecomposer.hpp"
#include <opencv2/core.hpp>
#include <memory>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SmallImage;
    class SmallImagePage;
    class SmallTiffWrapper;
    struct TiffDirectory;

    // A Tiler only for a page that is a tiled tiff directory: such a page is read one
    // tile at a time instead of being decoded whole for every block. Every other
    // page -- striped tiffs and everything FreeImage reads -- still has no tiles to
    // offer, and is still read in full.
    class SLIDEIO_GDAL_EXPORTS GDALScene : public slideio::CVScene, public Tiler
    {
    public:
        GDALScene(const std::shared_ptr<SmallImage>& image, int pageIndex, const std::string& filePath,
                  const std::string& driverId);
        virtual ~GDALScene() = default;
        std::string getFilePath() const override;
        int getSceneIndex() const override { return 0; }
        const std::string& getDriverId() const override {
            return m_driverId;
        }
        int getNumChannels() const override;
        slideio::DataType getChannelDataType(int channel) const override;
        slideio::Resolution getResolution() const override;
        double getMagnification() const override;
        std::string getName() const override;
        cv::Rect getRect() const override;
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
        Compression getCompression() const override;
        MetadataFormat getMetadataFormat() const override;
        std::string getRawMetadata() const override;
        // The page's acquisition time -- exif DateTimeOriginal or DateTime for a
        // freeimage page, TIFFTAG_DATETIME for a tiff one -- in seconds since the
        // Unix epoch; 0 where the file states none. No format this driver opens
        // states significant bits or a per-plane time, so those keep the base
        // class defaults; see TECH_DEBT #28.
        int64_t getAcquisitionTime() const override { return m_acquisitionTime; }
        ColorProfile getColorProfile() const override {
            return m_colorProfile;
        }
        void setColorProfile(const ColorProfile& profile) {
            m_colorProfile = profile;
        }
        // Tiler methods. Reached only through readResampledBlockChannelsEx, and only
        // when the page is a tiled tiff directory.
        int getTileCount(void* userData) override;
        bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override;
        bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
            void* userData) override;
        void initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices,
            cv::OutputArray output) override;
    private:
        void readWholePageBlock(const cv::Rect& rect, const cv::Size& size,
            const std::vector<int>& channelIndices, cv::OutputArray block);
    private:
        // The image owns the page; holding it keeps the page alive for as long as the
        // scene is, which may be longer than the slide that opened it.
        std::shared_ptr<SmallImage> m_image;
        SmallImagePage* m_imagePage;
        // Both non-null only for a tiled tiff page. The wrapper is owned by m_image and
        // the directory by the wrapper, so they live exactly as long as the scene does.
        // The wrapper's one libtiff handle is shared by every page of the file; this
        // scene keeps the base class's serialised reads, like the whole-page path.
        SmallTiffWrapper* m_tiff = nullptr;
        const TiffDirectory* m_tiledDirectory = nullptr;
        int64_t m_acquisitionTime = 0;
        std::string m_filePath;
        std::string m_driverId;
        ColorProfile m_colorProfile;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

