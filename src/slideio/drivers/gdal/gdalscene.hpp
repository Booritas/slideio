// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/gdal/gdal_api_def.hpp"
#include "slideio/core/colorprofile.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/slideio_enums.hpp"
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

    // A page FreeImage reads: PNG, JPEG, BMP, GIF, JP2 and the like. Tiffs never
    // come here -- they are GDALTiffScene's. A page has no tiles to offer and is
    // read in full from the bitmap FreeImage decoded when the page was locked.
    class SLIDEIO_GDAL_EXPORTS GDALScene : public slideio::CVScene
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
        // The page's acquisition time -- exif DateTimeOriginal or DateTime -- in
        // seconds since the Unix epoch; 0 where the file states none. No format this
        // driver opens states significant bits or a per-plane time, so those keep
        // the base class defaults; see TECH_DEBT #28.
        int64_t getAcquisitionTime() const override { return m_acquisitionTime; }
        ColorProfile getColorProfile() const override {
            return m_colorProfile;
        }
        void setColorProfile(const ColorProfile& profile) {
            m_colorProfile = profile;
        }
    private:
        void readWholePageBlock(const cv::Rect& rect, const cv::Size& size,
            const std::vector<int>& channelIndices, cv::OutputArray block);
    private:
        // The image owns the page; holding it keeps the page alive for as long as the
        // scene is, which may be longer than the slide that opened it.
        std::shared_ptr<SmallImage> m_image;
        SmallImagePage* m_imagePage;
        int64_t m_acquisitionTime = 0;
        std::string m_filePath;
        std::string m_driverId;
        ColorProfile m_colorProfile;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

