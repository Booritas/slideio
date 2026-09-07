// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include <cstdio>

#include "ndpitifftools.hpp"
#include "slideio/drivers/ndpi/ndpi_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/tools/tilecomposer.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class NDPIFile;
    class NDPIReadContext;
}

namespace slideio
{
    // What Tiler's methods receive as userData for one call to
    // readResampledLevelBlockChannelsEx: the directory being read (plus, for MCU-striped
    // directories, an open FILE* of its own) together with the context borrowed for the
    // duration of that one call -- the only place a TIFF handle enters the read path.
    // Acquired once by NDPIScene::readResampledLevelBlockChannelsEx and never re-acquired
    // mid-read; see getTileCount/getTileRect/readTile below. Declared here, next to
    // NDPIScene, rather than file-local to ndpiscene.cpp, so a white-box test driving those
    // methods directly can build one that matches the real read path.
    class NDPIUserData
    {
    public:
        NDPIUserData(const NDPITiffDirectory* dir, const std::string& filePath);
        ~NDPIUserData();

        const NDPITiffDirectory* dir() const {
            return m_dir;
        }
        FILE* file() const {
            return m_file;
        }
        const std::string& filePath() const {
            return m_filePath;
        }

        NDPIReadContext* context = nullptr;

    private:
        const NDPITiffDirectory* m_dir;
        FILE* m_file;
        std::string m_filePath;
    };

    class SLIDEIO_NDPI_EXPORTS NDPIScene : public CVScene, public Tiler
    {
        friend class NDPISlide;
    protected:
        NDPIScene();
    public:
        virtual ~NDPIScene();
        void init(const std::string& name, int sceneIndex, const std::string& driverId, NDPIFile* file, int32_t startDirIndex, int32_t endDirIndex);
        bool supportsConcurrentReads() const override { return true; }
        int getNumChannels() const override;
        cv::Rect getRect() const override;
        std::string getFilePath() const override;
		const std::string& getDriverId() const override {
			return m_driverId;
		}
        int getSceneIndex() const override {
			return m_sceneIndex;
        }
        std::string getName() const override {
            return m_sceneName;
        }
        slideio::DataType getChannelDataType(int channel) const override;
        Resolution getResolution() const override;
        double getMagnification() const override;
        Compression getCompression() const override;
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
        void readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
            const cv::Size& blockSize, const std::vector<int>& channelIndices,
            int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
        // The index of the level serving a zoom. NDPIFile::findZoomDirectory performs this
        // same search and then returns the directory; the level path needs the index itself.
        int findZoomLevelIndex(double zoom) const;
        const NDPITiffDirectory& findZoomDirectory(const cv::Rect& imageBlockRect, const cv::Size& requiredBlockSize) const;
        void scaleBlockToDirectory(const cv::Rect& imageBlockRect, const slideio::NDPITiffDirectory& dir, cv::Rect& dirBlockRect) const;
        int getTileCount(void* userData) override;
        bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override;
        bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
                      void* userData) override;
        void initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output) override;
    private:
        void makeSureValidDirectoryType(NDPITiffDirectory::Type directoryType);
    protected:
        NDPIFile* m_pfile;
        int m_startDir;
        int m_endDir;
        std::string m_sceneName;
        cv::Rect m_rect;
        int m_sceneIndex;
		std::string m_driverId;
    };

}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
