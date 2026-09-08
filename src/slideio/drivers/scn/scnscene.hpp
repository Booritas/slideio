// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/scn/scn_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/tools/contextpool.hpp"
#include "slideio/core/tools/readcontext.hpp"
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
    /// One libtiff handle. A fresh handle is cheap here because
    /// TiffTools::setCurrentDirectory positions with TIFFSetSubDirectory(offset),
    /// so it jumps straight to the right IFD with no directory walk and no
    /// re-parse of the pyramid -- a context duplicates the descriptor, not the
    /// parsed model.
    class SCNReadContext : public ReadContext
    {
    public:
        explicit SCNReadContext(const std::string& filePath)
            : keeper(TiffTools::openTiffFile(filePath)) {
            if (!keeper.isValid()) {
                RAISE_RUNTIME_ERROR << "SCNImageDriver: cannot open file " << filePath;
            }
        }
        TIFFKeeper keeper;
    };

    // What Tiler's methods receive as userData for one call to
    // readResampledLevelBlockChannelsEx: the per-channel directory map (immutable, shared
    // across threads) plus the context borrowed for the duration of that one call -- the only
    // place a TIFF handle enters the read path. Acquired once by the caller and never
    // re-acquired mid-read; see SCNScene::acquireContext. Declared here, next to
    // SCNReadContext, rather than file-local to scnscene.cpp, so a white-box test driving
    // getTileCount/getTileRect/readTile directly can build one that matches the real read path.
    struct SCNTileUserData
    {
        SCNTilingInfo info;
        SCNReadContext* context = nullptr;
    };

    class SLIDEIO_SCN_EXPORTS SCNScene : public CVScene, public Tiler
    {
    public:
        /**
         * \brief Constructor
         * \param filePath: path to the slide file
         * \param xmlImage: xml element corresponded to the scene
         */
        SCNScene(const std::string& filePath, int sceneIndex, const std::string& driverId, const tinyxml2::XMLElement* xmlImage);

        virtual ~SCNScene();

        bool supportsConcurrentReads() const override { return true; }

        std::string getFilePath() const override {
            return m_filePath;
        }
        const std::string& getDriverId() const override {
            return m_driverId;
		}
        int getSceneIndex() const override {
            return m_sceneIndex;
		}
        void setSceneIndex(int index) {
            m_sceneIndex = index;
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
        const std::vector<TiffDirectory>& getChannelDirectories(int channelIndex, int zIndex) const {
            const  int dirIndex = zIndex * m_planeCount + (m_interleavedChannels ? 0 : channelIndex);
            return m_channelDirectories[dirIndex];
        }
        cv::Rect getRect() const override;
        int getNumChannels() const override;
        int getNumZSlices() const override {
            return m_numZSlices;
        }
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& channelIndicesIn, int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
        void readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
            const cv::Size& blockSize, const std::vector<int>& channelIndices,
            int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
        std::string getChannelName(int channel) const override;
        int getTileCount(void* userData) override;
        bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override;
        bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
            void* userData) override;
        void initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output) override;
        const TiffDirectory* findZoomDirectory(int channelIndex, int zIndex, double zoom) const;
        static std::vector<SCNDimensionInfo> parseDimensions(const tinyxml2::XMLElement* xmlPixels);
    protected:
        void init(const tinyxml2::XMLElement* xmlImage);
        void parseChannelNames(const tinyxml2::XMLElement* xmlImage);
        void parseGeometry(const tinyxml2::XMLElement* xmlImage);
        void parseMagnification(const tinyxml2::XMLElement* xmlImage);
        void defineChannelDataType();
        void setupChannels(const tinyxml2::XMLElement* xmlPixels, libtiff::TIFF* hFile);
        /// Borrows a handle for the duration of one block read. Acquire once per
        /// read and pass the context down through userData -- never re-acquire
        /// mid-read.
        ContextPool::Borrow acquireContext() { return m_contextPool.acquire(); }
        void createEmptyChannelTile(int tileIndex, int channel, cv::OutputArray output, void* userData);
    protected:
        std::string m_filePath;
        std::string m_driverId;
        std::string m_name;
        Compression m_compression;
        Resolution m_resolution;
        double m_magnification;
        cv::Rect m_rect;
        int m_numChannels;
        int m_numZSlices;
        int m_planeCount;
        std::vector<std::string> m_channelNames;
        std::vector<DataType> m_channelDataType;
        std::vector<std::vector<TiffDirectory>> m_channelDirectories;
        bool m_interleavedChannels;
        int m_sceneIndex;
    private:
        // Declared last on purpose, and it must stay last -- the declaration
        // order is load-bearing, not tidiness. ~ContextPool blocks until every
        // outstanding borrow is returned, and members are destroyed in reverse
        // declaration order, so only a pool declared after the tables above is
        // destroyed *before* the m_channelDirectories that an in-flight readTile
        // walks. SCNScene is the most-derived scene class, so nothing is
        // declared after this. If a subclass is ever added, its own read state
        // needs its own pool (see SVSTiledScene) -- a pool in a base class is
        // destroyed too late to protect derived state.
        ContextPool m_contextPool;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
