// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/ome-tiff/ot_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/drivers/ome-tiff/otscene.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/core/tools/tilecomposer.hpp"
#include "slideio/drivers/ome-tiff/otstructs.hpp"
#include "slideio/drivers/ome-tiff/tiffdata.hpp"
#include "slideio/imagetools/tifffiles.hpp"
#include "slideio/drivers/ome-tiff/otdimensions.hpp"
#include <tinyxml2.h>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    namespace ometiff
    {
        struct ImageData;

        class SLIDEIO_OMETIFF_EXPORTS OTScene : public CVScene, public Tiler
        {
        public:
            explicit OTScene(const ImageData& filePath);
            int getNumChannels() const override;
            cv::Rect getRect() const override;
            int findZoomLevel(double zoom) const;
            // Tiler methods
            int getTileCount(void* userData) override;
            bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override;
            bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
                          void* userData) override;
            void initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output) override;
            std::string getChannelName(int channel) const override;
            void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex,
                cv::OutputArray output) override;
            std::string getFilePath() const override;
            std::string getName() const override;
            slideio::DataType getChannelDataType(int channel) const override;
            Resolution getResolution() const override;
            double getMagnification() const override;
            Compression getCompression() const override;
            int getNumZSlices() const override;
            int getNumTFrames() const override;
			int getNumTiffFiles() const { return m_files.getNumberOfOpenFiles(); }
			int getNumTiffDataItems() const { return static_cast<int>(m_tiffData.size()); }
            const TiffData& getTiffData(int index) const { return m_tiffData[index]; }
        private:
            void extractImagePyramids();
            void initialize();
            void initializeChannelNames(tinyxml2::XMLElement* pixels);
            void extractMagnificationFromMetadata();
            void extractTiffData(tinyxml2::XMLElement* pixels);
            void extractImageIndex();
            LevelInfo extractLevelInfo(const TiffDirectory& dir, int index) const;
            void collectTiffDataIndices(std::vector<int> channelIndices, int zSliceIndex, int tFrameIndex,
                std::vector<int>& tiffDataIndices) const;
        private:
            int m_numChannels = 0;
            std::vector<std::string> m_channelNames;
            tinyxml2::XMLElement* m_imageXml;
            std::shared_ptr<tinyxml2::XMLDocument> m_imageDoc;
            std::string m_imageId;
			std::vector<TiffData> m_tiffData;
            std::string m_dimensionOrder;
            DataType m_dataType = DataType::DT_Unknown;
            int m_numZSlices = 0;
            int m_numTFrames = 0;
			cv::Size m_imageSize;
            bool m_bigEndian = false;
            std::string m_imageName;
            std::string m_filePath;
            Compression m_compression = Compression::Unknown;
            Resolution m_resolution = {};
            double m_magnification = 0;
            int m_imageIndex = -1;
            TIFFFiles m_files;
        };
    }
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
