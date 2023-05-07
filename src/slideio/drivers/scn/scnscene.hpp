// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_scnscene_HPP
#define OPENCV_slideio_scnscene_HPP

#include "slideio/drivers/scn/scn_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/core/tools/tilecomposer.hpp"
#include "slideio/drivers/scn/scnstruct.h"
#include "slideio/imagetools/tiffkeeper.hpp"

namespace tinyxml2
{
    class XMLElement;
}

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_SCN_EXPORTS SCNScene : public CVScene, public Tiler
    {
    public:
        /**
         * \brief Constructor
         * \param filePath: path to the slide file
         * \param xmlImage: xml element corresponded to the scene
         */
        SCNScene(const std::string& filePath, const tinyxml2::XMLElement* xmlImage);

        virtual ~SCNScene();

        std::string getFilePath() const override {
            return m_filePath;
        }
        std::string getName() const override {
            return m_name;
        }
        Compression getCompression() const override{
            return m_compression;
        }
        slideio::Resolution getResolution() const override{
            return m_resolution;
        }
        double getMagnification() const override{
            return m_magnification;
        }
        DataType getChannelDataType(int channelIndex) const override{
            return m_channelDataType[channelIndex];
        }
        const std::vector<TiffDirectory>& getChannelDirectories(int channelIndex) const {
            const  int dirIndex = m_interleavedChannels ? 0 : channelIndex;
            return m_channelDirectories[dirIndex];
        }
        cv::Rect getRect() const override;
        int getNumChannels() const override;
        void readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
                                        const std::vector<int>& channelIndices, cv::OutputArray output) override;
        std::string getChannelName(int channel) const override;
        int getTileCount(void* userData) override;
        bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override;
        bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
            void* userData) override;
        void initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output) override;
        const TiffDirectory& findZoomDirectory(int channelIndex, double zoom) const;
    protected:
        void init(const tinyxml2::XMLElement* xmlImage);
        static std::vector<SCNDimensionInfo> parseDimensions(const tinyxml2::XMLElement* xmlPixels);
        void parseChannelNames(const tinyxml2::XMLElement* xmlImage);
        void parseGeometry(const tinyxml2::XMLElement* xmlImage);
        void parseMagnification(const tinyxml2::XMLElement* xmlImage);
        void defineChannelDataType();
        void setupChannels(const tinyxml2::XMLElement* xmlPixels);
        libtiff::TIFF* getFileHandle() {
            return m_tiff;
        }
    protected:
        TIFFKeeper m_tiff;
        std::string m_filePath;
        std::string m_name;
        std::string m_reawMetadata;
        Compression m_compression;
        Resolution m_resolution;
        double m_magnification;
        cv::Rect m_rect;
        int m_numChannels;
        std::vector<std::string> m_channelNames;
        std::vector<DataType> m_channelDataType;
        std::vector<std::vector<TiffDirectory>> m_channelDirectories;
        bool m_interleavedChannels;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
